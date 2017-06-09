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

static bool
set_svuidgid_0() {
	// Make sure we have the functions we need.
	NEED_VAL(currentproc);
	NEED_FN(kauth_cred_proc_ref);
	NEED_FN(kauth_cred_setsvuidgid);
	NEED_FN(proc_set_ucred);
	NEED_FN(kauth_cred_unref);
	// Add a reference to the current process's credentials and get a pointer to the
	// credentials.
	kaddr_t cred;
	bool success = kauth_cred_proc_ref(&cred, currentproc);
	if (!success) {
		return false;
	}
	// Create a new credentials structure based on our current one that has saved UID/GID 0.
	kaddr_t cred0;
	success = kauth_cred_setsvuidgid(&cred0, cred, 0, 0);
	if (!success) {
		// Leak the credential.
		return false;
	}
	// Replace our old unprivileged credentials with our new privileged credentials.
	success = proc_set_ucred(currentproc, cred0);
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

// The saved (original) credentials of the current process, or 0 if the current process has its
// original credentials.
static kaddr_t saved_cred;

bool
use_kernel_credentials(bool kernel) {
	if (saved_cred == 0 && kernel) {
		NEED_VAL(currentproc);
		NEED_FN(proc_ucred);
		NEED_VAL(kernproc);
		NEED_FN(kauth_cred_proc_ref);
		NEED_FN(proc_set_ucred);
		NEED_FN(kauth_cred_unref);
		kaddr_t current_cred;
		bool success = proc_ucred(&current_cred, currentproc);
		if (!success) {
			return false;
		}
		kaddr_t kern_cred;
		success = kauth_cred_proc_ref(&kern_cred, kernproc);
		if (!success) {
			return false;
		}
		success = proc_set_ucred(currentproc, kern_cred);
		if (!success) {
			return false;
		}
		saved_cred = current_cred;
	} else if (saved_cred != 0 && !kernel) {
		bool success = proc_set_ucred(currentproc, saved_cred);
		if (!success) {
			return false;
		}
		saved_cred = 0;
		// Remove the additional reference on the kernel credentials.
		error_stop();
		kaddr_t kern_cred;
		success = proc_ucred(&kern_cred, kernproc)
			&& kauth_cred_unref(kern_cred);
		error_start();
		if (!success) {
			memctl_warning("could not remove reference from kernel credentials");
		}
	}
	return true;
}
