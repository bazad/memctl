#include "kernel_slide.h"

#include "core.h"
#include "kernel.h"
#include "kernel_memory.h"
#include "memctl_error.h"
#include "utility.h"

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

#if KERNELCACHE

static const uint64_t kernel_region_size_min = 0x0000000100000000;
static const uint64_t kernel_region_size_max = 0x0000000104000000;
static const int kernel_region_protection    = 0;

static bool
find_kernel_region(kaddr_t *region_base, kaddr_t *region_end) {
	// Get the region containing the kernel.
	for (;;) {
		mach_vm_size_t size = 0;
		struct vm_region_basic_info_64 info;
		mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
		mach_port_t object_name = MACH_PORT_NULL;
		kern_return_t kr = mach_vm_region(kernel_task, region_base, &size,
				VM_REGION_BASIC_INFO_64, (vm_region_recurse_info_t) &info, &count,
				&object_name);
		if (kr != KERN_SUCCESS) {
			error_internal("mach_vm_region(%p) failed: %s", *region_base,
					mach_error_string(kr));
			return false;
		}
		if (kernel_region_size_min <= size
		    && size <= kernel_region_size_max
		    && info.protection == kernel_region_protection) {
			*region_end = *region_base + size;
			return true;
		}
		*region_base += size;
	}
}

static const kword_t slide_increment = 0x200000;
static const kword_t max_slide       = 0x200000 * 0x200;

static bool
kernel_slide_init_ios_unsafe() {
	memctl_warning("using unsafe memory scan to locate kernel base");
	kaddr_t region_base = 0;
	kaddr_t region_end;
	if (!find_kernel_region(&region_base, &region_end)) {
		return false;
	}
	// Scan memory to find the kernel base.
	kword_t min_slide = 0;
	if (region_base > kernel.base) {
		min_slide = round2_up(region_base - kernel.base, slide_increment);
	}
	unsigned n = macho_header_size(&kernel.macho) / sizeof(kword_t);
	const kword_t *kernel_header = (const kword_t *)kernel.macho.mh;
	for (kword_t slide = min_slide; slide < max_slide; slide += slide_increment) {
		kaddr_t base = kernel.base + slide;
		assert(base >= region_base);
		// If we ever get close to the end of the memory region, bail.
		if (base >= region_end - kernel.macho.size) {
			break;
		}
		// Try to match the first n words.
		for (unsigned i = 0;; i++) {
			if (i == n) {
				kernel_slide = base - kernel.base;
				goto found;
			}
			kword_t value;
			error_stop();
			kernel_io_result kior = kernel_read_word(kernel_read_unsafe,
					base + i * sizeof(value), &value, sizeof(value), 0);
			error_start();
			if (kior != KERNEL_IO_SUCCESS) {
				break;
			}
			if (value != kernel_header[i]) {
				break;
			}
		}
	}
	error_internal("could not find kernel slide");
	return false;
found:
	if (!is_kernel_slide(kernel_slide)) {
		error_internal("computed slide 0x%016llx is not a valid kASLR slide",
				kernel_slide);
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
	kext_result kxr = kext_resolve_symbol(&kernel, "_last_kernel_symbol", &last, NULL);
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
	if (!is_kernel_slide(kernel_slide)) {
		error_internal("computed slide 0x%016llx is not a valid kASLR slide",
				kernel_slide);
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
	return kernel_slide_init_ios_unsafe();
#else
	return kernel_slide_init_macos();
#endif
}
