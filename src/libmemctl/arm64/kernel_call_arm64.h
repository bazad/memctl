#ifndef MEMCTL__ARM64__KERNEL_CALL_ARM64_H_
#define MEMCTL__ARM64__KERNEL_CALL_ARM64_H_
/*
 * kernel_call routines for arm64. These functions should not be called directly.
 */

#include "memctl/kernel_call.h"

/*
 * kernel_call_init_arm64
 *
 * Description:
 * 	Initialize state for kernel_call_arm64.
 *
 * Dependencies:
 * 	kernelcache
 * 	kernel_slide
 * 	kernel_task
 */
bool kernel_call_init_arm64(void);

/*
 * kernel_call_deinit_arm64
 *
 * Description:
 * 	Clean up resources used by kernel_call_init_arm64.
 */
void kernel_call_deinit_arm64(void);

/*
 * kernel_call_arm64
 *
 * Description:
 * 	Call a function in the kernel. See kernel_call.
 *
 * Dependencies:
 * 	kernel_call_7
 */
bool kernel_call_arm64(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const struct kernel_call_argument args[]);

#endif
