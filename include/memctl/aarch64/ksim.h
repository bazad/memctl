#ifndef MEMCTL__AARCH64__KSIM_H_
#define MEMCTL__AARCH64__KSIM_H_

#include "memctl/aarch64/sim.h"
#include "memctl/aarch64/disasm.h"
#include "memctl/kernel.h"

// Forward declarations
struct ksim;

/*
 * macro KSIM_PC_UNKNOWN
 *
 * Description:
 * 	A PC value representing an unknown address.
 */
#define KSIM_PC_UNKNOWN 0xffffffffffffffff

/*
 * ksim_stop_fn
 *
 * Description:
 * 	A client-specified function that returns true when the simulator should stop.
 *
 * Parameters:
 * 		ksim			The ksim simulator.
 * 		ins			The instruction that is about to be executed (for
 * 					stop_before) or that was just executed (for stop_after).
 *
 * Returns:
 * 	True if the simulator should stop now, false to continue running.
 */
typedef bool (*ksim_stop_fn)(
		struct ksim *ksim,
		uint32_t ins);

/*
 * enum ksim_branch_condition
 *
 * Description:
 * 	For conditional branch instructions, the branch condition (true, false, or unknown).
 */
enum ksim_branch_condition {
	KSIM_BRANCH_CONDITION_UNKNOWN,
	KSIM_BRANCH_CONDITION_TRUE,
	KSIM_BRANCH_CONDITION_FALSE,
};

/*
 * ksim_handle_branch_fn
 *
 * Description:
 * 	A client-specified function that instructs the simulator how to handle a branch
 * 	instruction.
 *
 * Parameters:
 * 		ksim			The simulator.
 * 		ins			The branch instruction.
 * 		branch_address		The address that would be branched to if the branch
 * 					condition is true.
 * 		branch_condition	For a conditional branch instruction, the branch condition
 * 					(i.e., whether the branch would be taken).
 * 	out	take_branch		On return, instructs the simulator whether or not to take
 * 					the branch.
 * 	out	stop			On return, instructs the simulator whether to stop after
 * 					processing this instruction (overriding stop_after).
 *
 * Returns:
 * 	True to indicate that the branch was handled, false to let the simulator handle the branch
 * 	itself.
 */
typedef bool (*ksim_handle_branch_fn)(
		struct ksim *ksim,
		uint32_t ins,
		uint64_t branch_address,
		enum ksim_branch_condition branch_condition,
		bool *take_branch,
		bool *stop);

/*
 * struct ksim
 *
 * Description:
 * 	The AArch64 kernel/kext simulator.
 *
 * Behavior:
 * 	All general-purpose register values are initially unknown.
 *
 * 	No memory state is maintained. Values read from memory are always assumed to be unknown.
 * 	All stores are assumed to succeed.
 *
 * 	The simulator's behavior on branch instructions can be customized by specifying a
 * 	handle_branch function. By default, branches with link and conditional branches are not
 * 	taken, while every other branch type is taken. The simulator aborts on a ret instruction.
 */
struct ksim {
	// The aarch64_sim.
	struct aarch64_sim sim;
	// The bytecode being executed.
	const void *code_data;
	size_t code_size;
	uint64_t code_address;
	// Client-specified context.
	void *context;
	// A callback indicating if the simulation should stop before executing the current
	// instruction.
	ksim_stop_fn stop_before;
	// A callback indicating if the simulation should stop after executing the current
	// instruction.
	ksim_stop_fn stop_after;
	// A callback to allow the client to specify behavior on branch instructions.
	ksim_handle_branch_fn handle_branch;
	// The maximum number of instructions to execute before aborting.
	size_t max_instruction_count;
	// The current instruction count.
	size_t instruction_count;
	// Internal state.
	struct {
		bool do_stop;
		bool did_stop_before;
		bool clear_temporaries;
	} internal;
};

/*
 * ksim_init
 *
 * Description:
 * 	Initialize the ksim simulator.
 *
 * Parameters:
 * 		ksim			The simulator.
 * 		code			The AArch64 bytecode.
 * 		size			The size of the bytecode.
 * 		address			The address of the start of the bytecode.
 * 		pc			The address at which to start simulating. This must lie
 * 					within the region spanned by the bytecode.
 */
void ksim_init(struct ksim *ksim, const void *code, size_t size, uint64_t address, uint64_t pc);

/*
 * ksim_init_kext
 *
 * Description:
 * 	Initialize the ksim simulator with the specified kext.
 *
 * Parameters:
 * 		ksim			The simulator.
 * 		kext			The kernel extension to simulate.
 * 		pc			The address at which to start simulating.
 *
 * Returns:
 * 	True if the segment name was found in the kext.
 */
bool ksim_init_kext(struct ksim *ksim, const struct kext *kext, uint64_t pc);

/*
 * ksim_run
 *
 * Description:
 * 	Run the simulator until any stop condition is met (see stop_before, stop_after, and
 * 	handle_branch).
 *
 * Parameters:
 * 		ksim			The simulator.
 *
 * Returns:
 * 	True if the simulator was stopped, false if the simulator aborted.
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
