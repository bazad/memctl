#include "cli/cli.h"
#include "cli/error.h"
#include "cli/format.h"
#include "cli/read.h"
#include "cli/vmmap.h"

#include "core.h"
#include "kernel.h"
#include "kernel_slide.h"
#include "platform.h"

#include <stdio.h>

/*
 * looks_like_physical_address
 *
 * Description:
 * 	Returns true if the given address looks like it could be a physical address.
 */
static bool
looks_like_physical_address(paddr_t address) {
#if KERNEL_BITS == 32
	return true;
#else
	return ((address & 0xffff000000000000) == 0);
#endif
}

/*
 * looks_like_kernel_address
 *
 * Description:
 * 	Returns true if the given address looks like it could be a kernel address.
 */
static bool
looks_like_kernel_address(kaddr_t address) {
#if KERNEL_BITS == 32
	return (address >= 0xc0000000);
#else
	return ((address >> 40) == 0xffffff);
#endif
}

/*
 * check_address
 *
 * Description:
 * 	Checks that the given address looks valid.
 */
static bool
check_address(kaddr_t address, size_t length, bool physical) {
	if (address + length < address) {
		error_usage(NULL, NULL, "overflow at address "KADDR_FMT, address);
		return false;
	}
	if (physical) {
		if (!looks_like_physical_address(address)) {
			error_usage(NULL, NULL, "address "KADDR_FMT" does not look like a "
			            "physical address", address);
			return false;
		}
	} else {
		if (!looks_like_kernel_address(address)) {
			error_usage(NULL, NULL, "address "KADDR_FMT" does not look like a "
			            "kernel virtual address", address);
			return false;
		}
	}
	return true;
}

bool
default_action(void) {
	error_internal("default_action");
	return false;
}

bool
r_command(kaddr_t address, size_t length, bool physical, size_t width, size_t access, bool dump) {
	if (!check_address(address, length, physical)) {
		return false;
	}
	if (dump) {
		return memctl_dump(address, length, physical, width, access);
	} else {
		return memctl_read(address, length, physical, width, access);
	}
}

bool
rb_command(kaddr_t address, size_t length, bool physical, size_t access) {
	if (!check_address(address, length, physical)) {
		return false;
	}
	return  memctl_dump_binary(address, length, physical, access);
}

bool
rs_command(kaddr_t address, size_t length, bool physical, size_t access) {
	if (!check_address(address, length, physical)) {
		return false;
	}
	return memctl_read_string(address, length, physical, access);
}

bool
w_command(kaddr_t address, kword_t value, bool physical, size_t width, size_t access) {
	if (!check_address(address, width, physical)) {
		return false;
	}
	printf("w\n");
	return true;
}

bool
wd_command(kaddr_t address, const void *data, size_t length, bool physical, size_t access) {
	if (!check_address(address, length, physical)) {
		return false;
	}
	printf("wd\n");
	return true;
}

bool
f_command(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical,
		size_t access, size_t alignment) {
	printf("f\n");
	return true;
}

bool
fpr_command(pid_t pid) {
	printf("fpr\n");
	return true;
}

bool
fi_command(kaddr_t start, kaddr_t end, const char *classname, const char *bundle_id,
		size_t access) {
	printf("fi\n");
	return true;
}

bool
kp_command(kaddr_t address) {
	printf("kp("KADDR_FMT")\n", address);
	return true;
}

bool
kpm_command(kaddr_t start, kaddr_t end) {
	printf("kpm("KADDR_FMT", "KADDR_FMT")\n", start, end);
	return true;
}

bool
vt_command(const char *classname, const char *bundle_id) {
	printf("vt(%s, %s)\n", classname, bundle_id);
	return true;
}

bool
vm_command(kaddr_t address, unsigned depth) {
	return memctl_vmmap(address, address, depth);
}

bool
vmm_command(kaddr_t start, kaddr_t end, unsigned depth) {
	return memctl_vmmap(start, end, depth);
}

bool
ks_command(kaddr_t address, bool unslide) {
	printf("ks("KADDR_FMT", %d)\n", address, unslide);
	return true;
}

bool
a_command(const char *symbol, const char *kext) {
	printf("a(%s, %s)\n", symbol, kext);
	return true;
}

bool
ap_command(kaddr_t address, bool unpermute) {
	printf("ap("KADDR_FMT", %d)\n", address, unpermute);
	return true;
}

bool
s_command(kaddr_t address) {
	printf("s("KADDR_FMT")\n", address);
	return true;
}

/*
 * initialize
 *
 * Description:
 * 	Initialize libmemctl.
 */
static bool
initialize() {
	platform_init();
	printf("release:        %d.%d.%d\n"
	       "version:        %s\n"
	       "machine:        %s\n",
	       platform.release.major, platform.release.minor, platform.release.patch,
	       platform.version,
	       platform.machine);
	if (!core_load()) {
		return false;
	}
	if (!kernel_init(NULL)) {
		return false;
	}
	if (!kernel_slide_init()) {
		return false;
	}
	printf("kernel_slide:   0x%016llx\n", kernel_slide);
	kernel_init(NULL);
	printf("kernel __TEXT:  0x%016llx\n", kernel.base);
	return true;
}

static void
deinitialize() {
	kernel_deinit();
}

int main(int argc, const char *argv[]) {
	bool success = false;
	if (initialize()) {
		success = command_run_argv(argc - 1, argv + 1);
	}
	deinitialize();
	print_errors();
	return (success ? 0 : 1);
}
