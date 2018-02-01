#ifndef MEMCTL__ARM64__KSIM_H_
#define MEMCTL__ARM64__KSIM_H_

#include "memctl/arm64/disasm.h"
#include "memctl/arm64/sim.h"
#include "memctl/mapped_region.h"

/*
 * struct ksim
 *
 * Description:
 * 	The AArch64 kernel/kext simulator.
 */
struct ksim {
	// The aarch64_sim.
	struct aarch64_sim sim;
	// The bytecode being executed.
	struct mapped_region code;
	// Internal simulation state.
	bool clear_temporaries;
	bool did_stop;
};

/*
 * ksim_branch
 *
 * Description:
 * 	The type of element in a branch descriptor array. See ksim_exec_until.
 *
 * 	The ksim_exec functions take an array of ksim_branch elements to describe the simulator's
 * 	branching behavior on conditional branches. The elements are interpreted in-order for each
 * 	branch that is encountered during execution. Valid values are:
 * 	- 0 or false, to specify that the corresponding branch should not be taken.
 * 	- 1 or true, to specify that the corresponding branch should be taken.
 * 	- KSIM_BRANCH_ALL_FALSE to specify that the simulator should treat all further branches as
 * 	  false and stop reading from the array.
 *
 * 	Passing NULL to ksim_exec_until is equivalent to specifying an array with the single value
 * 	KSIM_BRANCH_ALL_FALSE.
 */
typedef uint8_t ksim_branch;

enum {
	KSIM_BRANCH_ALL_FALSE = true + 1,
};

/*
 * ksim_symbol
 *
 * Description:
 * 	Return the static address of the symbol in the given kernel extension.
 *
 * Parameters:
 * 		kext			The bundle ID of the kernel extension, or NULL for the
 * 					kernel.
 * 		symbol			The symbol name.
 *
 * Returns:
 * 	The address of the symbol or 0 if the symbol was not found.
 */
kaddr_t ksim_symbol(const char *kext, const char *symbol);

/*
 * ksim_string_reference
 *
 * Description:
 * 	Find the address of the first instruction that references the specified string.
 *
 * Parameters:
 * 		kext			The bundle ID of the kernel extension, or NULL for the
 * 					kernel.
 * 		reference		The string to find a reference to.
 *
 * Returns:
 * 	The address of the instruction or 0 if no instruction creates a reference to the string.
 */
kaddr_t ksim_string_reference(const char *kext, const char *reference);

/*
 * ksim_init_sim
 *
 * Description:
 * 	Initialize the simulator state to prepare it for execution. All registers are cleared and
 * 	PC is set to the specified value.
 *
 * 	This function must be called before any ksim_exec function can be used.
 *
 * Parameters:
 * 		ksim			The ksim struct to initialize.
 * 		pc			The PC value. May be 0.
 */
void ksim_init_sim(struct ksim *ksim, kaddr_t pc);

/*
 * ksim_clearregs
 *
 * Description:
 * 	Clear all the registers in the ksim struct except PC.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 */
void ksim_clearregs(struct ksim *ksim);

/*
 * ksim_set_pc
 *
 * Description:
 * 	Set the PC register in the simulator.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		pc			The PC value.
 *
 * Notes:
 * 	An assertion error is raised if the PC does not lie within the kernel or any kernel
 * 	extension.
 */
void ksim_set_pc(struct ksim *ksim, kaddr_t pc);

// Constants to use for ksim_scan_for functions.
enum {
	KSIM_FW =  1,
	KSIM_BW = -1,
};

/*
 * ksim_scan_for
 *
 * Description:
 * 	Advance PC at most count times until the specified instruction is found.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		direction		The direction to scan: KSIM_FW for forwards, KSIM_BW for
 * 					backwards.
 * 		ins			The instruction to find.
 * 		mask			Which bits in ins to match against.
 * 		index			The index of the matching instruction to stop at. Specify 0
 * 					to stop at the first matching instruction, 1 for the
 * 					second, and so on.
 * 	out	pc			On return, the PC of the instruction, if it was found.
 * 		count			The maximum number of instructions to scan forward.
 *
 * Returns:
 * 	True if the instruction was found.
 */
bool ksim_scan_for(struct ksim *ksim, int direction, uint32_t ins, uint32_t mask, unsigned index,
		kaddr_t *pc, unsigned count);

/*
 * ksim_scan_for_jump
 *
 * Description:
 * 	Advance PC until a B instruction is found.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		direction		The direction to scan: KSIM_FW for forwards, KSIM_BW for
 * 					backwards.
 * 		index			The index of the branch instruction.
 * 	out	pc			On return, the PC of the instruction, if it was found.
 * 	out	target			On return, the branch target.
 * 		count			The maximum number of instructions to scan forward.
 *
 * Returns:
 * 	True if the jump instruction was encountered.
 */
bool ksim_scan_for_jump(struct ksim *ksim, int direction, unsigned index, kaddr_t *pc,
		kaddr_t *target, unsigned count);

/*
 * ksim_scan_for_call
 *
 * Description:
 * 	Advance PC until a BL instruction is found.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		direction		The direction to scan: KSIM_FW for forwards, KSIM_BW for
 * 					backwards.
 * 		index			The index of the branch instruction.
 * 	out	pc			On return, the PC of the instruction, if it was found.
 * 	out	target			On return, the branch target.
 * 		count			The maximum number of instructions to scan forward.
 *
 * Returns:
 * 	True if the call instruction was encountered.
 */
bool ksim_scan_for_call(struct ksim *ksim, int direction, unsigned index, kaddr_t *pc,
		kaddr_t *target, unsigned count);

/*
 * ksim_setreg
 *
 * Description:
 * 	Set the value of an AArch64 general-purpose register.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		reg			The general-purpose register to set.
 * 		value			The new value of the register.
 */
void ksim_setreg(struct ksim *ksim, aarch64_gpreg reg, kword_t value);

/*
 * ksim_getreg
 *
 * Description:
 * 	Get the value of an AArch64 general-purpose register.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		reg			The general-purpose register to read.
 * 	out	value			On return, the value of the register, if it is known.
 *
 * Returns:
 * 	True if the register's value was known and successfully retrieved.
 */
bool ksim_getreg(struct ksim *ksim, aarch64_gpreg reg, kword_t *value);

/*
 * ksim_reg
 *
 * Description:
 * 	A wrapper around ksim_getreg that returns the register's value if it is known and 0
 * 	otherwise.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		reg			The general-purpose register to read.
 *
 * Returns:
 * 	The register's value or 0.
 */
static inline kword_t ksim_reg(struct ksim *ksim, aarch64_gpreg reg) {
	kword_t value;
	bool success = ksim_getreg(ksim, reg, &value);
	return (success ? value : 0);
}

/*
 * ksim_exec_until_callback
 *
 * Description:
 * 	A callback that specifies when ksim_exec_until should terminate.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		context			Caller-supplied context.
 * 		pc			The PC register.
 * 		ins			The current instruction that is about to be executed.
 *
 * Returns:
 * 	True if execution should stop here.
 */
typedef bool (*ksim_exec_until_callback)(void *context, struct ksim *ksim, kaddr_t pc,
		uint32_t ins);

/*
 * ksim_exec_until
 *
 * Description:
 * 	Run the simulator for the specified number of steps, or until the callback specifies
 * 	execution should stop.
 *
 * 	Execution stops whenever the until callback returns true or whenever execution takes a
 * 	branch (without link) to an unknown location. Branches with link are not taken, but
 * 	temporary registers are marked as unknown to simulate a function call.
 *
 * 	If the until callback stopped execution, the PC register will be set to the instruction at
 * 	which until returned true. Otherwise, the PC register will be set to the instruction that
 * 	triggered the stop.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		until			A callback specifying when execution should stop. May be
 * 					NULL.
 * 		context			A context value passed to the callback.
 * 		branches		An array of ksim_branch elements specifying conditional
 * 					branching behavior. See ksim_branch. May be NULL.
 * 		count			The maximum number of instructions to execute.
 *
 * Returns:
 * 	True if the stop condition was encountered.
 */
bool ksim_exec_until(struct ksim *ksim, ksim_exec_until_callback until, void *context,
		ksim_branch *branches, unsigned count);

/*
 * ksim_exec_until_call
 *
 * Description:
 * 	Run the simulator until a function call (BL instruction).
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		branches		The branching behavior. See ksim_exec_until.
 * 	out	target			The call target. May be NULL.
 * 		count			The maximum number of instructions to execute.
 *
 * Returns:
 * 	True if the call instruction was encountered.
 */
bool ksim_exec_until_call(struct ksim *ksim, ksim_branch *branches, kaddr_t *target,
		unsigned count);

/*
 * ksim_exec_until_return
 *
 * Description:
 * 	Run the simulator until a return (RET instruction).
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		branches		The branching behavior. See ksim_exec_until.
 * 		count			The maximum number of instructions to execute.
 *
 * Returns:
 * 	True if the return instruction was encountered.
 */
bool ksim_exec_until_return(struct ksim *ksim, ksim_branch *branches, unsigned count);

/*
 * ksim_exec_until_store
 *
 * Description:
 * 	Run the simulator until a store (STR, STRB, or STRH instruction) with the specified
 * 	base register.
 *
 * Parameters:
 * 		ksim			The ksim struct.
 * 		branches		The branching behavior. See ksim_exec_until.
 * 		base			The general-purpose base register.
 * 	out	value			On return, the value stored, if it is known.
 * 		count			The maximum number of instructions to execute.
 *
 * Returns:
 * 	True if the store instruction was encountered.
 */
bool ksim_exec_until_store(struct ksim *ksim, ksim_branch *branches, aarch64_gpreg base,
		kword_t *value, unsigned count);

#endif
