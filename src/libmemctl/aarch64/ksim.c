#include "memctl/aarch64/ksim.h"
#include "memctl/utility.h"


// AArch64 temporary registers.
#define TEMPREGS_START 0
#define TEMPREGS_END   17

// A strong taint denoting that the value is unknown.
#define TAINT_UNKNOWN 0x1

// A ksim instance will only execute a maximum of 0x1000000 instructions.
#define KSIM_MAX_INSTRUCTIONS 0x1000000

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
	// Check if the caller wants to break.
	if (ksim->instruction_count != ksim->internal.last_break
			&& ksim->run_until != NULL
			&& ksim->run_until(ksim, ins)) {
		ksim->internal.last_break = ksim->instruction_count;
		ksim->internal.break_condition = true;
		return false;
	}
	// Set the new instruction but keep the taint the same.
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
		const struct aarch64_sim_word *branch, bool *take_branch) {
	struct ksim *ksim = sim->context;
	// We will eventually need to let the client decide how to handle branches.
	*take_branch = false;
	// If the current branch looks like a function call, clear the temporary registers.
	// Branches without link are usually not function calls, but sometimes they are. There's
	// not really a good way to distinguish them as of yet.
	if (type == AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK) {
		ksim->internal.clear_temporaries = true;
		return true;
	}
	// For any other branch types we halt.
	return false;
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
ksim_init(struct ksim *ksim, const void *code, size_t size, uint64_t code_address, uint64_t pc) {
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
	ksim->code_address          = code_address;
	ksim->run_until             = NULL;
	ksim->max_instruction_count = KSIM_MAX_INSTRUCTIONS;
	ksim->instruction_count     = 0;
	// Initialize the internal state.
	ksim->internal.break_condition   = false;
	ksim->internal.last_break        = (uint64_t)(-1);
	ksim->internal.clear_temporaries = false;
	// Set the simulator to start at the specified PC.
	ksim->sim.PC.value          = pc;
	ksim->sim.PC.taint          = ksim->sim.taint_default[AARCH64_SIM_TAINT_CONSTANT];
	ksim->sim.instruction.taint = ksim->sim.PC.taint;
}

bool
ksim_run(struct ksim *ksim) {
	ksim->internal.break_condition = false;
	for (;;) {
		if (!aarch64_sim_step(&ksim->sim)) {
			return ksim->internal.break_condition;
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
