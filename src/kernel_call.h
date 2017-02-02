#ifndef MEMCTL__KERNEL_CALL_H_
#define MEMCTL__KERNEL_CALL_H_

#include "memctl_types.h"

/*
 * kernel_call_7
 *
 * Description:
 * 	Call a kernel function with the given arguments.
 *
 * Restrictions:
 * 	arg1 must be nonzero.
 * 	The return value is truncated to 32 bits.
 */
extern unsigned (*kernel_call_7)(kaddr_t func,
		kword_t arg1, kword_t arg2, kword_t arg3, kword_t arg4,
		kword_t arg5, kword_t arg6, kword_t arg7);

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

#endif
