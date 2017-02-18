#include "cli/cli.h"
#include "cli/disassemble.h"
#include "cli/error.h"
#include "cli/format.h"
#include "cli/memory.h"
#include "cli/read.h"
#include "cli/vmmap.h"

#include "core.h"
#include "kernel.h"
#include "kernel_slide.h"
#include "platform.h"
#include "vtable.h"

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

/*
 * kext_error
 *
 * Description:
 * 	Propagate a kext_result code into an error.
 */
static bool
kext_error(kext_result kr, const char *bundle_id, const char *symbol, kaddr_t address) {
	switch (kr) {
		case KEXT_SUCCESS:
			return false;
		case KEXT_ERROR:
			return true;
		case KEXT_NO_KEXT:
			if (bundle_id == NULL) {
				error_message("no kernel component contains address "KADDR_FMT,
				              address);
			} else {
				error_kext_not_found(bundle_id);
			}
			return true;
		case KEXT_NO_SYMBOLS:
			error_kext_no_symbols(bundle_id);
			return true;
		case KEXT_NOT_FOUND:
			error_kext_symbol_not_found(bundle_id, symbol);
			return true;
	}
}

bool
default_action(void) {
	return command_print_help(NULL);
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

#if MEMCTL_DISASSEMBLY

bool
ri_command(kaddr_t address, size_t length, bool physical, size_t access) {
	if (!check_address(address, length, physical)) {
		return false;
	}
	return memctl_disassemble(address, length, physical, access);
}

bool
rif_command(const char *function, const char *kext, size_t access) {
	kaddr_t address;
	size_t size;
	kext_result kr = resolve_symbol(kext, function, &address, &size);
	if (kext_error(kr, kext, function, 0)) {
		return false;
	}
	return memctl_disassemble(address, size, false, access);
}

#endif // MEMCTL_DISASSEMBLY

bool
rs_command(kaddr_t address, size_t length, bool physical, size_t access) {
	// If the user didn't specify a length, then length is -1, which will result in an overflow
	// error. Instead we check for one page of validity.
	if (!check_address(address, page_size, physical)) {
		return false;
	}
	return memctl_read_string(address, length, physical, access);
}

bool
w_command(kaddr_t address, kword_t value, bool physical, size_t width, size_t access) {
	return wd_command(address, &value, width, physical, access);
}

bool
wd_command(kaddr_t address, const void *data, size_t length, bool physical, size_t access) {
	if (!check_address(address, length, physical)) {
		return false;
	}
	return write_memory(address, &length, data, physical, access);
}

bool
ws_command(kaddr_t address, const char *string, bool physical, size_t access) {
	size_t length = strlen(string) + 1;
	return wd_command(address, string, length, physical, access);
}

bool
f_command(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical,
		size_t access, size_t alignment) {
	printf("f\n");
	return true;
}

bool
fpr_command(pid_t pid) {
	static kword_t _proc_find = 0;
	if (_proc_find == 0) {
		kext_result kr = kernel_symbol("_proc_find", &_proc_find, NULL);
		if (kext_error(kr, KERNEL_ID, "_proc_find", 0)) {
			return false;
		}
	}
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
	kaddr_t address;
	size_t size;
	kext_result kr = vtable_for_class(classname, bundle_id, &address, &size);
	if (kr == KEXT_NOT_FOUND) {
		if (bundle_id == NULL) {
			error_message("class %s not found", classname);
		} else if (strcmp(bundle_id, KERNEL_ID) == 0) {
			error_message("class %s not found in kernel", classname);
		} else {
			error_message("class %s not found in kernel extension %s", classname,
			              bundle_id);
		}
		return false;
	} else if (kext_error(kr, bundle_id, NULL, 0)) {
		return false;
	}
	printf(KADDR_FMT"  (%zu)\n", address, size);
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
	printf(KADDR_FMT"\n", address + (unslide ? -1 : 1) * kernel_slide);
	return true;
}

bool
a_command(const char *symbol, const char *kext) {
	kaddr_t address;
	size_t size;
	kext_result kr = resolve_symbol(kext, symbol, &address, &size);
	if (kext_error(kr, kext, symbol, 0)) {
		return false;
	}
	printf(KADDR_FMT"  (%zu)\n", address, size);
	return true;
}

bool
ap_command(kaddr_t address, bool unpermute) {
	static kword_t kernel_addrperm = 0;
	if (kernel_addrperm == 0) {
		kword_t _vm_kernel_addrperm;
		kext_result kr = kernel_symbol("_vm_kernel_addrperm", &_vm_kernel_addrperm, NULL);
		if (kext_error(kr, KERNEL_ID, "_vm_kernel_addrperm", 0)) {
			return false;
		}
		size_t size = sizeof(kernel_addrperm);
		bool read = read_memory(_vm_kernel_addrperm, &size, &kernel_addrperm, false, 0);
		if (!read) {
			return false;
		}
	}
	printf(KADDR_FMT"\n", address + (unpermute ? -1 : 1) * kernel_addrperm);
	return true;
}

bool
s_command(kaddr_t address) {
	char *bundle_id = NULL;
	kext_result kr = kext_containing_address(address, &bundle_id);
	if (kext_error(kr, NULL, NULL, address)) {
		return false;
	}
	struct kext kext;
	kr = kext_init(&kext, bundle_id);
	bool is_error = kext_error(kr, bundle_id, NULL, 0);
	free(bundle_id);
	if (is_error) {
		return false;
	}
	const char *name = NULL;
	size_t size = 0;
	size_t offset = 0;
	kr = kext_resolve_address(&kext, address, &name, &size, &offset);
	if (kr == KEXT_NOT_FOUND || (kr == KEXT_SUCCESS && strlen(name) == 0)) {
		if (offset == 0) {
			printf("%s\n", kext.bundle_id);
		} else {
			printf("%s+%zu\n", kext.bundle_id, offset);
		}
	} else if (kr == KEXT_SUCCESS) {
		if (offset == 0) {
			printf("%s: %s  (%zu)\n", kext.bundle_id, name, size);
		} else {
			printf("%s: %s+%zu  (%zu)\n", kext.bundle_id, name, offset, size);
		}
	} else {
		is_error = kext_error(kr, kext.bundle_id, NULL, 0);
	}
	kext_deinit(&kext);
	return !is_error;
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
	if (!core_load()) {
		return false;
	}
	if (!kernel_init(NULL)) {
		return false;
	}
	if (!kernel_slide_init()) {
		return false;
	}
	kernel_init(NULL);
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
