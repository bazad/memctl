#include "kernel_slide.h"

#include "core.h"
#include "kernel.h"
#include "memctl_error.h"
#include "utility.h"

#include <assert.h>
#include <mach/vm_region.h>
#include <mach/mach_vm.h>

kaddr_t kernel_slide;

static bool
is_kernel_slide(kword_t slide) {
	return ((slide & ~0x000000007fe00000) == 0);
}

#if KERNELCACHE

// TODO

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
		error_internal("mach_vm_region failed: %d", kr);
		return false;
	}
	// Compute the kernel slide.
	kernel_slide = (address + size) - round2_up(last, vm_page_size);
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
	assert(false); // TODO
#else
	return kernel_slide_init_macos();
#endif
}
