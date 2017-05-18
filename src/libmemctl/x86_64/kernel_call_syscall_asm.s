/*
 * x86-64 assembly for kernel_call_syscall_dispatch, the function we will shove into the kernel,
 * and syscall_kernel_call_x86_64, the function we use to invoke the hooked syscall.
 */
#include "x86_64/kernel_call_syscall_code.h"

#define SYSCALL_CLASS_UNIX      2
#define SYSCALL_CLASS_SHIFT     24
#define SYSCALL_CLASS_MASK      (0xFF << SYSCALL_CLASS_SHIFT)
#define SYSCALL_NUMBER_MASK     (~SYSCALL_CLASS_MASK)

#define SYSCALL_CONSTRUCT_UNIX(syscall_number)                  \
	((SYSCALL_CLASS_UNIX << SYSCALL_CLASS_SHIFT) |          \
	 (SYSCALL_NUMBER_MASK & (syscall_number)))

/*
 * _kernel_call_syscall_dispatch
 *
 * Description:
 * 	 The function we will shove into the kernel to be called by our syscall hook. This function
 * 	 transfers control to the first syscall argument, passing the remaining five syscall
 * 	 arguments to the called function.
 */
.globl _kernel_call_syscall_dispatch
.align 8
_kernel_call_syscall_dispatch:
	pushq   %rbx
	movq    %rdx, %rbx
	movq    (%rsi), %rax
	shrq    $48, %rax
	cmpq    $0xffff, %rax
	jne     _kernel_call_syscall_dispatch_abort
	movq    %rsi, %rax
	movq    0x8(%rax), %rdi
	movq    0x10(%rax), %rsi
	movq    0x18(%rax), %rdx
	movq    0x20(%rax), %rcx
	movq    0x28(%rax), %r8
	callq   *(%rax)
	movq    %rax, (%rbx)
_kernel_call_syscall_dispatch_abort:
	xorl    %eax, %eax
	popq    %rbx
	retq

/*
 * _kernel_call_syscall_dispatch_end
 *
 * Description:
 * 	A marker for the end of _kernel_call_syscall_dispatch so that we can determine its size.
 */
.globl _kernel_call_syscall_dispatch_end
_kernel_call_syscall_dispatch_end:

.globl _syscall_kernel_call_x86_64
.align 8
_syscall_kernel_call_x86_64:
	pushq   %rbp
	movq    %rsp, %rbp
	movl    $ SYSCALL_CONSTRUCT_UNIX(SYSCALL_CODE), %eax
	movq    %rcx, %r10
	syscall
	popq    %rbp
	retq
