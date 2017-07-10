#ifndef MEMCTL__AARCH64__AARCH64_SIM_H_
#define MEMCTL__AARCH64__AARCH64_SIM_H_

#include "memctl/aarch64/disasm.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Forward declarations
struct aarch64_sim;

/*
 * macro AARCH64_SIM_INSTRUCTION_SIZE
 *
 * Description:
 * 	The size of an AArch64 instruction.
 */
#define AARCH64_SIM_INSTRUCTION_SIZE	(sizeof(uint32_t))

/*
 * aarch64_sim_taint
 *
 * Description:
 * 	A taint is an attribute specified on some value processed by the simulator. The
 * 	aarch64_sim_taint struct has space for 32 weak taints and 32 strong taints.
 *
 * 	Taints from different sources can be combined using a meet operation. The meet of a weak
 * 	taint from two sources is the AND of the taint values, while the meet of a strong taint
 * 	from two sources is the OR of the values.
 */
struct aarch64_sim_taint_s {
	// Weak taints. The meet of these taints is bitwise AND.
	uint32_t t_and;
	// Strong taints. The meet of these taints is bitwise OR.
	uint32_t t_or;
};
typedef struct aarch64_sim_taint_s aarch64_sim_taint;

/*
 * macro AARCH64_SIM_TAINT_TOP
 *
 * Description:
 * 	An aarch64_sim_taint value that acts as the identity under meet: that is, the meet of TOP
 * 	and X is always X.
 */
#define AARCH64_SIM_TAINT_TOP	((aarch64_sim_taint) { .t_and = 0xffffffff, .t_or = 0 })

/*
 * struct aarch64_sim_word
 *
 * Description:
 * 	A 64-bit word and associated taints.
 */
struct aarch64_sim_word {
	uint64_t value;
	aarch64_sim_taint taint;
};

/*
 * enum aarch64_sim_taint_default_type
 *
 * Description:
 * 	The indices for taints in the client-specified taint_default table.
 */
enum aarch64_sim_taint_default_type {
	AARCH64_SIM_TAINT_CONSTANT,
	AARCH64_SIM_TAINT_UNKNOWN,
};

/*
 * aarch64_sim_instruction_fetch_fn
 *
 * Description:
 * 	A client-supplied function to fetch the next instruction to be executed. The address of the
 * 	next instruction is stored in sim->PC, and returned instruction should be stored in
 * 	sim->instruction.
 *
 * Parameters:
 * 		sim			The simulator.
 *
 * Returns:
 * 	True if the simulator should continue, false to abort before executing the instruction.
 */
typedef bool (*aarch64_sim_instruction_fetch_fn)(
		struct aarch64_sim *sim);

/*
 * aarch64_sim_memory_load_fn
 *
 * Description:
 * 	A client-supplied function to load a value from memory.
 *
 * Parameters:
 * 		sim			The simulator.
 * 	out	value			On return, the value loaded from memory.
 * 		address			The address being loaded.
 * 		size			The size of the load in bytes.
 *
 * Returns:
 * 	True if the simulator should continue, false to abort after executing the instruction.
 */
typedef bool (*aarch64_sim_memory_load_fn)(
		struct aarch64_sim *sim,
		struct aarch64_sim_word *value,
		const struct aarch64_sim_word *address,
		size_t size);

/*
 * aarch64_sim_memory_store_fn
 *
 * Description:
 * 	A client-supplied function to store a value to memory.
 *
 * Parameters:
 * 		sim			The simulator.
 * 		value			The value to store in memory.
 * 		address			The address to which the value is being stored.
 * 		size			The size of the store in bytes.
 *
 * Returns:
 * 	True if the simulator should continue, false to abort after executing the instruction.
 */
typedef bool (*aarch64_sim_memory_store_fn)(
		struct aarch64_sim *sim,
		const struct aarch64_sim_word *value,
		const struct aarch64_sim_word *address,
		size_t size);

/*
 * enum aarch64_sim_branch_type
 *
 * Description:
 * 	The type of branch instruction that was hit.
 */
enum aarch64_sim_branch_type {
	AARCH64_SIM_BRANCH_TYPE_BRANCH,
	AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK,
	AARCH64_SIM_BRANCH_TYPE_RETURN,
	AARCH64_SIM_BRANCH_TYPE_CONDITIONAL,
};

/*
 * aarch64_sim_branch_hit_fn
 *
 * Description:
 * 	A client-supplied function that is called whenever a branch instruction is executed.
 *
 * Parameters:
 * 		sim			The simulator.
 * 		type			The type of branch.
 * 		branch			The address of the branch.
 * 		condition		For conditional branch instructions, the branch condition
 * 					(i.e., true if the branch should be taken).
 * 	out	take_branch		On return, indicates whether the simulator should take the
 * 					branch or not.
 *
 * Returns:
 * 	True if the simulator should continue, false to abort after executing the instruction.
 */
typedef bool (*aarch64_sim_branch_hit_fn)(
		struct aarch64_sim *sim,
		enum aarch64_sim_branch_type type,
		const struct aarch64_sim_word *branch,
		const struct aarch64_sim_word *condition,
		bool *take_branch);

/*
 * aarch64_sim_illegal_instruction_fn
 *
 * Description:
 * 	A client-supplied function that is called whenever the simulator encounters an illegal
 * 	instruction.
 *
 * Parameters:
 * 		sim			The simulator.
 *
 * Returns:
 * 	True if the simulator should ignore the illegal instruction and continue, false to abort
 * 	after this instruction.
 *
 * Notes:
 * 	The simulator does not support exceptions.
 */
typedef bool (*aarch64_sim_illegal_instruction_fn)(
		struct aarch64_sim *sim);

/*
 * aarch64_pstate
 *
 * Description:
 * 	The PSTATE register.
 */
typedef uint32_t aarch64_pstate;

#define AARCH64_PSTATE_NZCV     0xf0000000
#define AARCH64_PSTATE_N        0x80000000
#define AARCH64_PSTATE_Z        0x40000000
#define AARCH64_PSTATE_C        0x20000000
#define AARCH64_PSTATE_V        0x10000000
#define AARCH64_PSTATE_Q_32     0x08000000
#define AARCH64_PSTATE_IT_32    0x06000000
#define AARCH64_PSTATE_J_32     0x01000000
#define AARCH64_PSTATE_SS_64    0x00200000
#define AARCH64_PSTATE_IL       0x00100000
#define AARCH64_PSTATE_GE_32    0x000f0000
#define AARCH64_PSTATE_IT2_32   0x0000fc00
#define AARCH64_PSTATE_E_32     0x00000200
#define AARCH64_PSTATE_D_64     0x00000200
#define AARCH64_PSTATE_A        0x00000100
#define AARCH64_PSTATE_I        0x00000080
#define AARCH64_PSTATE_F        0x00000040
#define AARCH64_PSTATE_T_32     0x00000020
#define AARCH64_PSTATE_M        0x00000010
#define AARCH64_PSTATE_M2       0x0000000f

#define AARCH64_PSTATE_SHIFT_NZCV   28
#define AARCH64_PSTATE_SHIFT_N      31
#define AARCH64_PSTATE_SHIFT_Z      30
#define AARCH64_PSTATE_SHIFT_C      29
#define AARCH64_PSTATE_SHIFT_V      28
#define AARCH64_PSTATE_SHIFT_Q_32   27
#define AARCH64_PSTATE_SHIFT_IT_32  25
#define AARCH64_PSTATE_SHIFT_J_32   24
#define AARCH64_PSTATE_SHIFT_SS_64  21
#define AARCH64_PSTATE_SHIFT_IL     20
#define AARCH64_PSTATE_SHIFT_GE_32  16
#define AARCH64_PSTATE_SHIFT_IT2_32 10
#define AARCH64_PSTATE_SHIFT_E_32   9
#define AARCH64_PSTATE_SHIFT_D_64   9
#define AARCH64_PSTATE_SHIFT_A      8
#define AARCH64_PSTATE_SHIFT_I      7
#define AARCH64_PSTATE_SHIFT_F      6
#define AARCH64_PSTATE_SHIFT_T_32   5
#define AARCH64_PSTATE_SHIFT_M      4
#define AARCH64_PSTATE_SHIFT_M2     0

/*
 * struct aarch64_sim_pstate
 *
 * Description:
 * 	The simulator's PSTATE register and associated taints.
 */
struct aarch64_sim_pstate {
	aarch64_pstate pstate;
	aarch64_sim_taint taint_nzcv;
};

/*
 * struct aarch64_sim
 *
 * Description:
 * 	The AArch64 simulator structure.
 */
struct aarch64_sim {
	// Client context. This value is not used by the simulator.
	void *context;

	// Client callbacks.
	aarch64_sim_instruction_fetch_fn instruction_fetch;
	aarch64_sim_memory_load_fn memory_load;
	aarch64_sim_memory_store_fn memory_store;
	aarch64_sim_branch_hit_fn branch_hit;
	aarch64_sim_illegal_instruction_fn illegal_instruction;

	// The default taint table. See enum aarch64_sim_taint_default_type above.
	aarch64_sim_taint *taint_default;

	// The current instruction.
	struct aarch64_sim_word instruction;

	// Simulator state.
	struct aarch64_sim_word PC;
#define AARCH64_SIM_GPREGS 31
	struct aarch64_sim_word X[AARCH64_SIM_GPREGS];
	struct aarch64_sim_word SP;
	struct aarch64_sim_pstate PSTATE;
};

/*
 * aarch64_sim_taint_meet_with
 *
 * Description:
 * 	Meet taint a with taint b, storing the result in a.
 *
 * Parameters:
 * 	inout	a			The first taint.
 * 		b			The second taint.
 */
void aarch64_sim_taint_meet_with(aarch64_sim_taint *a, aarch64_sim_taint b);

/*
 * aarch64_sim_word_clear
 *
 * Description:
 * 	Clear the given word and reset its taint to AARCH64_SIM_TAINT_UNKNOWN.
 *
 * Parameters:
 * 		sim			The simulator.
 * 		word			The word to clear.
 */
void aarch64_sim_word_clear(struct aarch64_sim *sim, struct aarch64_sim_word *word);

/*
 * aarch64_sim_pstate_clear
 *
 * Description:
 * 	Clear the PSTATE and reset its taints to AARCH64_SIM_TAINT_UNKNOWN.
 *
 * Parameters:
 * 		sim			The simulator.
 * 		pstate			The PSTATE to clear.
 */
void aarch64_sim_pstate_clear(struct aarch64_sim *sim, struct aarch64_sim_pstate *pstate);

/*
 * aarch64_sim_clear
 *
 * Description:
 * 	Clear the simulator state. All values are initialized as AARCH64_SIM_TAINT_UNKNOWN.
 *
 * Parameters:
 * 		sim			The simulator.
 */
void aarch64_sim_clear(struct aarch64_sim *sim);

/*
 * aarch64_sim_pc_advance
 *
 * Description:
 * 	Advance the PC by one instruction.
 *
 * Parameters:
 * 		sim			The simulator.
 */
void aarch64_sim_pc_advance(struct aarch64_sim *sim);
/*
 * aarch64_sim_step
 *
 * Description:
 * 	Execute a single instruction.
 *
 * Parameters:
 * 		sim			The simulator.
 *
 * Returns:
 * 	False if the simulator was aborted during any client callback, true otherwise.
 */
bool aarch64_sim_step(struct aarch64_sim *sim);

/*
 * aarch64_sim_run
 *
 * Description:
 * 	Run the simulator until aarch64_sim_step returns false.
 *
 * Parameters:
 * 		sim			The simulator.
 */
void aarch64_sim_run(struct aarch64_sim *sim);

#endif
