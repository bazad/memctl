#ifndef MEMCTL__KERNEL_CALL_H_
#define MEMCTL__KERNEL_CALL_H_

#include "memctl_types.h"

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
 * kernel_call_7
 *
 * Description:
 * 	Call a kernel function with the given arguments. See kernel_call.
 *
 * Restrictions:
 * 	args[0] must be nonzero.
 * 	The return value is truncated to 32 bits.
 */
bool kernel_call_7(void *result, unsigned result_size, unsigned arg_count,
		kaddr_t func, kword_t args[]);

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
 * 		arg_count		The number of arguments to the function. There can be no
 * 					more than 8 arguments, and on some platforms, the true
 * 					maximum number of supported arguments may be even smaller.
 * 		func			The function to call, or 0 to test if the given function
 * 					call is possible given the available functionality.
 * 		args			The arguments to the function.
 *
 * Returns:
 * 	true if the function call succeeded, false if there was an error.
 *
 * 	If func was 0, then this function returns true if the given call is supported and false
 * 	otherwise, with no errors produced.
 */
bool kernel_call(void *result, unsigned result_size, unsigned arg_count,
		kaddr_t func, kword_t args[]);

#endif
