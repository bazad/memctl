#include "memctl/aarch64/ksim.h"

#include "memctl/macho.h"
#include "memctl/utility.h"


// AArch64 temporary registers.
#define TEMPREGS_START 0
#define TEMPREGS_END   17

// A strong taint denoting that the value is unknown.
#define TAINT_UNKNOWN 0x1

// A ksim instance will execute a maximum of 0x10000 instructions by default.
#define KSIM_MAX_INSTRUCTIONS 0x10000

/*
 * ksim_taints
 *
 * Description:
 * 	The taint_default table for ksim.
 */
static aarch64_sim_taint ksim_taints[] = {
	{ .t_and = 0, .t_or = 0             }, // AARCH64_SIM_TAINT_CONSTANT
	{ .t_and = 0, .t_or = TAINT_UNKNOWN }, // AARCH64_SIM_TAINT_UNKNOWN
};

/*
 * taint_unknown
 *
 * Description:
 * 	Returns true if the taint indicates that the corresponding value is unknown.
 */
static bool
taint_unknown(aarch64_sim_taint taint) {
	return (taint.t_or & TAINT_UNKNOWN) != 0;
}

/*
 * clear_temporaries
 *
 * Description:
 * 	Clear the temporary registers after a function call/return.
 */
static void
clear_temporaries(struct aarch64_sim *sim, aarch64_sim_taint taint) {
	for (size_t n = TEMPREGS_START; n <= TEMPREGS_END; n++) {
		aarch64_sim_word_clear(sim, &sim->X[n]);
		aarch64_sim_taint_meet_with(&sim->X[n].taint, taint);
	}
}

/*
 * get_instruction
 *
 * Description:
 * 	Get the next instruction at the specified PC value.
 */
static bool
get_instruction(struct ksim *ksim, uint32_t *ins) {
	uint64_t pc = ksim->sim.PC.value;
	assert(ksim->code_address <= pc && pc < ksim->code_address + ksim->code_size);
	uint64_t offset = pc - ksim->code_address;
	// Make sure we have enough data left to fetch a full instruction.
	if (ksim->code_size - offset < sizeof(*ins)) {
		return false;
	}
	/// Get the instruction.
	*ins = *(const uint32_t *)((uintptr_t)ksim->code_data + offset);
	return true;
}

/*
 * ksim_instruction_fetch
 *
 * Description:
 * 	The aarch64_sim callback to fetch the next instruction.
 */
static bool
ksim_instruction_fetch(struct aarch64_sim *sim) {
	struct ksim *ksim = sim->context;
	// Abort if the PC's taint is unknown.
	if (taint_unknown(ksim->sim.PC.taint)) {
		return false;
	}
	// Limit the total number of instructions we execute.
	if (ksim->instruction_count >= ksim->max_instruction_count) {
		return false;
	}
	// Clear temporary registers if we are resuming after a function call.
	if (ksim->internal.clear_temporaries) {
		// This taint is from the old instruction: we haven't updated to the new
		// instruction yet.
		clear_temporaries(sim, sim->instruction.taint);
		ksim->internal.clear_temporaries = false;
	}
	// Get the next instruction.
	uint32_t ins;
	if (!get_instruction(ksim, &ins)) {
		return false;
	}
	// Check if the caller wants to stop before this instruction. If so, set the
	// did_stop_before field so that we know not to stop next time.
	if (ksim->stop_before != NULL && !ksim->internal.did_stop_before) {
		if (ksim->stop_before(ksim, ins)) {
			ksim->internal.did_stop_before = true;
			ksim->internal.do_stop = true;
			return false;
		}
	}
	// Set the new instruction but keep the taint the same.
	ksim->internal.did_stop_before = false;
	sim->instruction.value = ins;
	ksim->instruction_count++;
	return true;
}

/*
 * ksim_memory_load
 *
 * Description:
 * 	The aarch64_sim callback to load a value from memory.
 */
static bool
ksim_memory_load(struct aarch64_sim *sim, struct aarch64_sim_word *value,
		const struct aarch64_sim_word *address, size_t size) {
	aarch64_sim_taint_meet_with(&value->taint, sim->taint_default[AARCH64_SIM_TAINT_UNKNOWN]);
	value->value = 0;
	return true;
}

/*
 * ksim_memory_store
 *
 * Description:
 * 	The aarch64_sim callback to store a value to memory.
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
 * 	The aarch64_sim callback informing us of a branch about to be taken. This function always
 * 	aborts simulation.
 */
static bool
ksim_branch_hit(struct aarch64_sim *sim, enum aarch64_sim_branch_type type,
		const struct aarch64_sim_word *branch, const struct aarch64_sim_word *condition,
		bool *take_branch) {
	struct ksim *ksim = sim->context;
	// If the user has a branch handler, use that.
	if (ksim->handle_branch != NULL) {
		uint64_t branch_address = branch->value;
		if (taint_unknown(branch->taint)) {
			branch_address = KSIM_PC_UNKNOWN;
		}
		enum ksim_branch_condition branch_condition = KSIM_BRANCH_CONDITION_UNKNOWN;
		if (!taint_unknown(condition->taint)) {
			branch_condition = (condition->value
					? KSIM_BRANCH_CONDITION_TRUE
					: KSIM_BRANCH_CONDITION_FALSE);
		}
		bool stop;
		bool handled = ksim->handle_branch(ksim, sim->instruction.value, branch_address,
				branch_condition, take_branch, &stop);
		if (handled) {
			if (stop) {
				ksim->internal.do_stop = true;
			}
			return !stop;
		}
	}
	// Don't take conditional branches and branches with link.
	*take_branch = (type != AARCH64_SIM_BRANCH_TYPE_CONDITIONAL
			&& type != AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK);
	// If the current branch looks like a function call, clear the temporary registers.
	if (type == AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK) {
		ksim->internal.clear_temporaries = true;
	}
	// Stop the simulator if we're about to execute an unknown address.
	if (*take_branch && taint_unknown(branch->taint)) {
		return false;
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
	for (size_t n = 0; n < AARCH64_SIM_GPREGS; n++) {
		aarch64_sim_word_clear(sim, &sim->X[n]);
	}
	aarch64_sim_pstate_clear(sim, &sim->PSTATE);
	return true;
}

void
ksim_init(struct ksim *ksim, const void *code, size_t size, uint64_t address, uint64_t pc) {
	assert(address <= pc && pc < address + size);
	// Initialize the aarch64_sim client state.
	ksim->sim.context             = ksim;
	ksim->sim.instruction_fetch   = ksim_instruction_fetch;
	ksim->sim.memory_load         = ksim_memory_load;
	ksim->sim.memory_store        = ksim_memory_store;
	ksim->sim.branch_hit          = ksim_branch_hit;
	ksim->sim.illegal_instruction = ksim_illegal_instruction;
	ksim->sim.taint_default       = ksim_taints;
	// Clear the simulator.
	aarch64_sim_clear(&ksim->sim);
	// Initialize the ksim state.
	ksim->code_data             = code;
	ksim->code_size             = size;
	ksim->code_address          = address;
	ksim->stop_before           = NULL;
	ksim->stop_after            = NULL;
	ksim->handle_branch         = NULL;
	ksim->max_instruction_count = KSIM_MAX_INSTRUCTIONS;
	ksim->instruction_count     = 0;
	// Initialize the internal state.
	ksim->internal.do_stop           = false;
	ksim->internal.did_stop_before   = false;
	ksim->internal.clear_temporaries = false;
	// Set the simulator to start at the specified PC.
	ksim->sim.PC.value          = pc;
	ksim->sim.PC.taint          = ksim->sim.taint_default[AARCH64_SIM_TAINT_CONSTANT];
	ksim->sim.instruction.taint = ksim->sim.PC.taint;
}

/*
 * find_code_segment
 *
 * Description:
 * 	Find the load command for the segment containing the given address.
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

bool
ksim_init_kext(struct ksim *ksim, const struct kext *kext, uint64_t pc) {
	const struct load_command *sc = find_code_segment(&kext->macho, pc);
	if (sc == NULL) {
		return false;
	}
	const void *code;
	uint64_t address;
	size_t size;
	macho_segment_data(&kext->macho, sc, &code, &address, &size);
	ksim_init(ksim, code, size, address, pc);
	return true;
}

bool
ksim_run(struct ksim *ksim) {
	ksim->internal.do_stop = false;
	for (;;) {
		// Step the simulator.
		if (!aarch64_sim_step(&ksim->sim)) {
			return ksim->internal.do_stop;
		}
		// Check if we should stop.
		if (ksim->stop_after != NULL) {
			if (ksim->stop_after(ksim, ksim->sim.instruction.value)) {
				return true;
			}
		}
	}
}

bool
ksim_reg(struct ksim *ksim, aarch64_gpreg reg, uint64_t *value) {
	if (AARCH64_GPREGZR(reg)) {
		*value = 0;
		return true;
	}
	unsigned n = AARCH64_GPREGID(reg);
	uint64_t mask = ones(AARCH64_GPREGSIZE(reg));
	struct aarch64_sim_word *word;
	if (n == AARCH64_SIM_GPREGS) {
		word = &ksim->sim.SP;
	} else {
		word = &ksim->sim.X[n];
	}
	if (taint_unknown(word->taint)) {
		return false;
	}
	*value = word->value & mask;
	return true;
}
