#ifndef MEMCTL__KERNEL_MEMORY_H_
#define MEMCTL__KERNEL_MEMORY_H_

#include <stdint.h>
#include <mach/vm_page_size.h>

#include "memctl_error.h"

#define page_size	vm_kernel_page_size
#define page_shift	vm_kernel_page_shift
#define page_mask	vm_kernel_page_mask

/*
 * kernel_io_result
 *
 * Description:
 * 	The result of a kernel I/O operation.
 */
typedef enum {
	KERNEL_IO_SUCCESS,
	KERNEL_IO_ERROR,
	KERNEL_IO_PROTECTION,
	KERNEL_IO_UNMAPPED,
	KERNEL_IO_INACCESSIBLE,
} kernel_io_result;

/*
 * kernel_read_fn
 *
 * Description:
 * 	The type of a function to read kernel memory into user space.
 *
 * Parameters:
 * 		kaddr			The kernel virtual address to read
 * 	inout	size			On entry, the number of bytes to read. On return, the
 * 					number of bytes actually read. This may be smaller than
 * 					the desired number of bytes if an error was encountered.
 * 		data			The buffer in which to store the data.
 * 		access_width		The number of bytes to read at a time, or 0 to let
 * 					kernel_read decide the appropriate width. This parameter
 * 					is useful when accessing memory mapped registers, which
 * 					may trigger a panic if accessed with the wrong width.
 * 	out	next			On return, the next address at which a read might
 * 					succeed.
 *
 * Returns:
 * 	A kernel_io_result.
 *
 * Errors:
 * 	Any non-success result is accompanied by an error.
 */
typedef kernel_io_result (*kernel_read_fn)(
		kaddr_t kaddr,
		size_t *size,
		void *data,
		size_t access_width,
		kaddr_t *next);

/*
 * kernel_write_fn
 *
 * Description:
 * 	The type of a function to write data into kernel memory.
 *
 * Parameters:
 * 		kaddr			The kernel virtual address to write.
 * 	inout	size			On entry, the number of bytes to write. On return, the
 * 					number of bytes actually written. This may be smaller
 * 					than the desired number of bytes if an error was
 * 					encountered.
 * 		data			The data to write.
 * 		access_width		The number of bytes to write at a time, or 0 to let
 * 					kernel_write decide the appropriate width. See
 * 					kernel_read.
 * 	out	next			On return, the next address at which a write might
 * 					succeed.
 *
 * Returns:
 * 	A kernel_io_result.
 *
 * Errors:
 * 	Any non-success result is accompanied by an error.
 */
typedef kernel_io_result (*kernel_write_fn)(
		kaddr_t kaddr,
		size_t *size,
		const void *data,
		size_t access_width,
		kaddr_t *next);

/*
 * kernel_read_word
 *
 * Description:
 * 	Read a word of kernel memory using the given read function.
 *
 * Parameters:
 * 		read			The kernel_read_fn to use.
 * 		kaddr			The kernel virtual address to read.
 * 	out	value			On return, the value read.
 * 		width			The width of the value to read in bytes. width must be
 * 					1, 2, 4, or 8.
 * 		access_width		The access width. See kernel_read_fn.
 *
 * Returns:
 * 	A kernel_io_result.
 */
kernel_io_result kernel_read_word(kernel_read_fn read, kaddr_t kaddr, void *value, size_t width,
		size_t access_width);

/*
 * kernel_write_word
 *
 * Description:
 * 	Write a word of kernel memory using the given write function.
 *
 * Parameters:
 * 		write			The kernel_write_fn to use.
 * 		kaddr			The kernel virtual address to write
 * 	out	value			The value to write
 * 		width			The width of the value to write in bytes. width must be
 * 					1, 2, 4, or 8.
 * 		access_width		The access width. See kernel_read.
 *
 * Returns:
 * 	A kernel_io_result.
 */
kernel_io_result kernel_write_word(kernel_write_fn write, kaddr_t kaddr, kword_t value,
		size_t width, size_t access_width);

/*
 * kernel_read_unsafe
 *
 * Description:
 * 	A kernel_read_fn that reads directly from kernel memory without performing any safety
 * 	checks.
 *
 * Notes:
 * 	This is generally unsafe and can cause the system to panic.
 */
kernel_io_result kernel_read_unsafe(kaddr_t kaddr, size_t *size, void *data, size_t access_width,
		kaddr_t *next);

/*
 * kernel_write_unsafe
 *
 * Description:
 * 	A kernel_write_fn that writes directly to kernel memory without performing any safety
 * 	checks.
 *
 * Notes:
 * 	See kernel_read_unsafe.
 */
kernel_io_result kernel_write_unsafe(kaddr_t kaddr, size_t *size, const void *data,
		size_t access_width, kaddr_t *next);

/*
 * kernel_read_heap
 *
 * Description:
 * 	A kernel_read_fn that safely reads from the kernel heap. All non-heap addresses return
 * 	KERNEL_IO_PROTECTION.
 */
kernel_io_result kernel_read_heap(kaddr_t kaddr, size_t *size, void *data, size_t access_width,
		kaddr_t *next);

/*
 * kernel_write_heap
 *
 * Description:
 * 	A kernel_write_fn that safely writes to the kernel heap. All non-heap addresses return
 * 	KERNEL_IO_PROTECTION.
 */
kernel_io_result kernel_write_heap(kaddr_t kaddr, size_t *size, const void *data,
		size_t access_width, kaddr_t *next);

/*
 * kernel_read_safe
 *
 * Description:
 * 	A kernel_read_fn that performs safety checks before reading kernel memory. All non-readable
 * 	addresses return KERNEL_IO_PROTECTION.
 */
kernel_io_result kernel_read_safe(kaddr_t kaddr, size_t *size, void *data, size_t access_width,
		kaddr_t *next);

/*
 * kernel_write_safe
 *
 * Description:
 * 	A kernel_write_fn that performs safety checks before writing kernel memory. All
 * 	non-writable addresses return KERNEL_IO_PROTECTION.
 */
kernel_io_result kernel_write_safe(kaddr_t kaddr, size_t *size, const void *data,
		size_t access_width, kaddr_t *next);

/*
 * kernel_read_all
 *
 * Description:
 * 	A kernel_read_fn that tries to read all memory addresses that look safe. All non-readable
 * 	addresses return KERNEL_IO_PROTECTION.
 *
 * Notes:
 * 	While the goal is for this function to be safe, it is more likely to trigger a panic than
 * 	kernel_read_safe. It is also slower than kernel_read_safe because it does not rely on the
 * 	kernel virtual memory map to determine whether an address is mapped.
 */
kernel_io_result kernel_read_all(kaddr_t kaddr, size_t *size, void *data, size_t access_width,
		kaddr_t *next);

/*
 * kernel_write_all
 *
 * Description:
 * 	A kernel_write_fn that tries to write all memory addresses that look safe. All non-writable
 * 	addresses return KERNEL_IO_PROTECTION.
 *
 * Notes:
 * 	See kernel_read_all.
 */
kernel_io_result kernel_write_all(kaddr_t kaddr, size_t *size, const void *data,
		size_t access_width, kaddr_t *next);

/*
 * kernel_virtual_to_physical
 *
 * Description:
 * 	Convert a kernel virtual address into a physical address.
 *
 * Parameters:
 * 		kaddr			The kernel virtual address.
 * 	out	paddr			On return, the physical address corresponding to the
 * 					virtual address, or 0 if the virtual address is unmapped.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
bool kernel_virtual_to_physical(kaddr_t kaddr, paddr_t *paddr);

#endif
