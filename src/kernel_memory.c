#define KERNEL_PRIVATE 1
#include "mach/vm_statistics.h"
#undef KERNEL_PRIVATE

#include "kernel_memory.h"

#include "core.h"
#include "utility.h"

#include <mach/mach_vm.h>
#include <mach/vm_region.h>
#include <unistd.h>

/*
 * transfer_fn
 *
 * Description:
 * 	The type of a function to transfer memory between user space and the kernel.
 */
typedef kernel_io_result (*transfer_fn)(
		kaddr_t kaddr,
		size_t size,
		void *data,
		size_t access,
		bool into_kernel);

/*
 * transfer_range_fn
 *
 * Description:
 * 	The type of a function to find the range of memory that can be transferred between user
 * 	space and the kernel.
 */
typedef kernel_io_result (*transfer_range_fn)(
		kaddr_t kaddr,
		size_t *size,
		size_t *width,
		kaddr_t *next,
		bool into_kernel);

/*
 * mach_unexpected
 *
 * Description:
 * 	Generate an internal error due to the given mach call returning an unexpected error code.
 */
static void
mach_unexpected(const char *function, kern_return_t kr) {
	error_internal("%s returned %d: %s", function, kr, mach_error_string(kr));
}

/*
 * transfer_unsafe
 *
 * Description:
 * 	A transfer function that performs direct memory writes. This is generally unsafe.
 */
static kernel_io_result
transfer_unsafe(kaddr_t kaddr, size_t size, void *data, size_t access, bool into_kernel) {
	uint8_t *p = (uint8_t *)data;
	while (size > 0) {
		size_t copysize = min(size, page_size - (kaddr & page_mask));
		if (access < sizeof(kword_t) && access < copysize) {
			copysize = access;
		}
		kern_return_t kr;
		if (into_kernel) {
			kr = mach_vm_write(kernel_task, kaddr, (vm_offset_t)p, copysize);
		} else {
			mach_vm_size_t out_size = copysize;
			kr = mach_vm_read_overwrite(kernel_task, kaddr, copysize,
					(mach_vm_address_t)p, &out_size);
		}
		if (kr != KERN_SUCCESS) {
			if (kr == KERN_PROTECTION_FAILURE) {
				return KERNEL_IO_PROTECTION;
			} else {
				const char *fn = (into_kernel ? "mach_vm_write"
				                              : "mach_vm_read_overwrite");
				mach_unexpected(fn, kr);
				return KERNEL_IO_ERROR;
			}
		}
		kaddr += copysize;
		p += copysize;
		size -= copysize;
	}
	return KERNEL_IO_SUCCESS;
}

/*
 * transfer_range_unsafe
 *
 * Description:
 * 	A transfer range function that assumes the whole range to be transferred is valid. This is
 * 	generally unsafe.
 */
static kernel_io_result
transfer_range_unsafe(kaddr_t kaddr, size_t *size, size_t *access, kaddr_t *next, bool into_kernel) {
	if (next != NULL) {
		*next = kaddr + *size;
	}
	if (*access == 0) {
		*access = sizeof(kword_t);
	}
	return KERNEL_IO_SUCCESS;
}

/*
 * region_is_heap
 *
 * Description:
 * 	Returns whether the region is on the heap.
 */
static bool
region_is_heap(vm_region_submap_short_info_64_t info) {
	int prot = VM_PROT_READ | VM_PROT_WRITE;
	return ((info->protection & prot) == prot
		&& info->share_mode != SM_EMPTY
		&& (info->user_tag == VM_KERN_MEMORY_ZONE
		    || info->user_tag == VM_KERN_MEMORY_KALLOC));
}

/*
 * transfer_range_heap
 *
 * Description:
 * 	Find the transfer range for the given transfer, but only consider heap regions.
 */
static kernel_io_result
transfer_range_heap(kaddr_t kaddr, size_t *size, size_t *access, kaddr_t *next, bool into_kernel) {
	size_t left = *size;
	size_t transfer_size = 0;
	kaddr_t next_viable = kaddr + left;
	kernel_io_result result = KERNEL_IO_SUCCESS;
	while (left > 0) {
		mach_vm_address_t address = kaddr;
		mach_vm_size_t size = 0;
		uint32_t depth = 2048;
		vm_region_submap_short_info_data_64_t info;
		mach_msg_type_number_t count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
		kern_return_t kr = mach_vm_region_recurse(kernel_task, &address, &size,
				&depth, (vm_region_recurse_info_t)&info, &count);
		if (kr != KERN_SUCCESS) {
			if (kr == KERN_INVALID_ADDRESS) {
				next_viable = 0;
				result = KERNEL_IO_UNMAPPED;
				break;
			}
			next_viable = kaddr + page_size - (kaddr & page_mask);
			mach_unexpected("mach_vm_region_recurse", kr);
			result = KERNEL_IO_ERROR;
			break;
		}
		bool viable = region_is_heap(&info);
		if (address > kaddr) {
			result = KERNEL_IO_UNMAPPED;
			if (viable) {
				next_viable = address;
			} else {
				next_viable = address + size;
			}
			break;
		}
		if (!viable) {
			result = KERNEL_IO_PROTECTION;
			next_viable = address + size;
			break;
		}
		// Incorporate the region.
		size_t region_size = min(left, size - (kaddr - address));
		kaddr += region_size;
		left -= region_size;
		transfer_size += region_size;
	}
	*size = transfer_size;
	if (next != NULL) {
		*next = next_viable;
	}
	if (*access == 0) {
		*access = sizeof(kword_t);
	}
	return result;
}

/*
 * kernel_io
 *
 * Description:
 * 	Run the transfer between user space and the kernel with the given transfer functions.
 */
static kernel_io_result
kernel_io(kaddr_t kaddr, size_t *size, void *data, size_t access, kaddr_t *next,
		transfer_range_fn transfer_range, transfer_fn transfer, bool write) {
	kernel_io_result result = KERNEL_IO_SUCCESS;
	size_t left = *size;
	size_t total = 0;
	uint8_t *p = (uint8_t *)data;
	while (left > 0) {
		size_t transfer_size = left;
		size_t transfer_access = access;
		result = transfer_range(kaddr, &transfer_size, &transfer_access, next, write);
		if (result != KERNEL_IO_SUCCESS) {
			left = transfer_size;
		}
		kernel_io_result result2 = transfer(kaddr, transfer_size, p, transfer_access,
				write);
		if (result2 != KERNEL_IO_SUCCESS) {
			result = result2;
			break;
		}
		total += transfer_size;
		kaddr += transfer_size;
		left -= transfer_size;
		p += transfer_size;
	}
	switch (result) {
		case KERNEL_IO_SUCCESS:                                         break;
		case KERNEL_IO_ERROR:        error_kernel_io(kaddr);            break;
		case KERNEL_IO_PROTECTION:   error_address_protection(kaddr);   break;
		case KERNEL_IO_UNMAPPED:     error_address_unmapped(kaddr);     break;
		case KERNEL_IO_INACCESSIBLE: error_address_inaccessible(kaddr); break;
	}
	*size = total;
	return result;
}

kernel_io_result
kernel_read_unsafe(kaddr_t kaddr, size_t *size, void *data, size_t access_width, kaddr_t *next) {
	return kernel_io(kaddr, size, data, access_width, next, transfer_range_unsafe,
			transfer_unsafe, false);
}

kernel_io_result
kernel_write_unsafe(kaddr_t kaddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(kaddr, size, (void *)data, access_width, next, transfer_range_unsafe,
			transfer_unsafe, true);
}

kernel_io_result
kernel_read_heap(kaddr_t kaddr, size_t *size, void *data, size_t access_width, kaddr_t *next) {
	return kernel_io(kaddr, size, data, access_width, next, transfer_range_heap,
			transfer_unsafe, false);
}

kernel_io_result
kernel_write_heap(kaddr_t kaddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(kaddr, size, (void *)data, access_width, next, transfer_range_heap,
			transfer_unsafe, true);
}

kernel_io_result
kernel_read_word(kernel_read_fn read, kaddr_t kaddr, void *value, size_t width,
		size_t access_width) {
	return read(kaddr, &width, value, access_width, NULL);
}

kernel_io_result
kernel_write_word(kernel_write_fn write, kaddr_t kaddr, kword_t value, size_t width, size_t access_width) {
	pack_uint(&value, value, width);
	return write(kaddr, &width, &value, access_width, NULL);
}
