#include "memctl/aarch64/ksim.h"

#include "memctl/kernelcache.h"
#include "memctl/macho.h"
#include "memctl/utility.h"


// AArch64 temporary registers.
#define TEMPREGS_START 0
#define TEMPREGS_END   17

// A strong taint denoting that the value is unknown.
#define TAINT_BIT_UNKNOWN 0x1

// A ksim instance will execute a maximum of 0x10000 instructions by default.
#define KSIM_MAX_INSTRUCTIONS 0x10000

/*
 * ksim_taints
 *
 * Description:
 * 	The taint_default table for ksim.
 */
static aarch64_sim_taint ksim_taints[] = {
	{ .t_and = 0, .t_or = 0                 }, // AARCH64_SIM_TAINT_CONSTANT
	{ .t_and = 0, .t_or = TAINT_BIT_UNKNOWN }, // AARCH64_SIM_TAINT_UNKNOWN
};

/*
 * taint_unknown
 *
 * Description:
 * 	Returns true if the taint indicates that the corresponding value is unknown.
 */
static bool
taint_unknown(aarch64_sim_taint taint) {
	return (taint.t_or & TAINT_BIT_UNKNOWN) != 0;
}

kaddr_t
ksim_symbol(const char *kext, const char *symbol) {
	kaddr_t address;
	if (kext == NULL) {
		kext = KERNEL_ID;
	}
	kext_result kr = kext_id_find_symbol(kext, symbol, &address, NULL);
	return (kr == KEXT_SUCCESS ? address : 0);
}

void
ksim_clearregs(struct ksim *ksim) {
	for (size_t n = 0; n < AARCH64_SIM_GPREGS; n++) {
		aarch64_sim_word_clear(&ksim->sim, &ksim->sim.X[n]);
	}
	aarch64_sim_word_clear(&ksim->sim, &ksim->sim.SP);
	aarch64_sim_pstate_clear(&ksim->sim, &ksim->sim.PSTATE);
}

/*
 * ksim_cleartemps
 *
 * Description:
 * 	Clear the temporary registers, simulating a function call.
 */
static void
ksim_cleartemps(struct ksim *ksim) {
	for (size_t n = TEMPREGS_START; n <= TEMPREGS_END; n++) {
		aarch64_sim_word_clear(&ksim->sim, &ksim->sim.X[n]);
	}
	aarch64_sim_pstate_clear(&ksim->sim, &ksim->sim.PSTATE);
}

/*
 * find_code_segment
 *
 * Description:
 * 	Find the load command for the code segment containing the given address.
 */
static const struct load_command *
find_code_segment(const struct macho *macho, uint64_t pc) {
	const struct load_command *lc = NULL;
	assert(macho_is_64(macho));
	for (;;) {
		lc = macho_next_segment(macho, lc);
		if (lc == NULL) {
			return NULL;
		}
		assert(lc->cmd == LC_SEGMENT_64);
		const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
		const int prot = VM_PROT_READ | VM_PROT_EXECUTE;
		if ((sc->initprot & prot) != prot) {
			continue;
		}
		if (pc < sc->vmaddr || sc->vmaddr + sc->vmsize <= pc) {
			continue;
		}
		return lc;
	}
}

void
ksim_set_pc(struct ksim *ksim, kaddr_t pc) {
	struct macho kext;
	kext_result kr = kernelcache_find_containing_address(&kernelcache, pc, NULL, NULL, &kext);
	assert(kr == KEXT_SUCCESS);
	const struct load_command *sc = find_code_segment(&kext, pc);
	assert(sc != NULL);
	// The data will be valid until the kernelcache is de-initialized.
	macho_segment_data(&kext, sc, &ksim->code_data, &ksim->code_address, &ksim->code_size);
	ksim->sim.PC.value = pc;
	ksim->sim.PC.taint = AARCH64_SIM_TAINT_TOP;
	ksim->sim.instruction.taint = ksim->sim.PC.taint;
}

/*
 *
 * ksim_get_instruction
 *
 * Description:
 * 	Grab the current instruction from the instruction stream and store it in the simulator.
 */
static bool
ksim_get_instruction(struct ksim *ksim) {
	if (taint_unknown(ksim->sim.PC.taint)) {
		return false;
	}
	uint64_t pc = ksim->sim.PC.value;
	if (pc < ksim->code_address
			|| ksim->code_address + ksim->code_size < pc + AARCH64_INSTRUCTION_SIZE) {
		return false;
	}
	uint32_t ins = *(uint32_t *)((uintptr_t)ksim->code_data + (pc - ksim->code_address));
	ksim->sim.instruction.value = ins;
	return true;
}

bool
ksim_scan_for(struct ksim *ksim, int direction, uint32_t ins, uint32_t mask, unsigned index,
		unsigned count) {
	size_t step = (direction < 0 ? -1 : 1) * AARCH64_INSTRUCTION_SIZE;
	ins &= mask;
	bool found = false;
	for (unsigned i = 0; i < count; i++) {
		// Advance. We don't use the aarch64_sim API because that one only moves forward.
		ksim->sim.PC.value += step;
		if (!ksim_get_instruction(ksim)) {
			// Undo that last step before returning.
			ksim->sim.PC.value -= step;
			break;
		}
		// If we have a match, decrement the index or succeed.
		if ((ksim->sim.instruction.value & mask) == ins) {
			if (index == 0) {
				found = true;
				break;
			} else {
				index--;
			}
		}
	}
	return found;
}

bool
ksim_scan_for_jump(struct ksim *ksim, int direction, unsigned index, kaddr_t *target,
		unsigned count) {
	bool found = ksim_scan_for(ksim, direction, AARCH64_B_INS_BITS, AARCH64_B_INS_MASK, index,
			count);
	if (!found) {
		return false;
	}
	struct aarch64_ins_b b;
	bool success = aarch64_decode_b(ksim->sim.instruction.value, ksim->sim.PC.value, &b);
	assert(success);
	*target = b.label;
	return true;
}

bool
ksim_scan_for_call(struct ksim *ksim, int direction, unsigned index, kaddr_t *target,
		unsigned count) {
	bool found = ksim_scan_for(ksim, direction, AARCH64_BL_INS_BITS, AARCH64_BL_INS_MASK,
			index, count);
	if (!found) {
		return false;
	}
	struct aarch64_ins_b b;
	bool success = aarch64_decode_b(ksim->sim.instruction.value, ksim->sim.PC.value, &b);
	assert(success);
	*target = b.label;
	return true;
}

/*
 * gpreg_word
 *
 * Description:
 * 	Retrieve the aarch64_sim_word for the given general-purpose register.
 */
static struct aarch64_sim_word *
gpreg_word(struct ksim *ksim, aarch64_gpreg reg) {
	unsigned n = AARCH64_GPREGID(reg);
	if (n == AARCH64_SIM_GPREGS) {
		return &ksim->sim.SP;
	}
	return &ksim->sim.X[n];
}

void
ksim_setreg(struct ksim *ksim, aarch64_gpreg reg, kword_t value) {
	struct aarch64_sim_word *word = gpreg_word(ksim, reg);
	uint64_t mask = ones(AARCH64_GPREGSIZE(reg));
	word->value = value & mask;
	word->taint = AARCH64_SIM_TAINT_TOP;
}

bool
ksim_getreg(struct ksim *ksim, aarch64_gpreg reg, kword_t *value) {
	struct aarch64_sim_word *word = gpreg_word(ksim, reg);
	uint64_t mask = ones(AARCH64_GPREGSIZE(reg));
	if (taint_unknown(word->taint)) {
		return false;
	}
	*value = word->value & mask;
	return true;
}

/*
 * struct ksim_exec_context
 *
 * Description:
 * 	aarch64_sim context for ksim_exec_until.
 */
struct ksim_exec_context {
	struct ksim *            ksim;
	ksim_exec_until_callback until;
	void *                   until_context;
	ksim_branch *            branches;
	unsigned                 instructions_left;
	bool                     found;
};

/*
 * ksim_instruction_fetch
 *
 * Description:
 * 	The aarch64_sim callback to fetch the next instruction.
 */
static bool
ksim_instruction_fetch(struct aarch64_sim *sim) {
	struct ksim_exec_context *context = sim->context;
	struct ksim *ksim = context->ksim;
	// Abort if the PC's taint is unknown.
	if (taint_unknown(sim->PC.taint)) {
		return false;
	}
	// Limit the total number of instructions we execute.
	if (context->instructions_left == 0) {
		return false;
	}
	// Clear temporary registers if we are resuming after a function call.
	if (ksim->clear_temporaries) {
		ksim_cleartemps(ksim);
		ksim->clear_temporaries = false;
	}
	// Get the next instruction.
	if (!ksim_get_instruction(ksim)) {
		return false;
	}
	// Check if the client wants to stop before this instruction. If so, set the did_stop field
	// so that we know not to stop next time.
	if (context->until != NULL && !ksim->did_stop) {
		bool found = context->until(context->until_context, ksim, sim->PC.value,
				sim->instruction.value);
		if (found) {
			ksim->did_stop = true;
			context->found = true;
			return false;
		}
	}
	// Set the new instruction but keep the taint the same.
	ksim->did_stop = false;
	context->instructions_left--;
	return true;
}

/*
 * ksim_memory_load
 *
 * Description:
 * 	The aarch64_sim callback to load a value from memory. Set the loaded value to unknown.
 */
static bool
ksim_memory_load(struct aarch64_sim *sim, struct aarch64_sim_word *value,
		const struct aarch64_sim_word *address, size_t size) {
	aarch64_sim_word_clear(sim, value);
	return true;
}

/*
 * ksim_memory_store
 *
 * Description:
 * 	The aarch64_sim callback to store a value to memory. Do nothing.
 */
static bool
ksim_memory_store(struct aarch64_sim *sim,
		const struct aarch64_sim_word *value, const struct aarch64_sim_word *address,
		size_t size) {
	return true;
}

/*
 * ksim_branch_hit
 *
 * Description:
 * 	The aarch64_sim callback informing us of a branch instruction that was hit.
 */
static bool
ksim_branch_hit(struct aarch64_sim *sim, enum aarch64_sim_branch_type type,
		const struct aarch64_sim_word *branch, const struct aarch64_sim_word *condition,
		bool *take_branch, bool *keep_running) {
	struct ksim_exec_context *context = sim->context;
	struct ksim *ksim = context->ksim;
	bool advance_branches = false;
	// If it's a conditional branch, refer to the branches descriptions.
	if (type == AARCH64_SIM_BRANCH_TYPE_CONDITIONAL) {
		// Explicitly check for true because KSIM_BRANCH_ALL_FALSE is also nonzero.
		ksim_branch branch_behavior = context->branches[0];
		if (branch_behavior == false || branch_behavior == true) {
			*take_branch = branch_behavior;
			advance_branches = true;
		} else {
			assert(branch_behavior == KSIM_BRANCH_ALL_FALSE);
			*take_branch = false;
		}
	}
	// If it's a branch with link, pretend it's a function call: don't take the branch but mark
	// the temporary registers as needing to be cleared.
	else if (type == AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK) {
		ksim->clear_temporaries = true;
		*take_branch = false;
	}
	// Otherwise, if it's an unconditional branch without link, we must take it. This is
	// default behavior.
	// Abort if the address we're about to branch to is unknown.
	if (*take_branch && taint_unknown(branch->taint)) {
		return false;
	}
	// Update our branches list.
	if (advance_branches) {
		context->branches++;
	}
	// Continue running the simulator.
	return true;
}

/*
 * ksim_illegal_instruction
 *
 * Description:
 * 	The aarch64_sim callback for an illegal instruction. Since the simulator is currently
 * 	incomplete, we conservatively clear any simulator state that is likely to be invalidated by
 * 	an unknown instruction.
 */
static bool
ksim_illegal_instruction(struct aarch64_sim *sim) {
	struct ksim *ksim = ((struct ksim_exec_context *)sim->context)->ksim;
	ksim_clearregs(ksim);
	return true;
}

void
ksim_init_sim(struct ksim *ksim, kaddr_t pc) {
	// Initialize the aarch64_sim client state.
	ksim->sim.instruction_fetch   = ksim_instruction_fetch;
	ksim->sim.memory_load         = ksim_memory_load;
	ksim->sim.memory_store        = ksim_memory_store;
	ksim->sim.branch_hit          = ksim_branch_hit;
	ksim->sim.illegal_instruction = ksim_illegal_instruction;
	ksim->sim.taint_default       = ksim_taints;
	// Clear all the registers.
	aarch64_sim_clear(&ksim->sim);
	// Set PC.
	if (pc != 0) {
		ksim_set_pc(ksim, pc);
	}
	// Set up internal state.
	ksim->clear_temporaries = false;
	ksim->did_stop          = false;
}

bool
ksim_exec_until(struct ksim *ksim, ksim_exec_until_callback until, void *context,
		ksim_branch *branches, unsigned count) {
	ksim_branch dummy_branch = KSIM_BRANCH_ALL_FALSE;
	if (branches == NULL) {
		branches = &dummy_branch;
	}
	struct ksim_exec_context exec_context = { ksim, until, context, branches, count };
	ksim->sim.context = &exec_context;
	for (;;) {
		if (!aarch64_sim_step(&ksim->sim)) {
			return exec_context.found;
		}
	}
}

/*
 * ksim_exec_until_call_callback
 *
 * Description:
 * 	A ksim_exec_until_callback that stops just before executing a BL instruction.
 */
static bool
ksim_exec_until_call_callback(void *context, struct ksim *ksim, kaddr_t pc, uint32_t ins) {
	if (!AARCH64_INS_TYPE(ins, BL_INS)) {
		return false;
	}
	if (context != NULL) {
		kaddr_t *target = context;
		struct aarch64_ins_b b;
		aarch64_decode_b(ins, pc, &b);
		*target = b.label;
	}
	return true;
}

bool
ksim_exec_until_call(struct ksim *ksim, ksim_branch *branches, kaddr_t *target, unsigned count) {
	return ksim_exec_until(ksim, ksim_exec_until_call_callback, target, branches, count);
}

/*
 * ksim_exec_until_return_callback
 *
 * Description:
 * 	A ksim_exec_until_callback that stops just before executing a RET instruction.
 */
static bool
ksim_exec_until_return_callback(void *context, struct ksim *ksim, kaddr_t pc, uint32_t ins) {
	return AARCH64_INS_TYPE(ins, RET_INS);
}

bool
ksim_exec_until_return(struct ksim *ksim, ksim_branch *branches, unsigned count) {
	return ksim_exec_until(ksim, ksim_exec_until_return_callback, NULL, branches, count);
}

/*
 * struct ksim_exec_until_store_context
 *
 * Description:
 * 	Callback context for ksim_exec_until_store.
 */
struct ksim_exec_until_store_context {
	aarch64_gpreg base;
	kword_t *     value;
};

/*
 * ksim_exec_until_store_callback
 *
 * Description:
 * 	A ksim_exec_until_callback that stops just before executing a STR, STRB, or STRH
 * 	instruction.
 */
static bool
ksim_exec_until_store_callback(void *context, struct ksim *ksim, kaddr_t pc, uint32_t ins) {
	struct ksim_exec_until_store_context *c = context;
	struct aarch64_ins_ldr_im im;
	struct aarch64_ins_ldr_r  r;
	aarch64_gpreg value_reg;
	size_t        value_size;
	if (aarch64_decode_ldr_ui(ins, &im) || aarch64_decode_ldr_ix(ins, &im)) {
		if (!im.load && im.Xn == c->base) {
			value_reg  = im.Rt;
			value_size = im.size;
			goto found;
		}
	} else if (aarch64_decode_ldr_r(ins, &r)) {
		if (!r.load && r.Xn == c->base) {
			value_reg  = r.Rt;
			value_size = r.size;
			goto found;
		}
	}
	return false;
found:
	if (c->value != NULL) {
		kword_t value;
		if (ksim_getreg(ksim, value_reg, &value)) {
			value_size = 8 << value_size;
			*c->value = value & ones(value_size);
		}
	}
	return true;
}

bool
ksim_exec_until_store(struct ksim *ksim, ksim_branch *branches, aarch64_gpreg base, kword_t *value,
		unsigned count) {
	struct ksim_exec_until_store_context context = { base, value };
	return ksim_exec_until(ksim, ksim_exec_until_store_callback, &context, branches, count);
}
