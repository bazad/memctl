#ifndef MEMCTL__TASK_MEMORY_H_
#define MEMCTL__TASK_MEMORY_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <mach/mach_vm.h>

/*
 * task_io_result
 *
 * Description:
 * 	The result of a task memory operation.
 */
typedef enum {
	TASK_IO_SUCCESS,
	TASK_IO_ERROR,
	TASK_IO_PROTECTION,
	TASK_IO_UNMAPPED,
	TASK_IO_INACCESSIBLE,
} task_io_result;

/*
 * task_read_fn
 *
 * Description:
 * 	The type of a function to read memory from a task.
 *
 * Parameters:
 * 		task			The task from which to read memory.
 * 		address			The task virtual address to read.
 * 	inout	size			On entry, the number of bytes to read. On return, the
 * 					number of bytes actually read. This may be smaller than
 * 					the desired number of bytes if an error was encountered.
 * 		data			The buffer in which to store the data.
 * 		access_width		The number of bytes to read at a time, or 0 to let the
 * 					implementation decide the appropriate width. This parameter
 * 					is useful when accessing memory mapped registers, which
 * 					may trigger a kernel panic if accessed with the wrong
 * 					width.
 * 	out	next			On return, the next address at which a read might
 * 					succeed.
 *
 * Returns:
 * 	A task_io_result code.
 */
typedef task_io_result (*task_read_fn)(
		vm_map_t task,
		mach_vm_address_t address,
		size_t *size,
		void *data,
		size_t access_width,
		mach_vm_address_t *next);

/*
 * task_write_fn
 *
 * Description:
 * 	The type of a function to write data into a task's memory.
 *
 * Parameters:
 * 		task			The task in which to write memory.
 * 		address			The task virtual address to write.
 * 	inout	size			On entry, the number of bytes to write. On return, the
 * 					number of bytes actually written. This may be smaller
 * 					than the desired number of bytes if an error was
 * 					encountered.
 * 		data			The data to write.
 * 		access_width		The number of bytes to write at a time, or 0 to let the
 * 					implementation decide the appropriate width. See
 * 					task_read_fn.
 * 	out	next			On return, the next address at which a write might
 * 					succeed.
 *
 * Returns:
 * 	A task_io_result code.
 */
typedef task_io_result (*task_write_fn)(
		vm_map_t task,
		mach_vm_address_t address,
		size_t *size,
		const void *data,
		size_t access_width,
		mach_vm_address_t *next);

/*
 * task_transfer_range_fn
 *
 * Description:
 * 	The type of a function to find the range of memory that can be transferred between the
 * 	current task and the target task, as used by task_perform_transfer.
 *
 * Parameters:
 * 		task			The target task.
 * 		address			The virtual address of the start of the transfer.
 * 	inout	size			On entry, the number of bytes to transfer. On return, the
 * 					number of bytes that can be transferred.
 * 	inout	access_width		On entry, the desired access width. On return, the largest
 * 					permissible access width.
 * 	out	next			On return, the next virtual address at which this transfer
 * 					could succeed.
 * 		is_write		True if this transfer is a write.
 *
 * Returns:
 * 	A task_io_result code.
 */
typedef task_io_result (*task_transfer_range_fn)(
		vm_map_t task,
		mach_vm_address_t address,
		size_t *size,
		size_t *access_width,
		mach_vm_address_t *next,
		bool is_write);

/*
 * task_transfer_fn
 *
 * Description:
 * 	The type of a function to transfer memory between the current task and the target task, as
 * 	used by task_perform_transfer.
 *
 * Parameters:
 * 		task			The target task.
 * 		address			The virtual address of the start of the transfer.
 * 	inout	size			On entry, the number of bytes to transfer. On return, the
 * 					number of bytes that can be transferred.
 * 		data			The data to transfer.
 * 		access_width		The access width of the transfer, or 0 to use the default.
 * 		is_write		True if this transfer is a write.
 *
 * Returns:
 * 	A task_io_result code.
 */
typedef task_io_result (*task_transfer_fn)(
		vm_map_t task,
		mach_vm_address_t address,
		size_t *size,
		void *data,
		size_t access_width,
		bool is_write);

/*
 * task_transfer_range_all
 *
 * Description:
 * 	A task_transfer_range_fn that designates the entire region as safe to transfer.
 */
task_io_result task_transfer_range_all(task_t task, mach_vm_address_t address, size_t *size,
		size_t *access, mach_vm_address_t *next, bool is_write);

/*
 * task_transfer
 *
 * Description:
 * 	A task_transfer_fn that reads or writes task memory using mach_vm_read_overwrite or
 * 	mach_vm_write.
 */
task_io_result task_transfer(vm_map_t task, mach_vm_address_t address, size_t *size, void *data,
		size_t access, bool is_write);

/*
 * task_perform_transfer
 *
 * Description:
 * 	Run the transfer between the current task and the target task with the given transfer
 * 	functions.
 *
 * Parameters:
 * 		task			The target task.
 * 		address			The virtual address of the start of the transfer.
 * 	inout	size			On entry, the number of bytes to transfer. On return, the
 * 					number of bytes successfully transferred.
 * 		data			The data to transfer.
 * 		access_width		The access width of the transfer, or 0 to use the default.
 * 	out	next			On return, the next virtual address at which this transfer
 * 					could succeed.
 * 		transfer_range		A function to find the range of virtual memory on which to
 * 					attempt a transfer.
 * 		transfer		A function to perform the virtual memory transfer.
 * 		is_write		True if this transfer is a write.
 *
 * Returns:
 * 	A task_io_result code.
 */
task_io_result task_perform_transfer(vm_map_t task, mach_vm_address_t address, size_t *size,
		void *data, size_t access_width, mach_vm_address_t *next,
		task_transfer_range_fn transfer_range, task_transfer_fn transfer, bool is_write);
/*
 * task_read_word
 *
 * Description:
 * 	Read a word from the task's memory using the given read function.
 *
 * Parameters:
 * 		read			The task_read_fn to use.
 * 		task			The task from which to read memory.
 * 		address			The virtual address to read.
 * 	out	value			On return, the value read.
 * 		width			The width of the value to read in bytes. width must be
 * 					1, 2, 4, or 8.
 * 		access_width		The access width. See task_read_fn.
 *
 * Returns:
 * 	A task_io_result code.
 */
task_io_result task_read_word(task_read_fn read, vm_map_t task, mach_vm_address_t address,
		void *value, size_t width, size_t access_width);

/*
 * task_write_word
 *
 * Description:
 * 	Write a word to the task's memory using the given write function.
 *
 * Parameters:
 * 		write			The task_write_fn to use.
 * 		task			The task in which to write memory.
 * 		address			The virtual address to write.
 * 		value			The value to write.
 * 		width			The width of the value to write in bytes. width must be
 * 					1, 2, 4, or 8.
 * 		access_width		The access width. See task_read_fn.
 *
 * Returns:
 * 	A task_io_result code.
 */
task_io_result task_write_word(task_write_fn write, vm_map_t task, mach_vm_address_t address,
		mach_vm_offset_t value, size_t width, size_t access_width);

/*
 * task_read
 *
 * Description:
 * 	A task_read_fn function.
 */
task_io_result task_read(vm_map_t task, mach_vm_address_t address, size_t *size, void *data,
		size_t access_width, mach_vm_address_t *next);

/*
 * task_write
 *
 * Description:
 * 	A task_write_fn function.
 */
task_io_result task_write(vm_map_t task, mach_vm_address_t address, size_t *size, const void *data,
		size_t access_width, mach_vm_address_t *next);

#endif
