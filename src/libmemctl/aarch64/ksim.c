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

/*
 * ksim_cstring
 *
 * Description:
 * 	Return the address of the first string matching the reference string in the specified
 * 	kernel extension's __TEXT.__cstring section.
 */
static kaddr_t
ksim_cstring(const struct macho *kext, const char *reference) {
	struct mapped_region cstring;
	const struct load_command *sc = macho_find_segment(kext, "__TEXT");
	assert(sc != NULL);
	const void *sect = macho_find_section(kext, sc, "__cstring");
	assert(sect != NULL);
	macho_section_data(kext, sc, sect, &cstring.data, &cstring.addr, &cstring.size);
	size_t len = strlen(reference);
	const void *match = memmem(cstring.data, cstring.size, reference, len);
	if (match == NULL) {
		return 0;
	}
	return mapped_region_address(&cstring, match);
}

/*
 * sim_clear_regs
 *
 * Description:
 * 	Clear all registers except PC.
 */
static void
sim_clear_regs(struct aarch64_sim *sim) {
	for (size_t n = 0; n < AARCH64_SIM_GPREGS; n++) {
		aarch64_sim_word_clear(sim, &sim->X[n]);
	}
	aarch64_sim_word_clear(sim, &sim->SP);
	aarch64_sim_pstate_clear(sim, &sim->PSTATE);
}

/*
 * sim_clear_temps
 *
 * Description:
 * 	Clear the temporary registers, simulating a function call.
 */
static void
sim_clear_temps(struct aarch64_sim *sim) {
	for (size_t n = TEMPREGS_START; n <= TEMPREGS_END; n++) {
		aarch64_sim_word_clear(sim, &sim->X[n]);
	}
	aarch64_sim_pstate_clear(sim, &sim->PSTATE);
}

/*
 *
 * sim_get_instruction
 *
 * Description:
 * 	Grab the current instruction from the instruction stream and store it in the simulator.
 */
static bool
sim_get_instruction(struct aarch64_sim *sim, const struct mapped_region *code) {
	if (taint_unknown(sim->PC.taint)) {
		return false;
	}
	uint64_t pc = sim->PC.value;
	if (!mapped_region_contains(code, pc, AARCH64_INSTRUCTION_SIZE)) {
		return false;
	}
	const uint32_t *ins = mapped_region_get(code, pc, NULL);
	sim->instruction.value = *ins;
	return true;
}

/*
 * find_reference_instruction_fetch
 *
 * Description:
 * 	The aarch64_sim callback to fetch the next instruction.
 */
static bool
find_reference_instruction_fetch(struct aarch64_sim *sim) {
	const struct mapped_region *code = sim->context;
	return sim_get_instruction(sim, code);
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
 * find_reference_branch_hit
 *
 * Description:
 * 	The aarch64_sim callback informing us of a branch instruction that was hit.
 */
static bool
find_reference_branch_hit(struct aarch64_sim *sim, enum aarch64_sim_branch_type type,
		const struct aarch64_sim_word *branch, const struct aarch64_sim_word *condition,
		bool *take_branch, bool *keep_running) {
	*take_branch = false;
	if (type == AARCH64_SIM_BRANCH_TYPE_BRANCH || type == AARCH64_SIM_BRANCH_TYPE_RETURN) {
		// For an unconditional branch, we have no idea what jumps to right after it, so
		// clear all registers.
		sim_clear_regs(sim);
	} else if (type == AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK) {
		// For a branch with link, assume it's a function call and clear only the temporary
		// registers.
		sim_clear_temps(sim);
	}
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
	sim_clear_regs(sim);
	return true;
}

/*
 * sim_set_pc
 *
 * Description:
 * 	Initialize the AArch64 simulator's PC register and set the taint appropriately.
 */
static void
sim_set_pc(struct aarch64_sim *sim, kaddr_t pc) {
	sim->PC.value = pc;
	sim->PC.taint = AARCH64_SIM_TAINT_TOP;
	sim->instruction.taint = AARCH64_SIM_TAINT_TOP;
}

/*
 * ksim_exec_find_reference
 *
 * Description:
 * 	Run the simulator starting from the specified address linearly, looking for the first
 * 	instruction that creates a reference to the specified value in a register.
 */
static kaddr_t
ksim_exec_find_reference(struct macho *kext, kword_t value) {
	struct mapped_region code;
	const struct load_command *sc = macho_find_segment(kext, "__TEXT_EXEC");
	assert(sc != NULL);
	macho_segment_data(kext, sc, &code.data, &code.addr, &code.size);
	// Initialize the simulator.
	struct aarch64_sim sim = { &code };
	sim.instruction_fetch   = find_reference_instruction_fetch;
	sim.memory_load         = ksim_memory_load;
	sim.memory_store        = ksim_memory_store;
	sim.branch_hit          = find_reference_branch_hit;
	sim.illegal_instruction = ksim_illegal_instruction;
	sim.taint_default       = ksim_taints;
	aarch64_sim_clear(&sim);
	sim_set_pc(&sim, code.addr);
	// Single step until we either fail or find our instruction.
	for (;;) {
		if (!aarch64_sim_step(&sim)) {
			return 0;
		}
		// TODO: It would be nice if aarch64_sim could tell us which registers to look at.
		for (unsigned n = 0; n < AARCH64_SIM_GPREGS; n++) {
			if (!taint_unknown(sim.X[n].taint) && sim.X[n].value == value) {
				// Found it! The PC has already advanced, so subtract 4.
				return sim.PC.value - AARCH64_INSTRUCTION_SIZE;
			}
		}
	}
}

kaddr_t
ksim_string_reference(const char *kext_id, const char *reference) {
	if (kext_id == NULL) {
		kext_id = KERNEL_ID;
	}
	// Open the Mach-O file for the bundle ID.
	struct macho kext;
	kext_result kr = kernelcache_kext_init_macho(&kernelcache, &kext, kext_id);
	if (kr != KEXT_SUCCESS) {
		assert(kr == KEXT_NO_KEXT);
		return 0;
	}
	// Find the address of the string in the __TEXT.__cstring section.
	kaddr_t cstring = ksim_cstring(&kext, reference);
	if (cstring == 0) {
		return 0;
	}
	// Do the search.
	return ksim_exec_find_reference(&kext, cstring);
}

void
ksim_clearregs(struct ksim *ksim) {
	sim_clear_regs(&ksim->sim);
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
	macho_segment_data(&kext, sc, &ksim->code.data, &ksim->code.addr, &ksim->code.size);
	sim_set_pc(&ksim->sim, pc);
}

bool
ksim_scan_for(struct ksim *ksim, int direction, uint32_t ins, uint32_t mask, unsigned index,
		kaddr_t *pc, unsigned count) {
	size_t step = (direction < 0 ? -1 : 1) * AARCH64_INSTRUCTION_SIZE;
	ins &= mask;
	for (unsigned i = 0; i < count; i++) {
		// Advance. We don't use the aarch64_sim API because that one only moves forward.
		ksim->sim.PC.value += step;
		if (!sim_get_instruction(&ksim->sim, &ksim->code)) {
			// Undo that last step before returning.
			ksim->sim.PC.value -= step;
			break;
		}
		// If we have a match, decrement the index or succeed.
		if ((ksim->sim.instruction.value & mask) == ins) {
			if (index == 0) {
				if (pc != NULL) {
					*pc = ksim->sim.PC.value;
				}
				return true;
			}
			index--;
		}
	}
	return false;
}

bool
ksim_scan_for_jump(struct ksim *ksim, int direction, unsigned index, kaddr_t *pc, kaddr_t *target,
		unsigned count) {
	bool found = ksim_scan_for(ksim, direction, AARCH64_B_INS_BITS, AARCH64_B_INS_MASK, index,
			pc, count);
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
ksim_scan_for_call(struct ksim *ksim, int direction, unsigned index, kaddr_t *pc, kaddr_t *target,
		unsigned count) {
	bool found = ksim_scan_for(ksim, direction, AARCH64_BL_INS_BITS, AARCH64_BL_INS_MASK,
			index, pc, count);
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
 * ksim_exec_instruction_fetch
 *
 * Description:
 * 	The aarch64_sim callback to fetch the next instruction.
 */
static bool
ksim_exec_instruction_fetch(struct aarch64_sim *sim) {
	struct ksim_exec_context *context = sim->context;
	struct ksim *ksim = context->ksim;
	// Limit the total number of instructions we execute.
	if (context->instructions_left == 0) {
		return false;
	}
	// Get the next instruction.
	if (!sim_get_instruction(sim, &ksim->code)) {
		return false;
	}
	// Clear temporary registers if we are resuming after a function call.
	if (ksim->clear_temporaries) {
		sim_clear_temps(sim);
		ksim->clear_temporaries = false;
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
 * ksim_exec_branch_hit
 *
 * Description:
 * 	The aarch64_sim callback informing us of a branch instruction that was hit.
 */
static bool
ksim_exec_branch_hit(struct aarch64_sim *sim, enum aarch64_sim_branch_type type,
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

void
ksim_init_sim(struct ksim *ksim, kaddr_t pc) {
	// Initialize the aarch64_sim client state.
	ksim->sim.instruction_fetch   = ksim_exec_instruction_fetch;
	ksim->sim.memory_load         = ksim_memory_load;
	ksim->sim.memory_store        = ksim_memory_store;
	ksim->sim.branch_hit          = ksim_exec_branch_hit;
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
