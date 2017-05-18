#include "memctl/x86_64/kernel_call_syscall_x86_64.h"

#include "x86_64/kernel_call_syscall_code.h"

#include "memctl/kernel.h"
#include "memctl/kernel_slide.h"
#include "memctl/utility.h"

kernel_read_fn  kernel_read_text;
kernel_write_fn kernel_write_text;

/*
 * struct syscall_hook
 *
 * Description:
 * 	The state needed to install a system call hook.
 */
struct syscall_hook {
	// The location of the sysent table in kernel memory.
	kaddr_t sysent;
	// The target function address.
	kaddr_t function;
	// The original contents of the memory at the target function address.
	void *original;
	// The number of bytes at the start of the target function that were overwritten.
	size_t size;
	// The address of _nosys in the kernel.
	kaddr_t _nosys;
};

/*
 * struct sysent
 *
 * Description:
 * 	An entry in the system call table.
 */
struct sysent {
	kaddr_t sy_call;
	kaddr_t sy_munge;
	int32_t  sy_return_type;
	int16_t  sy_narg;
	uint16_t sy_arg_bytes;
};

// The global syscall hook.
static struct syscall_hook syscall_hook;

// The target function that will be overwritten to install the syscall hook.
static const char target_function[] = "_bsd_init";

// Prototypes for the assembly functions.
extern int kernel_call_syscall_dispatch(void *p, uint64_t arg[6], uint64_t *ret);
extern void kernel_call_syscall_dispatch_end(void);

#define _SYSCALL_RET_NONE       0
#define _SYSCALL_RET_INT_T      1
#define _SYSCALL_RET_SSIZE_T    6
#define _SYSCALL_RET_UINT64_T   7

#define RESOLVE_STR(sym, str)						\
	kaddr_t sym;							\
	do {								\
		kext_result kr = kernel_symbol(str, &sym, NULL);	\
		if (kr != KEXT_SUCCESS) {				\
			error_internal("could not resolve %s", str);	\
			return false;					\
		}							\
	} while (0)

#define RESOLVE_STATIC(sym)						\
	RESOLVE_STR(sym, #sym);						\
	sym -= kernel_slide;						\
/*
 * find_sysent
 *
 * Description:
 * 	Find the system call table.
 */
static bool
find_sysent() {
	// Resolve the various symbols we need.
	RESOLVE_STATIC(_nosys);
	RESOLVE_STATIC(_exit);
	RESOLVE_STATIC(_fork);
	RESOLVE_STATIC(_read);
	RESOLVE_STATIC(_write);
	RESOLVE_STATIC(_munge_w);
	RESOLVE_STATIC(_munge_www);
	// Find the runtime address of the system call table.
	struct sysent sysent_init[] = {
		{ _nosys, 0,          _SYSCALL_RET_INT_T,   0,  0 },
		{ _exit,  _munge_w,   _SYSCALL_RET_NONE,    1,  4 },
		{ _fork,  0,          _SYSCALL_RET_INT_T,   0,  0 },
		{ _read,  _munge_www, _SYSCALL_RET_SSIZE_T, 3, 12 },
		{ _write, _munge_www, _SYSCALL_RET_SSIZE_T, 3, 12 },
	};
	kaddr_t sysent;
	kext_result kr = kext_search_data(&kernel, sysent_init, sizeof(sysent_init), 0, &sysent);
	if (kr != KEXT_SUCCESS) {
		error_internal("could not find sysent in kernel image");
		return false;
	}
	// Check that the sysent in the kernel matches what we expect.
	for (unsigned i = 0; i < sizeof(sysent_init) / sizeof(sysent_init[0]); i++) {
		sysent_init[i].sy_call += kernel_slide;
		if (sysent_init[i].sy_munge != 0) {
			sysent_init[i].sy_munge += kernel_slide;
		}
	}
	uint8_t sysent_data[sizeof(sysent_init)];
	size_t size = sizeof(sysent_init);
	kernel_io_result kio = kernel_read_text(sysent, &size, sysent_data, 0, NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		error_internal("could not read kernel sysent");
		return false;
	}
	if (memcmp(sysent_init, sysent_data, sizeof(sysent_init)) != 0) {
		error_internal("kernel sysent data mismatch");
		return false;
	}
	// Save the sysent and _nosys.
	syscall_hook.sysent = sysent;
	syscall_hook._nosys = _nosys + kernel_slide;
	return true;
}

/*
 * install_syscall_hook
 *
 * Description:
 * 	Install the system call hook.
 */
static bool
install_syscall_hook() {
	RESOLVE_STR(function, target_function);
	const uintptr_t hook = (uintptr_t)kernel_call_syscall_dispatch;
	size_t hook_size = (uintptr_t)kernel_call_syscall_dispatch_end - hook;
	// Check that the target syscall can be overwritten.
	kaddr_t target_sysent = syscall_hook.sysent + SYSCALL_CODE * sizeof(struct sysent);
	kaddr_t target_sy_call = target_sysent + offsetof(struct sysent, sy_call);
	kernel_io_result kio = kernel_read_word(kernel_read_text, target_sy_call, &target_sy_call,
			sizeof(target_sy_call), 0);
	if (kio != KERNEL_IO_SUCCESS) {
		error_internal("could not read syscall %d at address 0x%llx", SYSCALL_CODE,
		               target_sy_call);
		return false;
	}
	if (target_sy_call != syscall_hook._nosys) {
		error_internal("target syscall %d is not empty: currently points to 0x%llx",
		               SYSCALL_CODE, target_sy_call);
		return false;
	}
	// Read the original data from the target function.
	hook_size = round2_up(hook_size, sizeof(kword_t));
	syscall_hook.size = hook_size;
	syscall_hook.original = malloc(syscall_hook.size);
	if (syscall_hook.original == NULL) {
		error_out_of_memory();
		return false;
	}
	kio = kernel_read_text(function, &hook_size, syscall_hook.original, 0, NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		error_internal("could not read contents of function %s", target_function);
		return false;
	}
	// Overwrite the target function. We do this first so that if we fail partway through we
	// don't leave the system with an unstable syscall.
	kio = kernel_write_text(function, &hook_size, (const void *)hook, 0, NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		error_internal("could not overwrite the contents of function %s", target_function);
		return false;
	}
	// Overwrite the sysent.
	struct sysent hook_sysent = {
		.sy_call        = function,
		.sy_munge       = 0,
		.sy_return_type = _SYSCALL_RET_UINT64_T,
		.sy_narg        = 6,
		.sy_arg_bytes   = 48,
	};
	size_t sysent_size = sizeof(hook_sysent);
	kio = kernel_write_text(target_sysent, &sysent_size, &hook_sysent, 0, NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		error_internal("could not overwrite sysent %d with a syscall hook", SYSCALL_CODE);
		return false;
	}
	// Success.
	syscall_hook.function = function;
	return true;
}

/*
 * remove_syscall_hook
 *
 * Description:
 * 	Remove the system call hook.
 */
static void
remove_syscall_hook() {
	if (syscall_hook.function == 0) {
		return;
	}
	// We do this first because physical_write_unsafe calls kernel_call; see
	// kernel_call_syscall_x86_64.
	kaddr_t function = syscall_hook.function;
	syscall_hook.function = 0;
	// Replace our sysent hook with an empty sysent.
	kaddr_t target_sysent = syscall_hook.sysent + SYSCALL_CODE * sizeof(struct sysent);
	struct sysent empty_sysent = {
		.sy_call        = syscall_hook._nosys,
		.sy_munge       = 0,
		.sy_return_type = _SYSCALL_RET_INT_T,
		.sy_narg        = 0,
		.sy_arg_bytes   = 0,
	};
	size_t sysent_size = sizeof(empty_sysent);
	kernel_io_result kio = kernel_write_text(target_sysent, &sysent_size, &empty_sysent, 0,
			NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		memctl_warning("could not replace original sysent at address 0x%llx",
		               target_sysent);
	}
	// Replace the original contents of the function we overwrote.
	kio = kernel_write_text(function, &syscall_hook.size, syscall_hook.original, 0, NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		memctl_warning("could not replace original contents of function %s",
		               target_function);
	}
	free(syscall_hook.original);
}

/*
 * kernel_write_text_default
 *
 * Description:
 * 	A default implementation of kernel_write_text based on physical_write_unsafe.
 */
static kernel_io_result
kernel_write_text_default(kaddr_t kaddr, size_t *size, const void *data, size_t access_width,
		kaddr_t *next) {
	paddr_t paddr;
	bool success = kernel_virtual_to_physical(kaddr, &paddr);
	if (!success) {
		error_internal("could not convert 0x%llx to physical address", kaddr);
		return false;
	}
	return physical_write_unsafe(paddr, size, data, access_width, next);
}

/*
 * init_text_io
 *
 * Description:
 * 	Initialize kernel_read_text and kernel_write_text if no values were specified.
 */
static bool
init_text_io() {
	const char *func;
	if (kernel_read_text == NULL) {
		if (kernel_read_unsafe == NULL) {
			func = "kernel_read_text";
			goto need_func;
		}
		kernel_read_text = kernel_read_unsafe;
	}
	if (kernel_write_text == NULL) {
		if (physical_write_unsafe == NULL) {
			func = "kernel_write_text";
			goto need_func;
		}
		kernel_write_text = kernel_write_text_default;
	}
	return true;
need_func:
	error_internal("kernel_call_init_syscall_x86_64: need an implementation of %s", func);
	return false;
}

bool
kernel_call_init_syscall_x86_64(void) {
	if (kernel.base == 0 || kernel_slide == 0 || kernel.slide == 0) {
		error_internal("kernel_call_init_syscall_x86_64: kernel and kernel_slide "
		               "not initialized");
		return false;
	}
	return init_text_io() && find_sysent() && install_syscall_hook();
}

void
kernel_call_deinit_syscall_x86_64(void) {
	remove_syscall_hook();
	kernel_read_text  = NULL;
	kernel_write_text = NULL;
}

bool
kernel_call_syscall_x86_64(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const kword_t args[]) {
	// There's unfortunately a dependency chain here: We use physical_write_unsafe to write
	// the kernel, which calls kernel_call, which calls kernel_call_syscall_x86_64. Thus, we
	// need to fail unless the syscall hook has been installed.
	// TODO: A better solution is to export an API to register kernel call handlers with
	// libmemctl.
	if (func == 0) {
		// Everything with at most 5 arguments is supported.
		return (syscall_hook.function != 0 && arg_count <= 5);
	}
	assert(syscall_hook.function != 0);
	assert(arg_count <= 5);
	// Get exactly 5 arguments.
	uint64_t args64[5] = { 0 };
	for (unsigned i = 0; i < arg_count; i++) {
		args64[i] = args[i];
	}
	// Do the syscall.
	uint64_t result64 = syscall_kernel_call_x86_64(func, args64[0], args64[1], args64[2],
			args64[3], args64[4]);
	// Save the result.
	if (result_size > 0) {
		pack_uint(result, result64, result_size);
	}
	return true;
}
