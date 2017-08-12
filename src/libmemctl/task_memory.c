#include "memctl/task_memory.h"

#include "memctl/memctl_error.h"
#include "memctl/utility.h"

#include <mach/mach_error.h>
#include <mach/mach_vm.h>
#include <mach/vm_page_size.h>
#include <mach/vm_region.h>

/*
 * mach_unexpected
 *
 * Description:
 * 	Generate an internal error due to the given mach call returning an unexpected error code.
 */
static void
mach_unexpected(bool error, const char *function, kern_return_t kr) {
	if (error) {
		error_internal("%s returned %d: %s", function, kr, mach_error_string(kr));
	} else {
		memctl_warning("%s returned %d: %s", function, kr, mach_error_string(kr));
	}
}

task_io_result
task_transfer_range_all(task_t task, mach_vm_address_t address, size_t *size, size_t *access,
		mach_vm_address_t *next, bool is_write) {
	if (next != NULL) {
		*next = address + *size;
	}
	return TASK_IO_SUCCESS;
}

task_io_result
task_transfer(vm_map_t task, mach_vm_address_t address, size_t *size, void *data, size_t access,
		bool is_write) {
	uint8_t *p = (uint8_t *)data;
	size_t left = *size;
	while (left > 0) {
		// Only transfer up to a page boundary at a time.
		size_t transfer_size = vm_kernel_page_size - (address & vm_kernel_page_mask);
		if (left < transfer_size) {
			transfer_size = left;
		}
		if (access != 0 && access < transfer_size) {
			transfer_size = access;
		}
		kern_return_t kr;
		// Unfortunately, there's no way to tell how much of the write succeeded for
		// mach_vm_write. We're relying on the page-at-a-time looping behavior to guarantee
		// that writes fail at a page boundary.
		mach_vm_size_t out_size = 0;
		if (is_write) {
			kr = mach_vm_write(task, address, (vm_offset_t)p, transfer_size);
		} else {
			out_size = transfer_size;
			kr = mach_vm_read_overwrite(task, address, transfer_size,
					(mach_vm_address_t)p, &out_size);
		}
		if (kr != KERN_SUCCESS) {
			*size = out_size;
			if (kr == KERN_PROTECTION_FAILURE) {
				return TASK_IO_PROTECTION;
			} else {
				const char *fn = (is_write ? "mach_vm_write"
				                           : "mach_vm_read_overwrite");
				mach_unexpected(true, fn, kr);
				return TASK_IO_ERROR;
			}
		}
		address += transfer_size;
		p       += transfer_size;
		left    -= transfer_size;
	}
	return TASK_IO_SUCCESS;
}

task_io_result
task_perform_transfer(vm_map_t task, mach_vm_address_t address, size_t *size, void *data,
		size_t access, mach_vm_address_t *next, task_transfer_range_fn transfer_range,
		task_transfer_fn transfer, bool is_write) {
	task_io_result result = TASK_IO_SUCCESS;
	size_t left = *size;
	mach_vm_address_t start = address;
	uint8_t *p = (uint8_t *)data;
	while (left > 0) {
		size_t transfer_size = left;
		size_t transfer_access = access;
		result = transfer_range(task, address, &transfer_size, &transfer_access, next,
				is_write);
		if (result != TASK_IO_SUCCESS) {
			if (transfer_size == 0) {
				break;
			}
			left = transfer_size;
		}
		task_io_result result2 = transfer(task, address, &transfer_size, p, transfer_access,
				is_write);
		address += transfer_size;
		left    -= transfer_size;
		p       += transfer_size;
		if (result2 != TASK_IO_SUCCESS) {
			result = result2;
			break;
		}
	}
	*size = address - start;
	return result;
}

task_io_result
task_read_word(task_read_fn read, vm_map_t task, mach_vm_address_t address, void *value,
		size_t width, size_t access_width) {
	return read(task, address, &width, value, access_width, NULL);
}

task_io_result
task_write_word(task_write_fn write, vm_map_t task, mach_vm_address_t address,
		mach_vm_offset_t value, size_t width, size_t access_width) {
	pack_uint(&value, value, width);
	return write(task, address, &width, &value, access_width, NULL);
}

task_io_result task_read(vm_map_t task, mach_vm_address_t address, size_t *size, void *data,
		size_t access_width, mach_vm_address_t *next) {
	return task_perform_transfer(task, address, size, data, access_width, next,
			task_transfer_range_all, task_transfer, false);
}

task_io_result task_write(vm_map_t task, mach_vm_address_t address, size_t *size, const void *data,
		size_t access_width, mach_vm_address_t *next) {
	return task_perform_transfer(task, address, size, (void *)data, access_width, next,
			task_transfer_range_all, task_transfer, true);
}
