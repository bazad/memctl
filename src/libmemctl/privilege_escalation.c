#include "memctl/privilege_escalation.h"

#include "memctl/kernel_memory.h"
#include "memctl/memctl_error.h"
#include "memctl/process.h"

#include <mach/mach_vm.h>

#define NEED_FN(fn)									\
	if (fn == NULL) {								\
		error_functionality_unavailable("no implementation of %s", #fn);	\
		return false;								\
	}

#define NEED_VAL(val)									\
	if (val == 0) {									\
		error_functionality_unavailable("value of %s is unknown", #val);	\
		return false;								\
	}

/*
 * set_svuidgid_0
 *
 * Description:
 * 	Set the specified process's UID and GID to root. The process must not already be root.
 */
static bool
set_svuidgid_0(kaddr_t proc) {
	assert(proc != 0);
	// Make sure we have the functions we need.
	NEED_FN(kauth_cred_proc_ref);
	NEED_FN(kauth_cred_setsvuidgid);
	NEED_FN(proc_set_ucred);
	NEED_FN(kauth_cred_unref);
	// Add a reference to the process's credentials and get a pointer to the credentials.
	kaddr_t cred;
	bool success = kauth_cred_proc_ref(&cred, proc);
	if (!success) {
		return false;
	}
	// Create a new credentials structure based on the current one that has saved UID/GID 0.
	kaddr_t cred0;
	success = kauth_cred_setsvuidgid(&cred0, cred, 0, 0);
	if (!success) {
		// Leak the credential.
		return false;
	}
	// Replace the old unprivileged credentials with the new privileged credentials.
	success = proc_set_ucred(proc, cred0);
	if (!success) {
		// Leak the credential.
		return false;
	}
	// Remove a reference on the old credentials.
	error_stop();
	kauth_cred_unref(cred);
	error_start();
	return true;
}

bool
setuid_root() {
	// Check if we're already root.
	seteuid(0);
	setuid(0);
	setgid(0);
	if (getuid() == 0) {
		return true;
	}
	// Set our saved UID/GID to root.
	NEED_VAL(currentproc);
	if (!set_svuidgid_0(currentproc)) {
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

bool
proc_copy_credentials(kaddr_t to_proc, kaddr_t from_proc) {
	assert(from_proc != 0 && to_proc != 0);
	NEED_FN(proc_ucred);
	NEED_FN(kauth_cred_proc_ref);
	NEED_FN(proc_set_ucred);
	NEED_FN(kauth_cred_unref);
	kaddr_t to_cred = 0;
	error_stop();
	proc_ucred(&to_cred, to_proc);
	error_start();
	kaddr_t from_cred;
	bool success = kauth_cred_proc_ref(&from_cred, from_proc);
	if (!success) {
		return false;
	}
	success = proc_set_ucred(to_proc, from_cred);
	if (!success) {
		return false;
	}
	if (to_cred != 0) {
		error_stop();
		kauth_cred_unref(to_cred);
		error_start();
	}
	return true;
}
