#ifndef MEMCTL__X86_64__KERNEL_CALL_SYSCALL_X86_64_H_
#define MEMCTL__X86_64__KERNEL_CALL_SYSCALL_X86_64_H_
/*
 * kernel_call routines for x86-64. Implemented using a system call hook.
 */

#include "memctl/kernel_call.h"
#include "memctl/kernel_memory.h"

/*
 * To use this module in a core, specify implementations for kernel_read_text and
 * kernel_write_text. These functions need not satisfy the full specification: they will always be
 * called with access_width = 0 and next = NULL.
 */
extern kernel_read_fn  kernel_read_text;
extern kernel_write_fn kernel_write_text;

/*
 * kernel_call_init_syscall_x86_64
 *
 * Description:
 * 	Initialize state for kernel_call_syscall_x86_64.
 *
 * Dependencies:
 * 	kernel image
 * 	kernel_slide
 * 	kernel_read_text/kernel_write_text OR kernel_call_7/physical_write_unsafe
 */
bool kernel_call_init_syscall_x86_64(void);

/*
 * kernel_call_deinit_syscall_x86_64
 *
 * Description:
 * 	Clean up resources used by kernel_call_init_syscall_x86_64.
 */
void kernel_call_deinit_syscall_x86_64(void);

/*
 * kernel_call_syscall_x86_64
 *
 * Description:
 * 	Call a function in the kernel. See kernel_call.
 */
bool kernel_call_syscall_x86_64(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const struct kernel_call_argument args[]);

/*
 * syscall_kernel_call_x86_64
 *
 * Description:
 * 	The underlying kernel_call implementation for kernel_call_syscall_x86_64. Only call this
 * 	function when the kernel_call_syscall_x86_64 subsystem has been initialized.
 */
uint64_t syscall_kernel_call_x86_64(uint64_t func,
		uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif
