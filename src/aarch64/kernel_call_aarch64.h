#ifndef MEMCTL__AARCH64__KERNEL_CALL_AARCH64_H_
#define MEMCTL__AARCH64__KERNEL_CALL_AARCH64_H_
/*
 * kernel_call routines for AArch64. These functions should not be called directly.
 */

#include "memctl_types.h"

/*
 * kernel_call_init_aarch64
 *
 * Description:
 * 	Initialize state for kernel_call_aarch64.
 *
 * Dependencies:
 * 	kernelcache
 * 	kernel_slide
 * 	kernel_task
 */
bool kernel_call_init_aarch64(void);

/*
 * kernel_call_deinit_aarch64
 *
 * Description:
 * 	Clean up resources used by kernel_call_init_aarch64.
 */
void kernel_call_deinit_aarch64(void);

/*
 * kernel_call_aarch64
 *
 * Description:
 * 	Call a function in the kernel. See kernel_call.
 *
 * Dependencies:
 * 	kernel_call_7
 */
bool kernel_call_aarch64(void *result, unsigned result_size, unsigned arg_count,
		kaddr_t func, kword_t args[]);

#endif
