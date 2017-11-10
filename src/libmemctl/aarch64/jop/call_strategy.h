#ifndef MEMCTL__AARCH64__JOP__CALL_STRATEGY_H_
#define MEMCTL__AARCH64__JOP__CALL_STRATEGY_H_

#include "memctl/memctl_types.h"

/*
 * jop_call_initial_state
 *
 * Description:
 * 	A struct to keep track of register values when starting JOP.
 */
struct jop_call_initial_state {
	uint64_t pc;
	uint64_t x[7];
};

/*
 * jop_call_build_fn
 *
 * Description:
 * 	A function to build a JOP payload and set up arguments to kernel_call_7.
 *
 * Parameters:
 * 		func			The kernel function to call.
 * 		args			The arguments to the kernel function.
 * 		stack			The extra arguments beyond the first 8 passed on the stack.
 * 		kernel_payload		The address of the payload in the kernel.
 * 	out	payload			On return, the JOP payload. This will be copied into the
 * 					kernel at address jop_payload.
 * 	out	initial_state		On return, the state of the CPU registers to set at the
 * 					start of JOP execution.
 * 	out	result_address		On return, the address of the result value.
 */
typedef void (*jop_call_build_fn)(uint64_t func, const uint64_t args[8], const uint64_t stack[8],
		kaddr_t kernel_payload, void *payload,
		struct jop_call_initial_state *initial_state, uint64_t *result_address);

/*
 * struct jop_call_strategy
 *
 * Description:
 * 	A description of a JOP call strategy.
 */
struct jop_call_strategy {
	uint64_t          gadgets[1];
	size_t            payload_size;
	size_t            stack_size;
	jop_call_build_fn build_jop;
};

// All of the defined JOP call strategies. See the corresponding C file for details about the
// implementation and capabilities.
extern struct jop_call_strategy jop_call_strategy_1;
extern struct jop_call_strategy jop_call_strategy_2;


// Internal definitions.

// Get the size of an array.
#define ARRSIZE(x)	(sizeof(x) / sizeof((x)[0]))

#endif
