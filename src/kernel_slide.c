#include "kernel_slide.h"

#include "core.h"
#include "memctl_error.h"

#include <assert.h>
#include <mach/vm_region.h>
#include <mach/mach_vm.h>

kaddr_t kernel_slide;

bool
kernel_slide_init() {
	if (kernel_slide != 0) {
		return true;
	}
#if KERNELCACHE
	assert(false); // TODO
#else
	// TODO
	mach_vm_address_t address;
	mach_vm_size_t size;
	uint32_t depth = 0;
	vm_region_basic_info_data_64_t info;
	mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
	kern_return_t kr = mach_vm_region_recurse(kernel_task, &address, &size, &depth,
			(vm_region_recurse_info_t) &info, &count);
	if (kr != KERN_SUCCESS) {
		error_internal("mach_vm_region_recurse failed: %x", kr); // TODO
		return false;
	}
	assert(false); // TODO
#endif
	return false;
}
