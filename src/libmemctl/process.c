#include "memctl/process.h"

#include "memctl/kernel.h"
#include "memctl/kernel_call.h"
#include "memctl/kernel_memory.h"
#include "memctl/memctl_error.h"
#include "memctl/utility.h"


#define PROC_DUMP_SIZE 128

DEFINE_OFFSET(proc, p_ucred);

kaddr_t kernproc;
kaddr_t currentproc;
bool (*current_proc)(kaddr_t *proc);
bool (*proc_find)(kaddr_t *proc, int pid, bool release);
bool (*proc_rele)(kaddr_t proc);
bool (*proc_task)(kaddr_t *task, kaddr_t proc);
bool (*proc_ucred)(kaddr_t *ucred, kaddr_t proc);
bool (*proc_set_ucred)(kaddr_t proc, kaddr_t ucred);
bool (*kauth_cred_proc_ref)(kaddr_t *cred, kaddr_t proc);
bool (*kauth_cred_unref)(kaddr_t cred);
bool (*kauth_cred_setsvuidgid)(kaddr_t *newcred, kaddr_t cred, uid_t uid, gid_t gid);
bool (*task_reference)(kaddr_t task);
bool (*convert_task_to_port)(kaddr_t *ipc_port, kaddr_t task);
bool (*proc_to_task_port)(mach_port_t *task_port, kaddr_t proc);

static kaddr_t _kernproc;
static kaddr_t _current_proc;
static kaddr_t _proc_find;
static kaddr_t _proc_rele;
static kaddr_t _proc_task;
static kaddr_t _proc_ucred;
static kaddr_t _kauth_cred_proc_ref;
static kaddr_t _kauth_cred_unref;
static kaddr_t _kauth_cred_setsvuidgid;
static kaddr_t _task_reference;
static kaddr_t _convert_task_to_port;

#define ERROR_CALL(symbol)	error_internal("could not call %s", #symbol)

static bool
current_proc_(kaddr_t *proc) {
	bool success = kernel_call(proc, sizeof(*proc), _current_proc, 0, NULL);
	if (!success) {
		ERROR_CALL(_current_proc);
	}
	return success;
}

static bool
proc_rele_(kaddr_t proc) {
	bool success = kernel_call(NULL, 0, _proc_rele, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_rele);
	}
	return success;
}

static bool
proc_find_(kaddr_t *proc, int pid, bool release) {
	kword_t args[] = { pid };
	kaddr_t proc0;
	bool success = kernel_call(&proc0, sizeof(proc0), _proc_find, 1, args);
	if (!success) {
		ERROR_CALL(_proc_find);
		return false;
	}
	if (!release) {
		success = proc_rele_(proc0);
		if (!success) {
			return false;
		}
	}
	*proc = proc0;
	return true;
}

static bool
proc_task_(kaddr_t *task, kaddr_t proc) {
	bool success = kernel_call(task, sizeof(*task), _proc_task, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_task);
	}
	return success;
}

static bool
proc_ucred_(kaddr_t *ucred, kaddr_t proc) {
	bool success = kernel_call(ucred, sizeof(*ucred), _proc_ucred, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_ucred);
	}
	return success;
}

static bool
proc_set_ucred_(kaddr_t proc, kaddr_t ucred) {
	kaddr_t proc_cred_addr = proc + OFFSETOF(proc, p_ucred);
	kernel_io_result kio = kernel_write_word(kernel_write_unsafe, proc_cred_addr,
			ucred, sizeof(ucred), 0);
	if (kio != KERNEL_IO_SUCCESS) {
		error_internal("could not replace process credentials");
		return false;
	}
	return true;
}

static bool
kauth_cred_proc_ref_(kaddr_t *cred, kaddr_t proc) {
	bool success = kernel_call(cred, sizeof(*cred), _kauth_cred_proc_ref, 1, &proc);
	if (!success) {
		ERROR_CALL(_kauth_cred_proc_ref);
	}
	return success;
}

static bool
kauth_cred_unref_(kaddr_t cred) {
	bool successful = false;
	kaddr_t pcred;
	bool success = kernel_allocate(&pcred, sizeof(cred));
	if (!success) {
		goto fail_0;
	}
	kernel_io_result kio = kernel_write_word(kernel_write_unsafe, pcred, cred, sizeof(cred), 0);
	if (kio != KERNEL_IO_SUCCESS) {
		goto fail_1;
	}
	success = kernel_call(NULL, 0, _kauth_cred_unref, 1, &pcred);
	if (!success) {
		ERROR_CALL(_kauth_cred_unref);
		goto fail_1;
	}
	successful = true;
fail_1:
	kernel_deallocate(pcred, sizeof(cred));
fail_0:
	return successful;
}

static bool
kauth_cred_setsvuidgid_(kaddr_t *newcred, kaddr_t cred, uid_t uid, gid_t gid) {
	kword_t args[3] = { cred, 0, 0 };
	bool success = kernel_call(newcred, sizeof(*newcred), _kauth_cred_setsvuidgid, 3, args);
	if (!success) {
		ERROR_CALL(_kauth_cred_setsvuidgid);
	}
	return success;
}

static bool
task_reference_(kaddr_t task) {
	bool success = kernel_call(NULL, 0, _task_reference, 1, &task);
	if (!success) {
		ERROR_CALL(_task_reference);
	}
	return success;
}

static bool
convert_task_to_port_(kaddr_t *ipc_port, kaddr_t task) {
	bool success = kernel_call(ipc_port, sizeof(*ipc_port), _convert_task_to_port, 1, &task);
	if (!success) {
		ERROR_CALL(_convert_task_to_port);
	}
	return success;
}

static bool
initialize_p_ucred_offset() {
	// Dump the contents of the current proc struct.
	kword_t proc_data[PROC_DUMP_SIZE];
	size_t readsize = sizeof(proc_data);
	kernel_io_result kio = kernel_read_unsafe(currentproc, &readsize, proc_data, 0, NULL);
	if (kio != KERNEL_IO_SUCCESS) {
		return false;
	}
	const unsigned max_idx = readsize / sizeof(kword_t);
	// Get the credentials pointer for the current process.
	kaddr_t cred;
	bool success = proc_ucred(&cred, currentproc);
	if (!success) {
		return false;
	}
	// Get the index of the credentials in the proc struct.
	unsigned cred_idx;
	for (cred_idx = 0; cred_idx < max_idx; cred_idx++) {
		if (proc_data[cred_idx] == cred) {
			OFFSET(proc, p_ucred).offset = cred_idx * sizeof(kword_t);
			OFFSET(proc, p_ucred).valid  = 2;
			return true;
		}
	}
	return false;
}

static void
initialize_offsets() {
	if (OFFSET(proc, p_ucred).valid == 0) {
		initialize_p_ucred_offset();
	}
}

void
process_init() {
	error_stop();
#define READ(sym, val)								\
	if (sym == 0) {								\
		kern_return_t kr = kernel_symbol(#sym, &sym, NULL);		\
		if (kr == KEXT_SUCCESS) {					\
			(void)kernel_read_word(kernel_read_unsafe,		\
					sym, &val, sizeof(val), 0);		\
		}								\
	}
#define RESOLVE(sym, fn, impl)							\
	if (sym == 0) {								\
		kern_return_t kr = kernel_symbol(#sym, &sym, NULL);		\
		if (kr == KEXT_SUCCESS) {					\
			fn = impl;						\
		}								\
	}
	READ(_kernproc, kernproc);
	RESOLVE(_current_proc, current_proc, current_proc_);
	RESOLVE(_proc_find, proc_find, proc_find_);
	RESOLVE(_proc_rele, proc_rele, proc_rele_);
	RESOLVE(_proc_task, proc_task, proc_task_);
	RESOLVE(_proc_ucred, proc_ucred, proc_ucred_);
	RESOLVE(_kauth_cred_proc_ref, kauth_cred_proc_ref, kauth_cred_proc_ref_);
	RESOLVE(_kauth_cred_unref, kauth_cred_unref, kauth_cred_unref_);
	RESOLVE(_kauth_cred_setsvuidgid, kauth_cred_setsvuidgid, kauth_cred_setsvuidgid_);
	RESOLVE(_task_reference, task_reference, task_reference_);
	RESOLVE(_convert_task_to_port, convert_task_to_port, convert_task_to_port_);
#undef READ
#undef RESOLVE
	if (currentproc == 0 && current_proc != NULL) {
		current_proc(&currentproc);
	}
	initialize_offsets();
	if (OFFSET(proc, p_ucred).valid > 0) {
		proc_set_ucred = proc_set_ucred_;
	}
	error_start();
}
