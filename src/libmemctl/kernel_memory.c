#define KERNEL_PRIVATE 1
#include "mach/vm_statistics.h"
#undef KERNEL_PRIVATE

#include "memctl/kernel_memory.h"

#include "memctl/core.h"
#include "memctl/kernel.h"
#include "memctl/kernel_call.h"
#include "memctl/memory_region.h"
#include "memctl/task_memory.h"
#include "memctl/utility.h"

#include <mach/mach_vm.h>
#include <mach/vm_region.h>
#include <unistd.h>


// The read/write functions.
kernel_read_fn  kernel_read_unsafe;
kernel_write_fn kernel_write_unsafe;
kernel_read_fn  kernel_read_heap;
kernel_write_fn kernel_write_heap;
kernel_read_fn  kernel_read_safe;
kernel_write_fn kernel_write_safe;
kernel_read_fn  kernel_read_all;
kernel_write_fn kernel_write_all;
kernel_read_fn  physical_read_unsafe;
kernel_write_fn physical_write_unsafe;

// Other functions.
bool (*kernel_virtual_to_physical)(kaddr_t kaddr, paddr_t *paddr);
bool (*zone_element_size)(kaddr_t kaddr, size_t *size);

// pmap_t kernel_pmap;
kaddr_t kernel_pmap;

// ppnum_t pmap_find_phys(pmap_t map, addr64_t va);
static kaddr_t _pmap_find_phys;

// UInt<N> IOMappedRead<N>(IOPhysicalAddress address)
static kaddr_t _IOMappedRead8;
static kaddr_t _IOMappedRead16;
static kaddr_t _IOMappedRead32;
static kaddr_t _IOMappedRead64;

// void IOMappedWrite<N>(IOPhysicalAddress address, UInt<N> value)
static kaddr_t _IOMappedWrite8;
static kaddr_t _IOMappedWrite16;
static kaddr_t _IOMappedWrite32;
static kaddr_t _IOMappedWrite64;

// vm_size_t zone_element_size(void *addr, zone_t *z)
static kaddr_t _zone_element_size;

#define ERROR_CALL(symbol)	error_internal("could not call %s", #symbol)

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

bool
kernel_allocate(kaddr_t *addr, size_t size) {
	mach_vm_address_t address = 0;
	kern_return_t kr = mach_vm_allocate(kernel_task, &address, size, VM_FLAGS_ANYWHERE);
	if (kr != KERN_SUCCESS) {
		mach_unexpected(true, "mach_vm_allocate", kr);
		return false;
	}
	*addr = address;
	return true;
}

void
kernel_deallocate(kaddr_t addr, size_t size) {
	kern_return_t kr = mach_vm_deallocate(kernel_task, addr, size);
	if (kr != KERN_SUCCESS) {
		mach_unexpected(false, "mach_vm_deallocate", kr);
	}
}

/*
 * physical_word_read_unsafe
 *
 * Description:
 * 	Read a word of physical memory.
 */
static bool
physical_word_read_unsafe(paddr_t paddr, void *data, size_t logsize) {
	assert(logsize <= 3);
	const kaddr_t fn[4] = {
		_IOMappedRead8, _IOMappedRead16, _IOMappedRead32, _IOMappedRead64
	};
	bool success = kernel_call(data, 1 << logsize, fn[logsize], 1, &paddr);
	if (!success) {
		error_internal("could not read physical address 0x%llx", (long long)paddr);
	}
	return success;
}

/*
 * physical_word_write_unsafe
 *
 * Description:
 * 	Write a word of physical memory.
 */
static bool
physical_word_write_unsafe(paddr_t paddr, uint64_t data, size_t logsize) {
	assert(logsize <= 3);
	const kaddr_t fn[4] = {
		_IOMappedWrite8, _IOMappedWrite16, _IOMappedWrite32, _IOMappedWrite64
	};
	kword_t args[2] = { paddr, data };
	bool success = kernel_call(NULL, 0, fn[logsize], 2, args);
	if (!success) {
		error_internal("could not write physical address 0x%llx", (long long)paddr);
	}
	return success;
}

/*
 * transfer_physical_words_unsafe
 *
 * Description:
 * 	A transfer function that performs direct physical memory reads and writes using word-sized
 * 	transfers. This is generally unsafe.
 *
 * Notes:
 * 	Since at most 8 bytes can be transferred per kernel call, this operation is slow.
 *
 * 	If access is 8 but we are reading from the kernel and 8-byte return values are not
 * 	supported, then memory will silently be read with width 4.
 */
static task_io_result
transfer_physical_words_unsafe(vm_map_t task, mach_vm_address_t paddr, size_t *size, void *data,
		size_t access, bool is_write) {
	mach_vm_address_t start = paddr;
	size_t left = *size;
	uint8_t *p = (uint8_t *)data;
	kword_t dummy_args[] = { 1 };
	bool trunc_32 = !is_write && !kernel_call(NULL, sizeof(uint64_t), 0, 1, dummy_args);
	task_io_result result = TASK_IO_SUCCESS;
	while (left > 0) {
		size_t wordsize = min(left, sizeof(kword_t) - (paddr % sizeof(kword_t)));
		if (access != 0 && access < wordsize) {
			wordsize = access;
		}
		if (wordsize > sizeof(kword_t)) {
			wordsize = sizeof(kword_t);
		}
		if (wordsize == sizeof(uint64_t) && trunc_32) {
			wordsize = sizeof(uint32_t);
		}
		size_t logsize = ilog2(wordsize);
		wordsize = 1 << logsize;
		bool success;
		if (is_write) {
			success = physical_word_write_unsafe(paddr, unpack_uint(p, wordsize),
					logsize);
		} else {
			success = physical_word_read_unsafe(paddr, p, logsize);
		}
		if (!success) {
			result = TASK_IO_ERROR;
			break;
		}
		paddr += wordsize;
		p     += wordsize;
		left  -= wordsize;
	}
	*size = paddr - start;
	return result;
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
static task_io_result
transfer_range_heap(vm_map_t task, mach_vm_address_t kaddr, size_t *size, size_t *access,
		kaddr_t *next, bool is_write) {
	size_t left = *size;
	size_t transfer_size = 0;
	kaddr_t next_viable = kaddr + left;
	task_io_result result = TASK_IO_SUCCESS;
	while (left > 0) {
		mach_vm_address_t address = kaddr;
		mach_vm_size_t vmsize = 0;
		uint32_t depth = 2048;
		vm_region_submap_short_info_data_64_t info;
		mach_msg_type_number_t count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
		kern_return_t kr = mach_vm_region_recurse(task, &address, &vmsize, &depth,
				(vm_region_recurse_info_t)&info, &count);
		if (kr != KERN_SUCCESS) {
			if (kr == KERN_INVALID_ADDRESS) {
				// We've reached the end of the kernel memory map.
				next_viable = 0;
				result = TASK_IO_UNMAPPED;
				break;
			}
			next_viable = (kaddr & ~page_mask) + page_size;
			mach_unexpected(true, "mach_vm_region_recurse", kr);
			result = TASK_IO_ERROR;
			break;
		}
		bool viable = region_is_heap(&info);
		if (address > kaddr) {
			result = TASK_IO_UNMAPPED;
			if (viable) {
				next_viable = address;
			} else {
				next_viable = address + vmsize;
			}
			break;
		}
		if (!viable) {
			result = TASK_IO_PROTECTION;
			next_viable = address + vmsize;
			break;
		}
		// Incorporate the region.
		size_t region_size = min(left, vmsize - (kaddr - address));
		kaddr         += region_size;
		left          -= region_size;
		transfer_size += region_size;
	}
	*size = transfer_size;
	if (next != NULL) {
		*next = next_viable;
	}
	return result;
}

/*
 * region_looks_safe
 *
 * Description:
 * 	Returns whether the region looks safe for the given operation.
 */
static bool
region_looks_safe(vm_region_submap_short_info_64_t info, bool is_write) {
	int prot = (is_write ? VM_PROT_WRITE : VM_PROT_READ);
	return ((info->protection & prot) == prot
	        && info->share_mode != SM_EMPTY);
}

/*
 * transfer_range_safe
 *
 * Description:
 * 	Find the transfer range for the given transfer, but only consider safe regions. Needs
 * 	kernel_virtual_to_physical.
 */
static task_io_result
transfer_range_safe(vm_map_t task, mach_vm_address_t kaddr, size_t *size, size_t *access,
		mach_vm_address_t *next, bool is_write) {
	assert(kernel_virtual_to_physical != NULL);
	size_t left = *size;
	size_t transfer_size = 0;
	mach_vm_address_t next_viable = kaddr + left;
	task_io_result result = TASK_IO_SUCCESS;
	while (result == TASK_IO_SUCCESS && left > 0) {
		// First, check if the virtual memory region looks viable. If not, then abort the
		// loop, since no data can be transferred from address kaddr.
		mach_vm_address_t address = kaddr;
		mach_vm_size_t vmsize = 0;
		uint32_t depth = 2048;
		vm_region_submap_short_info_data_64_t info;
		mach_msg_type_number_t count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
		kern_return_t kr = mach_vm_region_recurse(task, &address, &vmsize, &depth,
				(vm_region_recurse_info_t)&info, &count);
		if (kr != KERN_SUCCESS) {
			if (kr == KERN_INVALID_ADDRESS) {
				// We've reached the end of the kernel memory map.
				next_viable = 0;
				result = TASK_IO_UNMAPPED;
				break;
			}
			next_viable = (kaddr & ~page_mask) + page_size;
			mach_unexpected(true, "mach_vm_region_recurse", kr);
			result = TASK_IO_ERROR;
			break;
		}
		bool viable = region_looks_safe(&info, is_write);
		if (address > kaddr) {
			result = TASK_IO_UNMAPPED;
			if (viable) {
				next_viable = address;
			} else {
				next_viable = address + vmsize;
			}
			break;
		}
		if (!viable) {
			result = TASK_IO_PROTECTION;
			next_viable = address + vmsize;
			break;
		}
		// Next, check to see how many pages starting at kaddr are actually mapped. Here we
		// do complete the rest of the loop, since some data may be transferrable.
		size_t region_size = min(left, vmsize - (kaddr - address));
		error_stop();
		for (kaddr_t unmapped = kaddr & ~page_mask; unmapped < kaddr + region_size;
				unmapped += page_size) {
			paddr_t paddr = 0;
			kernel_virtual_to_physical(unmapped, &paddr);
			if (paddr == 0) {
				// We've encountered an unmapped page before the end of the current
				// region. Truncate our region to this smaller size.
				result = TASK_IO_UNMAPPED;
				region_size = (unmapped > kaddr ? unmapped - kaddr : 0);
				next_viable = unmapped + page_size;
				break;
			}
		}
		error_start();
		// Incorporate the region.
		kaddr         += region_size;
		left          -= region_size;
		transfer_size += region_size;
	}
	*size = transfer_size;
	if (next != NULL) {
		*next = next_viable;
	}
	return result;
}

/*
 * transfer_range_all
 *
 * Description:
 * 	Find the transfer range for the given transfer, checking virtual and physical addresses to
 * 	see if the memory is mapped and safe to access. Needs kernel_virtual_to_physical.
 */
static task_io_result
transfer_range_all(vm_map_t task, mach_vm_address_t kaddr, size_t *size, size_t *access,
		mach_vm_address_t *next, bool is_write) {
	assert(kernel_virtual_to_physical != NULL);
	bool default_access = (*access == 0);
	size_t remaining = *size;
	size_t transfer_size = 0;
	size_t transfer_access = 0;
	mach_vm_address_t next_viable = kaddr + remaining;
	task_io_result result = TASK_IO_SUCCESS;
	error_stop();
	while (remaining > 0) {
		size_t this_access = 0;
		size_t this_size   = remaining;
		// Check the virtual address.
		const struct memory_region *vr = virtual_region_find(kaddr, this_size);
		if (vr == NULL) {
			// Everything's good; do nothing.
		} else if (kaddr < vr->start) {
			this_size = vr->start - kaddr;
		} else if (default_access && vr->access == 0) {
			result = TASK_IO_INACCESSIBLE;
			next_viable = vr->end + 1;
			break;
		} else {
			this_access = vr->access;
			this_size   = min(this_size, vr->end + 1 - kaddr);
		}
		// Check the physical address.
		paddr_t paddr = 0;
		kernel_virtual_to_physical(kaddr, &paddr);
		if (paddr == 0) {
			result = TASK_IO_UNMAPPED;
			next_viable = (kaddr & ~page_mask) + page_size;
			break;
		}
		size_t phys_size = min(this_size, page_size - (kaddr & page_mask));
		const struct memory_region *pr = physical_region_find(paddr, phys_size);
		if (pr == NULL) {
			this_size = phys_size;
		} else if (paddr < pr->start) {
			this_size = pr->start - paddr;
		} else if (default_access && pr->access == 0) {
			result = TASK_IO_INACCESSIBLE;
			next_viable = (kaddr & ~page_mask) + page_size;
			break;
		} else {
			if (this_access != 0 && this_access != pr->access) {
				// We need two contradictory access widths?
				result = TASK_IO_INACCESSIBLE;
				next_viable = (kaddr & ~page_mask) + page_size;
				break;
			}
			this_access = pr->access;
			this_size   = min(phys_size, pr->end + 1 - paddr);
		}
		// Check if this access would be compatible with the currently selected transfer
		// range.
		if (this_access != transfer_access) {
			if (transfer_access == 0) {
				transfer_access = this_access;
			} else {
				next_viable = kaddr;
				break;
			}
		}
		kaddr         += this_size;
		remaining     -= this_size;
		transfer_size += this_size;
	}
	error_start();
	*size = transfer_size;
	if (next != NULL) {
		*next = next_viable;
	}
	if (default_access && transfer_access != 0) {
		*access = transfer_access;
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
		task_transfer_range_fn transfer_range, task_transfer_fn transfer, bool is_write) {
	task_io_result result = task_perform_transfer(kernel_task, kaddr, size, data, access, next,
			transfer_range, transfer, is_write);
	switch (result) {
		case TASK_IO_SUCCESS:                                         return KERNEL_IO_SUCCESS;
		case TASK_IO_ERROR:        error_kernel_io(kaddr);            return KERNEL_IO_ERROR;
		case TASK_IO_PROTECTION:   error_address_protection(kaddr);   return KERNEL_IO_PROTECTION;
		case TASK_IO_UNMAPPED:     error_address_unmapped(kaddr);     return KERNEL_IO_UNMAPPED;
		case TASK_IO_INACCESSIBLE: error_address_inaccessible(kaddr); return KERNEL_IO_INACCESSIBLE;
	}
}

kernel_io_result
kernel_read_word(kernel_read_fn read, kaddr_t kaddr, void *value, size_t width,
		size_t access_width) {
	return read(kaddr, &width, value, access_width, NULL);
}

kernel_io_result
kernel_write_word(kernel_write_fn write, kaddr_t kaddr, kword_t value, size_t width,
		size_t access_width) {
	pack_uint(&value, value, width);
	return write(kaddr, &width, &value, access_width, NULL);
}

static kernel_io_result
kernel_read_unsafe_(kaddr_t kaddr, size_t *size, void *data, size_t access_width, kaddr_t *next) {
	return kernel_io(kaddr, size, data, access_width, next, task_transfer_range_all,
			task_transfer, false);
}

static kernel_io_result
kernel_write_unsafe_(kaddr_t kaddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(kaddr, size, (void *)data, access_width, next, task_transfer_range_all,
			task_transfer, true);
}

static kernel_io_result
kernel_read_heap_(kaddr_t kaddr, size_t *size, void *data, size_t access_width, kaddr_t *next) {
	return kernel_io(kaddr, size, data, access_width, next, transfer_range_heap,
			task_transfer, false);
}

static kernel_io_result
kernel_write_heap_(kaddr_t kaddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(kaddr, size, (void *)data, access_width, next, transfer_range_heap,
			task_transfer, true);
}

static kernel_io_result
kernel_read_safe_(kaddr_t kaddr, size_t *size, void *data, size_t access_width, kaddr_t *next) {
	return kernel_io(kaddr, size, data, access_width, next, transfer_range_safe,
			task_transfer, false);
}

static kernel_io_result
kernel_write_safe_(kaddr_t kaddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(kaddr, size, (void *)data, access_width, next, transfer_range_safe,
			task_transfer, true);
}

static kernel_io_result
kernel_read_all_(kaddr_t kaddr, size_t *size, void *data, size_t access_width, kaddr_t *next) {
	return kernel_io(kaddr, size, data, access_width, next, transfer_range_all,
			task_transfer, false);
}

static kernel_io_result
kernel_write_all_(kaddr_t kaddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(kaddr, size, (void *)data, access_width, next, transfer_range_all,
			task_transfer, true);
}

static kernel_io_result
physical_read_unsafe_(paddr_t paddr, size_t *size, void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(paddr, size, data, access_width, next, task_transfer_range_all,
			transfer_physical_words_unsafe, false);
}

static kernel_io_result
physical_write_unsafe_(paddr_t paddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	return kernel_io(paddr, size, (void *)data, access_width, next, task_transfer_range_all,
			transfer_physical_words_unsafe, true);
}

bool
kernel_virtual_to_physical_(kaddr_t kaddr, paddr_t *paddr) {
	assert(_pmap_find_phys != 0 && kernel_pmap != 0);
	ppnum_t ppnum;
	kword_t args[] = { kernel_pmap, kaddr };
	bool success = kernel_call(&ppnum, sizeof(ppnum), _pmap_find_phys, 2, args);
	if (!success) {
		ERROR_CALL(_pmap_find_phys);
		return false;
	}
	if (ppnum == 0) {
		*paddr = 0;
	} else {
		*paddr = ((paddr_t)ppnum << page_shift) | (kaddr & page_mask);
	}
	return true;
}

bool
zone_element_size_(kaddr_t address, size_t *size) {
	vm_size_t vmsize;
	kword_t args[] = { address, 0 };
	bool success = kernel_call(&vmsize, sizeof(vmsize), _zone_element_size, 2, args);
	if (!success) {
		ERROR_CALL(_zone_element_size);
		return false;
	}
	*size = vmsize;
	return true;
}

void
kernel_memory_init() {
	error_stop();
#define SET(fn)									\
	if (fn == NULL) {							\
		fn = fn##_;							\
	}
#define RESOLVE_KERNEL(sym)							\
	if (sym == 0 && kernel.base != 0 && kernel.slide != 0) {		\
		(void)kernel_symbol(#sym, &sym, NULL);				\
	}
#define READ(sym, val)								\
	if (val == 0 && kernel_read_unsafe != NULL) {				\
		kaddr_t sym = 0;						\
		RESOLVE_KERNEL(sym);						\
		if (sym != 0) {							\
			(void)kernel_read_word(kernel_read_unsafe,		\
					sym, &val, sizeof(val), 0);		\
		}								\
	}
	// Load the basic kernel read/write functions.
	if (kernel_task != MACH_PORT_NULL) {
		SET(kernel_read_unsafe);
		SET(kernel_write_unsafe);
		SET(kernel_read_heap);
		SET(kernel_write_heap);
	}
	// Get the kernel_pmap.
	READ(_kernel_pmap, kernel_pmap);
	// Load kernel_virtual_to_physical.
	if (kernel_virtual_to_physical == NULL && kernel_pmap != 0) {
		RESOLVE_KERNEL(_pmap_find_phys);
		if (_pmap_find_phys != 0) {
			kword_t dummy_args[2] = { kernel_pmap, kernel.base };
			if (kernel_call(NULL, sizeof(ppnum_t), 0, 2, dummy_args)) {
				SET(kernel_virtual_to_physical);
			}
		}
	}
	// Load the kernel read/write functions that depend on kernel_virtual_to_physical.
	if (kernel_task != MACH_PORT_NULL && kernel_virtual_to_physical != NULL) {
		SET(kernel_read_safe);
		SET(kernel_write_safe);
		SET(kernel_read_all);
		SET(kernel_write_all);
	}
	// Resolve the XNU physical memory functions.
	RESOLVE_KERNEL(_IOMappedRead8);
	RESOLVE_KERNEL(_IOMappedRead16);
	RESOLVE_KERNEL(_IOMappedRead32);
	RESOLVE_KERNEL(_IOMappedRead64);
	RESOLVE_KERNEL(_IOMappedWrite8);
	RESOLVE_KERNEL(_IOMappedWrite16);
	RESOLVE_KERNEL(_IOMappedWrite32);
	RESOLVE_KERNEL(_IOMappedWrite64);
	// Load the unsafe physical memory read/write functions.
	if (physical_read_unsafe == NULL && _IOMappedRead8 != 0 && _IOMappedRead16 != 0
			&& _IOMappedRead32 != 0 && _IOMappedRead64 != 0) {
		kword_t dummy_args[2] = { 1, 1 };
		if (kernel_call(NULL, sizeof(uint32_t), 0, 1, dummy_args)) {
			SET(physical_read_unsafe);
		}
	}
	if (physical_write_unsafe == NULL && _IOMappedWrite8 != 0 && _IOMappedWrite16 != 0
			&& _IOMappedWrite32 != 0 && _IOMappedWrite64 != 0) {
		kword_t dummy_args[2] = { 1, 1 };
		if (kernel_call(NULL, 0, 0, 2, dummy_args)) {
			SET(physical_write_unsafe);
		}
	}
	// Load zone_element_size.
	if (zone_element_size == NULL) {
		RESOLVE_KERNEL(_zone_element_size);
		if (_zone_element_size != 0) {
			kword_t dummy_args[2] = { kernel.base, 0 };
			if (kernel_call(NULL, sizeof(vm_size_t), 0, 2, dummy_args)) {
				SET(zone_element_size);
			}
		}
	}
#undef SET
#undef RESOLVE_KERNEL
#undef READ
	error_start();
}
