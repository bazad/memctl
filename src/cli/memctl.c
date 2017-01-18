#include "cli/cli.h"
#include "cli/error.h"
#include "cli/format.h"

#include "core.h"
#include "kernel.h"
#include "kernel_slide.h"
#include "platform.h"

#include <stdio.h>

bool
default_action(void) {
	error_internal("default_action");
	return false;
}

bool
r_command(kaddr_t address, size_t length, bool physical, size_t width, size_t access, bool dump) {
	printf("r("KADDR_FMT", %zu, %s, %zu, %zu, %s)\n", address, length,
			(physical ? "physical" : "virtual"), width, access,
			(dump ? "dump" : "read"));
	return true;
}

bool
rb_command(kaddr_t address, size_t length, bool physical, size_t access) {
	printf("rb("KADDR_FMT", %zu, %d, %zu)\n", address, length, physical, access);
	return true;
}

bool
rs_command(kaddr_t address, size_t length, bool physical, size_t access) {
	printf("rs("KADDR_FMT", %zu, %d, %zu)\n", address, length, physical, access);
	return true;
}

bool
w_command(kaddr_t address, kword_t value, bool physical, size_t width, size_t access) {
	printf("w\n");
	return true;
}

bool
wd_command(kaddr_t address, const void *data, size_t length, bool physical, size_t access) {
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
	printf("vm("KADDR_FMT", %u)\n", address, depth);
	return true;
}

bool
vmm_command(kaddr_t start, kaddr_t end, unsigned depth) {
	printf("vm("KADDR_FMT", "KADDR_FMT", %u)\n", start, end, depth);
	return true;
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
	if (!kernel_slide_init()) {
		return false;
	}
	printf("kernel_slide:   0x%016llx\n", kernel_slide);
	if (!kernel_init(NULL)) {
		return false;
	}
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
