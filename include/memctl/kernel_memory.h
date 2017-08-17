#ifndef MEMCTL__KERNEL_MEMORY_H_
#define MEMCTL__KERNEL_MEMORY_H_

#include "memctl/memctl_error.h"

#include <stdint.h>
#include <mach/vm_page_size.h>

// Page-size macros.
#define page_size	vm_kernel_page_size
#define page_shift	vm_kernel_page_shift
#define page_mask	vm_kernel_page_mask

// Machine-independent WIMG bits.
// Defined in osfmk/vm/pmap.h.
#define VM_MEM_GUARDED		0x1
#define VM_MEM_COHERENT		0x2
#define VM_MEM_NOT_CACHEABLE	0x4
#define VM_MEM_WRITE_THROUGH	0x8

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
 * 		address			The kernel virtual address to read.
 * 	inout	size			On entry, the number of bytes to read. On return, the
 * 					number of bytes actually read. This may be smaller than
 * 					the desired number of bytes if an error was encountered.
 * 		data			The buffer in which to store the data.
 * 		access_width		The number of bytes to read at a time, or 0 to let the
 * 					implementation decide the appropriate width. This parameter
 * 					is useful when accessing memory mapped registers, which
 * 					may trigger a panic if accessed with the wrong width.
 * 	out	next			On return, the next address at which a read might
 * 					succeed.
 *
 * Returns:
 * 	A kernel_io_result.
 */
typedef kernel_io_result (*kernel_read_fn)(
		kaddr_t address,
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
 * 		address			The kernel virtual address to write.
 * 	inout	size			On entry, the number of bytes to write. On return, the
 * 					number of bytes actually written. This may be smaller
 * 					than the desired number of bytes if an error was
 * 					encountered.
 * 		data			The data to write.
 * 		access_width		The number of bytes to write at a time, or 0 to let the
 * 					implementation decide the appropriate width. See
 * 					kernel_read_fn.
 * 	out	next			On return, the next address at which a write might
 * 					succeed.
 *
 * Returns:
 * 	A kernel_io_result.
 */
typedef kernel_io_result (*kernel_write_fn)(
		kaddr_t address,
		size_t *size,
		const void *data,
		size_t access_width,
		kaddr_t *next);

/*
 * kernel_allocate
 *
 * Description:
 * 	A wrapper around mach_vm_allocate with kernel_task.
 *
 * Parameters:
 * 	out	address			On return, the address of the allocated region.
 * 		size			The size of the region to allocate.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
bool kernel_allocate(kaddr_t *address, size_t size);

/*
 * kernel_deallocate
 *
 * Description:
 * 	A wrapper around mach_vm_deallocate with kernel_task.
 *
 * Parameters:
 * 		address			The address of the allocated region.
 * 		size			The size of the region to deallocate.
 * 		error			If deallocation fails, generate an error message and fail.
 *
 * Returns:
 * 	True if no errors were encountered. If error is false, then this function always returns
 * 	true.
 */
bool kernel_deallocate(kaddr_t address, size_t size, bool error);

/*
 * kernel_read_word
 *
 * Description:
 * 	Read a word of kernel memory using the given read function.
 *
 * Parameters:
 * 		read			The kernel_read_fn to use.
 * 		address			The kernel virtual address to read.
 * 	out	value			On return, the value read.
 * 		width			The width of the value to read in bytes. width must be
 * 					1, 2, 4, or 8.
 * 		access_width		The access width. See kernel_read_fn.
 *
 * Returns:
 * 	A kernel_io_result.
 */
kernel_io_result kernel_read_word(kernel_read_fn read, kaddr_t address, void *value, size_t width,
		size_t access_width);

/*
 * kernel_write_word
 *
 * Description:
 * 	Write a word of kernel memory using the given write function.
 *
 * Parameters:
 * 		write			The kernel_write_fn to use.
 * 		address			The kernel virtual address to write.
 * 		value			The value to write.
 * 		width			The width of the value to write in bytes. width must be
 * 					1, 2, 4, or 8.
 * 		access_width		The access width. See kernel_read_fn.
 *
 * Returns:
 * 	A kernel_io_result.
 */
kernel_io_result kernel_write_word(kernel_write_fn write, kaddr_t address, kword_t value,
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
extern kernel_read_fn kernel_read_unsafe;

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
extern kernel_write_fn kernel_write_unsafe;

/*
 * kernel_read_heap
 *
 * Description:
 * 	A kernel_read_fn that safely reads from the kernel heap. All non-heap addresses return
 * 	KERNEL_IO_PROTECTION.
 */
extern kernel_read_fn kernel_read_heap;

/*
 * kernel_write_heap
 *
 * Description:
 * 	A kernel_write_fn that safely writes to the kernel heap. All non-heap addresses return
 * 	KERNEL_IO_PROTECTION.
 */
extern kernel_write_fn kernel_write_heap;

/*
 * kernel_read_safe
 *
 * Description:
 * 	A kernel_read_fn that performs safety checks before reading kernel memory. All non-readable
 * 	addresses return KERNEL_IO_PROTECTION.
 */
extern kernel_read_fn kernel_read_safe;

/*
 * kernel_write_safe
 *
 * Description:
 * 	A kernel_write_fn that performs safety checks before writing kernel memory. All
 * 	non-writable addresses return KERNEL_IO_PROTECTION.
 */
extern kernel_write_fn kernel_write_safe;

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
extern kernel_read_fn kernel_read_all;

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
extern kernel_write_fn kernel_write_all;

/*
 * physical_read_unsafe
 *
 * Description:
 * 	A kernel_read_fn that reads physical memory without any safety checks.
 */
extern kernel_read_fn physical_read_unsafe;

/*
 * physical_write_unsafe
 *
 * Description:
 * 	A kernel_write_fn that writes physical memory without any safety checks.
 */
extern kernel_write_fn physical_write_unsafe;

/*
 * physical_read_safe
 *
 * Description:
 * 	A kernel_read_fn that reads physical memory with basic safety checks. All non-accessible
 * 	addresses return KERNEL_IO_PROTECTION.
 */
extern kernel_read_fn physical_read_safe;

/*
 * physical_write_safe
 *
 * Description:
 * 	A kernel_write_fn that writes physical memory with basic safety checks. All non-accessible
 * 	addresses return KERNEL_IO_PROTECTION.
 */
extern kernel_write_fn physical_write_safe;

/*
 * kernel_pmap
 *
 * Description:
 * 	XNU's kernel_pmap. The kernel pmap structure.
 */
extern kaddr_t kernel_pmap;

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
extern bool (*kernel_virtual_to_physical)(kaddr_t kaddr, paddr_t *paddr);

/*
 * zone_element_size
 *
 * Description:
 * 	Get the size of a block of memory allocated with zalloc.
 *
 * Parameters:
 * 		address			The address of the memory block.
 * 	out	size			On return, the allocated size of the memory, or 0 if the
 * 					memory was not allocated with zalloc.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*zone_element_size)(kaddr_t address, size_t *size);

/*
 * pmap_cache_attributes
 *
 * Description:
 * 	XNU's pmap_cache_attributes. Retrieve cache attributes for the specified physical page.
 *
 * Parameters:
 * 	out	cacheattr		The cache attributes bits.
 * 		page			The physical page.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*pmap_cache_attributes)(unsigned int *cacheattr, ppnum_t page);

/*
 * kernel_memory_init
 *
 * Description:
 * 	Initialize the kernel memory functions based on currently available functionality. This
 * 	function can be called multiple times.
 */
void kernel_memory_init(void);

#endif
