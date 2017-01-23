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
 * 	TODO
 *
 * Returns:
 * 	A kernel_io_result.
 *
 * Errors:
 * 	Any non-success result is accompanied by an error. TODO
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
 * 	TODO
 *
 * Returns:
 * 	A kernel_io_result.
 *
 * Errors:
 * 	Any non-success result is accompanied by an error. TODO
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
 */
kernel_io_result kernel_read_word(kernel_read_fn read, kaddr_t kaddr, void *value, size_t width,
		size_t access_width);

/*
 * kernel_write_word
 *
 * Description:
 * 	Write a word of kernel memory using the given write function.
 */
kernel_io_result kernel_write_word(kernel_write_fn write, kaddr_t kaddr, kword_t value,
		size_t width, size_t access_width);

/*
 * kernel_read_unsafe
 *
 * Description:
 * 	A kernel read function that reads directly from kernel memory without performing any safety
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
 * 	A kernel write function that writes directly to kernel memory without performing any safety
 * 	checks.
 */
kernel_io_result kernel_write_unsafe(kaddr_t kaddr, size_t *size, const void *data,
		size_t access_width, kaddr_t *next);

/*
 * kernel_read_heap
 *
 * Description:
 * 	A kernel read function that safely reads from the kernel heap. All non-heap addresses
 * 	return KERNEL_IO_PROTECTION.
 */
kernel_io_result kernel_read_heap(kaddr_t kaddr, size_t *size, void *data, size_t access_width,
		kaddr_t *next);

/*
 * kernel_write_heap
 *
 * Description:
 * 	A kernel write function that safely writes to the kernel heap. All non-heap addresses
 * 	return KERNEL_IO_PROTECTION.
 */
kernel_io_result kernel_write_heap(kaddr_t kaddr, size_t *size, const void *data,
		size_t access_width, kaddr_t *next);

#endif
