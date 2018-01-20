#define KERNEL_PRIVATE 1
#include "mach/vm_statistics.h"
#undef KERNEL_PRIVATE

#include "memctl/kernel_slide.h"

#include "memctl/core.h"
#include "memctl/kernel.h"
#include "memctl/kernel_memory.h"
#include "memctl/memctl_error.h"
#include "memctl/memctl_signal.h"
#include "memctl/platform.h"
#include "memctl/utility.h"

#include <assert.h>
#include <mach/vm_region.h>
#include <mach/mach_vm.h>

kaddr_t kernel_slide;

/*
 * is_kernel_slide
 *
 * Description:
 * 	Returns true if the given value is a valid kernel slide.
 */
static bool
is_kernel_slide(kword_t slide) {
	return ((slide & ~0x000000007fe00000) == 0);
}

/*
 * validate_kernel_slide
 *
 * Description:
 * 	Pushes an error message onto the error stack if the computed kernel slide is not valid.
 */
static bool
validate_kernel_slide() {
	if (!is_kernel_slide(kernel_slide)) {
		error_internal("computed slide 0x%016llx is not a valid kASLR slide",
				kernel_slide);
		return false;
	}
	return true;
}

#if KERNELCACHE

static const uint64_t kernel_region_size_min = 0x0000000100000000;
static const uint64_t kernel_region_size_max = 0x0000000104000000;
static const uint64_t kernel_region_static_address  = 0xfffffff000000000;
static const uint64_t kernel_region_static_size_min = 0x000000027fffc000;
static const int kernel_region_protection    = 0;

static const kword_t slide_increment = 0x200000;
static const kword_t max_slide       = 0x200000 * 0x200;

/*
 * find_kernel_region
 *
 * Description:
 * 	Find the region of kernel memory containing the kernel mapping.
 */
static bool
find_kernel_region(kaddr_t *region_base, kaddr_t *region_end) {
	// Get the region containing the kernel.
	mach_vm_address_t address = 0;
	for (;;) {
		mach_vm_size_t size = 0;
		struct vm_region_basic_info_64 info;
		mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
		mach_port_t object_name = MACH_PORT_NULL;
		kern_return_t kr = mach_vm_region(kernel_task, &address, &size,
				VM_REGION_BASIC_INFO_64, (vm_region_recurse_info_t) &info, &count,
				&object_name);
		if (kr != KERN_SUCCESS) {
			error_internal("mach_vm_region(%p) failed: %s", address,
					mach_error_string(kr));
			return false;
		}
		if (interrupted) {
			error_interrupt();
			return false;
		}
		// There are 2 different virtual memory layouts on iOS:
		//   - On the iPhone 5s and iPhone 6s, the kernel lives inside a 4GB region mapped
		//     somewhere at or above 0xfffffff000000000.
		//   - On the iPhone 7, the kernel lives inside a ~10GB region from
		//     0xfffffff000000000 to 0xfffffff27fffc000.
		// The kernel is always mapped with ---/--- permissions and SHRMOD=NUL.
		if (info.protection == kernel_region_protection
		    && (   (kernel_region_size_min <= size
		            && size <= kernel_region_size_max)
		        || (address == kernel_region_static_address
		            && size >= kernel_region_static_size_min))) {
			*region_base = address;
			*region_end  = address + size;
			return true;
		}
		address += size;
	}
}

/*
 * read_word
 *
 * Description:
 * 	Read a word of kernel memory, suppressing any errors.
 */
static bool
read_word(kaddr_t addr, kword_t *value) {
	error_stop();
	kernel_io_result ior = kernel_read_word(kernel_read_unsafe, addr, value, sizeof(*value),
			0);
	error_start();
	return ior == KERNEL_IO_SUCCESS;
}

/*
 * check_kernel_base
 *
 * Description:
 * 	Checks if the given address contains the kernel base. If so, sets the kernel_slide and
 * 	returns true.
 */
static bool
check_kernel_base(kaddr_t base) {
	unsigned n = macho_header_size(&kernel.macho) / sizeof(kword_t);
	const kword_t *kernel_header = (const kword_t *)kernel.macho.mh;
	// Try to match the first n words.
	for (unsigned i = 0;; i++) {
		if (i == n) {
			kernel_slide = base - kernel.base;
			return true;
		}
		kword_t value;
		bool success = read_word(base + i * sizeof(value), &value);
		if (!success) {
			return false;
		}
		if (value != kernel_header[i]) {
			return false;
		}
	}
}

/*
 * find_kernel_slide_from_heap_zone
 *
 * Description:
 * 	Find the kernel slide given a possible heap zone_array element.
 */
static bool
find_kernel_slide_from_heap_zone(kaddr_t zone_address, size_t zone_size,
		kaddr_t kernel_region_base, kaddr_t kernel_region_end) {
	// Read the pointer at the start of the possible zone element.
	kaddr_t zone_array_ptr;
	bool success = read_word(zone_address, &zone_array_ptr);
	if (!success) {
		return false;
	}
	if (zone_array_ptr <= kernel_region_base || kernel_region_end <= zone_array_ptr) {
		return false;
	}
	// Check that the first pointer in the zone array points back to the zone.
	kaddr_t ptr;
	success = read_word(zone_array_ptr, &ptr);
	if (!success || ptr != zone_address) {
		return false;
	}
	// This looks like it might be a valid pointer within the kernel zone_array. Scan backwards
	// looking for the kernel header.
	ptr = zone_array_ptr & ~page_mask;
	for (; ptr >= kernel_region_base; ptr -= page_size) {
		if (check_kernel_base(ptr)) {
			return true;
		}
	}
	return false;

}

/*
 * kernel_slide_init_ios_heap_scan
 *
 * Description:
 * 	Find the kernel slide by scanning the heap. This is a hack, but it seems reasonably safe in
 * 	practice.
 *
 * 	We scan the heap by looking for memory regions with the VM_KERN_MEMORY_ZONE tag at submap
 * 	depth 0. On both iOS and macOS, there are 2 of these zones, and both contain as their first
 * 	element a pointer into _zone_array.
 */
static bool
kernel_slide_init_ios_heap_scan(kaddr_t kernel_region_base, kaddr_t kernel_region_end) {
	mach_vm_address_t address = 0;
	for (;;) {
		// Get the next memory region.
		mach_vm_size_t size = 0;
		uint32_t depth = 0;
		struct vm_region_submap_info_64 info;
		mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
		kern_return_t kr = mach_vm_region_recurse(kernel_task, &address, &size, &depth,
			(vm_region_recurse_info_t)&info, &count);
		if (kr != KERN_SUCCESS) {
			return false;
		}
		// Skip regions that are not on the heap, in a submap, not readable and writable,
		// or empty.
		int prot = VM_PROT_READ | VM_PROT_WRITE;
		if (info.user_tag != VM_KERN_MEMORY_ZONE
		    || depth != 0
		    || (info.protection & prot) != prot
		    || info.pages_resident == 0) {
			goto next;
		}
		// Check whether we can recover the kernel slide from this region.
		bool success = find_kernel_slide_from_heap_zone(address, size, kernel_region_base,
				kernel_region_end);
		if (success) {
			return true;
		}
next:
		address += size;
	}
}

/*
 * find_kernelcache_pre_header_segments
 *
 * Description:
 * 	Find the start and end addresses of the segments of the kernelcache that are mapped before
 * 	the kernel header.
 */
static bool
find_kernelcache_pre_header_segments(kaddr_t *preheader_start, kaddr_t *preheader_end) {
	kaddr_t preheader_min = kernel.base;
	kaddr_t preheader_max = 0;
	const struct load_command *sc = NULL;
	for (;;) {
		sc = macho_next_segment(&kernel.macho, sc);
		if (sc == NULL) {
			break;
		}
		const struct segment_command_64 *segment = (const struct segment_command_64 *) sc;
		// Skip zero-sized segments or segments that don't fall before the kernel base.
		if (segment->vmsize == 0 || segment->vmaddr >= kernel.base) {
			continue;
		}
		if (segment->vmaddr < preheader_min) {
			preheader_min = segment->vmaddr;
		}
		kaddr_t vmend = segment->vmaddr + segment->vmsize;
		if (vmend > preheader_max) {
			preheader_max = vmend;
		}
	}
	if (preheader_min == kernel.base) {
		return false;
	}
	*preheader_start = preheader_min;
	*preheader_end   = preheader_max;
	return true;
}

/*
 * kernel_slide_init_ios_heap_scan_2_unsafe
 *
 * Description:
 * 	Find the kernel slide by scanning the contents of the heap to look for pointers to the
 * 	__PRELINK sections.
 *
 * 	For each memory region returned by mach_vm_region_recurse(), we check whether the region
 * 	looks like a heap region that is safe to examine. If so, we read the first word of each
 * 	page in the region. If that word looks like a pointer into the kernel carveout, then it's
 * 	possible that this word is a vtable pointer or other pointer to the kernel image. We take
 * 	the minimum value of all of these pointers and hope that it points to memory in one of the
 * 	__PRELINK sections, which lies before the kernel's Mach-O header in memory. Then, we check
 * 	each possible kernel slide value, filtering out any proposed slide values that would cause
 * 	the minimum kernel pointer found earlier to lie outside of the __PRELINK sections.
 *
 * 	This method of detecting the kernel slide is unsafe. It is possible that the heap changes
 * 	while we are reading it (leading to a data access abort) or that the heap contains a word
 * 	that looks like a valid pointer but isn't.
 */
static bool
kernel_slide_init_ios_heap_scan_2_unsafe(kaddr_t kernel_region_base, kaddr_t kernel_region_end) {
	// Make sure we have a __TEXT segment and some segments that fall before it.
	kaddr_t preheader_start, preheader_end;
	bool ok = find_kernelcache_pre_header_segments(&preheader_start, &preheader_end);
	if (!ok) {
		return false;
	}
	// Now try and find a pointer in the kernel heap to data in the kernel image that lies
	// before the kernel's __TEXT segment. We'll take the smallest such pointer.
	kaddr_t kernel_ptr = (kaddr_t)(-1);
	mach_vm_address_t address = 0;
	for (;;) {
		// Get the next memory region.
		mach_vm_size_t size = 0;
		uint32_t depth = 2;
		struct vm_region_submap_info_64 info;
		mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
		kern_return_t kr = mach_vm_region_recurse(kernel_task, &address, &size, &depth,
			(vm_region_recurse_info_t)&info, &count);
		if (kr != KERN_SUCCESS) {
			break;
		}
		// Skip any region that is not on the heap, not in a submap, not readable and
		// writable, or not fully mapped.
		int prot = VM_PROT_READ | VM_PROT_WRITE;
		if (info.user_tag != VM_KERN_MEMORY_ZONE
		    || depth != 1
		    || (info.protection & prot) != prot
		    || info.pages_resident * page_size != size) {
			goto next;
		}
		// Read the first word of this region. This is sometimes a pointer into the kernel
		// carveout. If it is, it might be a vtable pointer. We are hoping that at least
		// one IOKit class gets allocated like this, so that the min value of all pointers
		// into the kernel carveout falls in the kernel image before the kernel's __TEXT
		// segment.
		for (size_t offset = 0; offset < size; offset += page_size) {
			kword_t value = 0;
			bool ok = read_word(address + offset, &value);
			if (ok
			    && kernel_region_base <= value
			    && value < kernel_region_end
			    && value < kernel_ptr) {
				kernel_ptr = value;
			}
		}
next:
		address += size;
	}
	// If we didn't find any such pointer, abort.
	if (kernel_ptr == (kaddr_t)(-1)) {
		return false;
	}
	// Now try each possible slide value until we find one that works. We want to probe
	// addresses from low to high, which means attempting kernel slides from low to high. Our
	// constraint is that the test pointer must lie within our (slid) pre-__TEXT regions.
	for (kword_t slide = 0; slide < max_slide; slide += slide_increment) {
		// Check that our kernel pointer lies within the proposed pre-__TEXT region.
		kword_t slid_preheader_start = preheader_start + slide;
		kword_t slid_preheader_end   = preheader_end + slide;
		if (kernel_ptr < slid_preheader_start || slid_preheader_end <= kernel_ptr) {
			continue;
		}
		// This slide is consistent. Check whether it is correct.
		kaddr_t base = kernel.base + slide;
		assert(base >= kernel_region_base);
		if (check_kernel_base(base)) {
			return true;
		}
	}
	return false;
}

/*
 * kernel_slide_init_ios_unsafe_scan
 *
 * Description:
 * 	Find the kernel slide by performing an unsafe memory scan. This seems to work on iOS
 * 	10.1.1, but not on 10.2.
 */
static bool
kernel_slide_init_ios_unsafe_scan(kaddr_t region_base, kaddr_t region_end) {
	// Don't use this technique after iOS 10.2 (XNU 16.3.0).
	if (PLATFORM_XNU_VERSION_GE(16, 3, 0)) {
		return false;
	}
	memctl_warning("Using unsafe memory scan to locate kernel base; this may trigger a panic");
	// Scan memory to find the kernel base.
	kword_t min_slide = 0;
	if (region_base > kernel.base) {
		min_slide = round2_up(region_base - kernel.base, slide_increment);
	}
	for (kword_t slide = min_slide; slide < max_slide; slide += slide_increment) {
		kaddr_t base = kernel.base + slide;
		assert(base >= region_base);
		// If we ever get close to the end of the memory region, bail.
		if (base >= region_end - kernel.macho.size) {
			break;
		}
		if (check_kernel_base(base)) {
			return true;
		}
	}
	return false;
}

/*
 * kernel_slide_init_ios
 *
 * Description:
 * 	Initialize the kernel slide on iOS.
 */
static bool
kernel_slide_init_ios() {
	platform_init();
	kaddr_t region_base, region_end;
	if (!find_kernel_region(&region_base, &region_end)) {
		error_internal("could not find kernel region");
		return false;
	}
	bool success = kernel_slide_init_ios_heap_scan_2_unsafe(region_base, region_end)
		|| kernel_slide_init_ios_heap_scan(region_base, region_end)
		|| kernel_slide_init_ios_unsafe_scan(region_base, region_end);
	if (!success) {
		error_internal("could not find kernel slide");
		return false;
	}
	if (!validate_kernel_slide()) {
		return false;
	}
	return true;
}

#else

/*
 * kernel_slide_init_macos
 *
 * Description:
 * 	Initialize the kernel slide on macOS.
 */
static bool
kernel_slide_init_macos() {
	// Get the binary address of _last_kernel_symbol.
	kaddr_t last;
	kext_result kxr = kernel_symbol("_last_kernel_symbol", &last, NULL);
	if (kxr != KEXT_SUCCESS) {
		error_internal("could not find _last_kernel_symbol");
		return false;
	}
	// Get the region containing the kernel.
	mach_vm_address_t address = kernel.base;
	mach_vm_size_t size = 0;
	struct vm_region_basic_info_64 info;
	mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
	mach_port_t object_name = MACH_PORT_NULL;
	kern_return_t kr = mach_vm_region(kernel_task, &address, &size, VM_REGION_BASIC_INFO_64,
			(vm_region_recurse_info_t) &info, &count, &object_name);
	if (kr != KERN_SUCCESS) {
		error_internal("mach_vm_region failed: %s", mach_error_string(kr));
		return false;
	}
	// Compute the kernel slide.
	kernel_slide = (address + size) - round2_up(last, page_size);
	if (!validate_kernel_slide()) {
		return false;
	}
	return true;
}

#endif

bool
kernel_slide_init() {
	if (kernel_slide != 0) {
		return true;
	}
#if KERNELCACHE
	kernel_memory_init();
	return kernel_slide_init_ios();
#else
	return kernel_slide_init_macos();
#endif
}
