#ifndef MEMCTL__KERNEL_CALL_H_
#define MEMCTL__KERNEL_CALL_H_

#include "memctl/memctl_types.h"
#include "memctl/offset.h"

DECLARE_OFFSET(IORegistryEntry, reserved);
DECLARE_OFFSET(IORegistryEntry__ExpansionData, fRegistryEntryID);

/*
 * kernel_call_init
 *
 * Description:
 * 	Initialize kernel_call functions.
 *
 * Dependencies:
 * 	kernel_task
 * 	kernel_slide
 * 	kernel
 * 	memctl_offsets
 *
 * Notes:
 * 	After this function is called, every effort should be made to ensure kernel_call_deinit is
 * 	called before the program exits. Otherwise, kernel resources may be leaked or the kernel
 * 	may be left in an inconsistent state.
 */
bool kernel_call_init(void);

/*
 * kernel_call_deinit
 *
 * Description:
 * 	Deinitialize the kernel call subsystem and restore the kernel to a safe state.
 */
void kernel_call_deinit(void);

/*
 * struct kernel_call_argument
 *
 * Description:
 * 	An argument to kernel_call.
 */
struct kernel_call_argument {
	// The size of the argument in bytes. This must be a power of 2 between 1 and the kernel
	// word size.
	size_t  size;
	// The argument value.
	kword_t value;
};

/*
 * macro KERNEL_CALL_ARG
 *
 * Description:
 * 	A helper macro to construct argument arrays for kernel_call.
 */
#define KERNEL_CALL_ARG(type, argument)	\
	((struct kernel_call_argument) { sizeof(type), argument })

/*
 * kernel_call_7
 *
 * Description:
 * 	Call a kernel function with the given arguments. See kernel_call.
 *
 * Restrictions:
 * 	args[0] must be nonzero.
 * 	The return value is truncated to 32 bits.
 */
bool kernel_call_7(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const struct kernel_call_argument args[]);

/*
 * kernel_call
 *
 * Description:
 * 	Call a kernel function with the given arguments.
 *
 * Parameters:
 * 	out	result			The return value of the kernel function.
 * 		result_size		The size of the return value in bytes. Must be 1, 2, 4, or
 * 					8.
 * 		func			The function to call, or 0 to test if the given function
 * 					call is possible given the available functionality.
 * 		arg_count		The number of arguments to the function. Maximum allowed
 * 					value is 32. The actual upper limit on the number of
 * 					arguments will usually be lower, and will vary by platform
 * 					based on the loaded functionality.
 * 		args			The arguments to the kernel function.
 *
 * Returns:
 * 	True if the function call succeeded, false if there was an error.
 *
 * 	If func was 0, then this function returns true if the given call is supported and false
 * 	otherwise, with no errors produced.
 *
 * Notes:
 * 	Currently only integer/pointer arguments are guaranteed to be supported.
 */
bool kernel_call(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const struct kernel_call_argument args[]);

/*
 * kernel_call_x
 *
 * Description:
 * 	Call a kernel function with the given word-sized arguments.
 *
 * Parameters:
 * 	out	result			The return value of the kernel function.
 * 		result_size		The size of the return value in bytes. Must be 1, 2, 4, or
 * 					8.
 * 		func			The function to call, or 0 to test if the given function
 * 					call is possible given the available functionality.
 * 		arg_count		The number of arguments to the function. Maximum allowed
 * 					value is 8. The actual upper limit on the number of
 * 					arguments will usually be lower, and will vary by platform
 * 					based on the loaded functionality.
 * 		args			The arguments to the kernel function. The kernel function
 * 					must expect every argument to be word-sized.
 *
 * Returns:
 * 	True if the function call succeeded, false if there was an error.
 *
 * 	If func was 0, then this function returns true if the given call is supported and false
 * 	otherwise, with no errors produced.
 *
 * Notes:
 * 	This is a convenience wrapper around kernel_call, for the common case when the kernel
 * 	function being called is known to expect word-sized arguments. This is true for example
 * 	when the kernel function takes all its arguments in registers.
 */
bool kernel_call_x(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const kword_t args[]);

#endif
