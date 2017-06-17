#ifndef MEMCTL__AARCH64__KSIM_H_
#define MEMCTL__AARCH64__KSIM_H_

#include "memctl/aarch64/aarch64_sim.h"
#include "memctl/aarch64/disasm.h"

// Forward declarations
struct ksim;

/*
 * ksim_run_until_fn
 *
 * Description:
 * 	A client-specified function that returns true when the simulator should stop.
 *
 * Parameters:
 * 		ksim			The ksim simulator.
 * 		ins			The instruction about to be run.
 *
 * Returns:
 * 	True if the simulator should stop now (before processing the given instruction), false to
 * 	continue running.
 */
typedef bool (*ksim_run_until_fn)(
		struct ksim *ksim,
		uint32_t ins);

/*
 * struct ksim
 *
 * Description:
 * 	The AArch64 kernel/kext simulator.
 *
 * Behavior:
 * 	No memory state is maintained. Values read from memory are always assumed to be unknown.
 *
 * 	Branch and return instructions immediately abort the simulator. Branch-and-link
 * 	instructions (usually function calls) cause the simulator to clear temporary registers and
 * 	resume after the branch instruction.
 */
struct ksim {
	// The aarch64_sim.
	struct aarch64_sim sim;
	// The bytecode being executed.
	const void *code_data;
	size_t code_size;
	uint64_t code_address;
	// A callback indicating when the simulation should stop.
	ksim_run_until_fn run_until;
	// The maximum number of instructions to execute before aborting.
	size_t max_instruction_count;
	// The current instruction count.
	size_t instruction_count;
	// Internal state.
	struct {
		bool break_condition;
		size_t last_break;
		bool clear_temporaries;
	} internal;
};

/*
 * ksim_init
 *
 * Description:
 * 	Initialize the ksim simulator.
 */
void ksim_init(struct ksim *ksim, const void *code, size_t size, uint64_t code_address,
		uint64_t pc);

/*
 * ksim_run
 *
 * Description:
 * 	Run the simulator until the run_until function returns true.
 *
 * Parameters:
 * 		ksim			The simulator.
 *
 * Returns:
 * 	True if the condition specified by run_until is hit.
 */
bool ksim_run(struct ksim *ksim);

/*
 * ksim_reg
 *
 * Description:
 * 	Get the contents of the given register number.
 *
 * Parameters:
 * 		ksim			The simulator.
 * 		reg			The register name.
 * 	out	value			On return, the value of the register.
 *
 * Returns:
 * 	True if the register value was read successfully. If the register value is unknown, false
 * 	is returned.
 */
bool ksim_reg(struct ksim *ksim, aarch64_gpreg reg, uint64_t *value);

#endif
