#include "memctl/process.h"

#include "memctl/kernel.h"
#include "memctl/kernel_call.h"
#include "memctl/kernel_memory.h"
#include "memctl/memctl_error.h"
#include "memctl/utility.h"

#include <sys/param.h>


#define PROC_DUMP_SIZE 128

DEFINE_OFFSET(proc, p_ucred);

kaddr_t kernproc;
kaddr_t currentproc;
kaddr_t currenttask;
bool (*current_proc)(kaddr_t *proc);
bool (*proc_find)(kaddr_t *proc, pid_t pid, bool release);
bool (*proc_find_path)(kaddr_t *proc, const char *path, bool release);
bool (*proc_rele)(kaddr_t proc);
bool (*proc_lock)(kaddr_t proc);
bool (*proc_unlock)(kaddr_t proc);
bool (*proc_task)(kaddr_t *task, kaddr_t proc);
bool (*proc_ucred)(kaddr_t *ucred, kaddr_t proc);
bool (*proc_set_ucred)(kaddr_t proc, kaddr_t ucred);
bool (*kauth_cred_proc_ref)(kaddr_t *cred, kaddr_t proc);
bool (*kauth_cred_unref)(kaddr_t cred);
bool (*kauth_cred_setsvuidgid)(kaddr_t *newcred, kaddr_t cred, uid_t uid, gid_t gid);
bool (*task_reference)(kaddr_t task);
bool (*convert_task_to_port)(kaddr_t *ipc_port, kaddr_t task);
bool (*get_task_ipcspace)(kaddr_t *ipc_space, kaddr_t task);
bool (*ipc_port_copyout_send)(mach_port_t *port_name, kaddr_t send_right, kaddr_t ipc_space);
bool (*task_to_task_port)(mach_port_t *task_port, kaddr_t task, kaddr_t sender);
bool (*proc_to_task_port)(mach_port_t *task_port, kaddr_t proc);

static kaddr_t _kernproc;
static kaddr_t _current_proc;
static kaddr_t _proc_find;
static kaddr_t _proc_rele;
static kaddr_t _proc_lock;
static kaddr_t _proc_unlock;
static kaddr_t _proc_task;
static kaddr_t _proc_ucred;
static kaddr_t _kauth_cred_proc_ref;
static kaddr_t _kauth_cred_unref;
static kaddr_t _kauth_cred_setsvuidgid;
static kaddr_t _task_reference;
static kaddr_t _convert_task_to_port;
static kaddr_t _get_task_ipcspace;
static kaddr_t _ipc_port_copyout_send;

#define ERROR_CALL(symbol)	error_internal("could not call %s", #symbol)

// iOS does not provide libproc.h or sys/proc_info.h. We will just declare prototypes for these
// functions rather than try to include the headers. The headers would need to be heavily modified
// to compile here.

int proc_listallpids(void *buffer, int buffersize);
int proc_pidpath(int pid, void *buffer, uint32_t buffersize);

bool
proc_pids_find_path(const char *path, pid_t *pids, size_t *count) {
	// Get the number of processes.
	int capacity = proc_listallpids(NULL, 0);
	if (capacity <= 0) {
fail_0:
		error_functionality_unavailable("proc_listallpids fails");
		return false;
	}
	capacity += 10;
	assert(capacity > 0);
	// Get the list of all PIDs.
	pid_t all_pids[capacity];
	int all_count = proc_listallpids(all_pids, capacity * sizeof(*all_pids));
	if (all_count <= 0) {
		goto fail_0;
	}
	// Find all PIDs that match the specified path. We walk the list in reverse because
	// proc_listallpids seems to return the PIDs in reverse order.
	pid_t *end = pids + *count;
	size_t found = 0;
	for (int i = all_count - 1; i >= 0; i--) {
		pid_t pid = all_pids[i];
		// Get this process's path.
		char pid_path[MAXPATHLEN];
		int len = proc_pidpath(pid, pid_path, sizeof(pid_path));
		if (len <= 0) {
			continue;
		}
		// If it's a match, add it to the list and increment the number of PIDs found.
		if (strncmp(path, pid_path, len) == 0) {
			if (pids < end) {
				*pids = pid;
				pids++;
			}
			found++;
		}
	}
	*count = found;
	return true;
}

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
	assert(proc != 0);
	bool success = kernel_call(NULL, 0, _proc_rele, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_rele);
	}
	return success;
}

static bool
proc_find_(kaddr_t *proc, pid_t pid, bool release) {
	kword_t args[] = { pid };
	kaddr_t proc0;
	bool success = kernel_call(&proc0, sizeof(proc0), _proc_find, 1, args);
	if (!success) {
		ERROR_CALL(_proc_find);
		return false;
	}
	if (release) {
		success = proc_rele_(proc0);
		if (!success) {
			return false;
		}
	}
	*proc = proc0;
	return true;
}

#define PROC_FIND_PATH_PID_COUNT 16

/*
 * proc_find_path_once
 *
 * Description:
 * 	Try to find the process with the given path.
 */
static bool
proc_find_path_once(kaddr_t *proc, const char *path, bool release, bool *retry) {
	// Get the list of all PIDs matching the given path.
	pid_t pids[PROC_FIND_PATH_PID_COUNT];
	size_t count = PROC_FIND_PATH_PID_COUNT;
	bool success = proc_pids_find_path(path, pids, &count);
	if (!success) {
		return false;
	}
	// If no processes matched or if too many matched, return.
	if (count == 0) {
		*proc = 0;
		return true;
	} else if (count > PROC_FIND_PATH_PID_COUNT) {
		error_internal("too many processes match path '%s'", path);
		return false;
	}
	// Find the smallest PID.
	pid_t pid = pids[0];
	for (size_t i = 1; i < count; i++) {
		if (pids[i] < pid) {
			pid = pids[i];
		}
	}
	// Get the corresponding proc struct.
	success = proc_find(proc, pid, release);
	if (!success) {
		return false;
	}
	// If we're trying to be safe, make sure it's still the right process.
	if (!release) {
		char path2[MAXPATHLEN];
		int len = proc_pidpath(pid, path2, sizeof(path2));
		if (len <= 0 || strcmp(path, path2) != 0) {
			// The paths didn't match. Release the process and retry.
			error_stop();
			success = proc_rele_(*proc);
			error_start();
			*retry = true;
			return false;
		}
	}
	// We've opened the process and it's still the same one, so return success.
	return true;
}

static bool
proc_find_path_(kaddr_t *proc, const char *path, bool release) {
	for (unsigned tries = 0; tries < 3; tries++) {
		bool retry = false;
		bool success = proc_find_path_once(proc, path, release, &retry);
		if (success || !retry) {
			return success;
		}
	}
	error_internal("proc_find_path failed: too many retries");
	return false;

}

static bool
proc_lock_(kaddr_t proc) {
	assert(proc != 0);
	bool success = kernel_call(NULL, 0, _proc_lock, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_lock);
	}
	return success;
}

static bool
proc_unlock_(kaddr_t proc) {
	assert(proc != 0);
	bool success = kernel_call(NULL, 0, _proc_unlock, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_unlock);
	}
	return success;
}

static bool
proc_task_(kaddr_t *task, kaddr_t proc) {
	assert(proc != 0);
	bool success = kernel_call(task, sizeof(*task), _proc_task, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_task);
	}
	return success;
}

static bool
proc_ucred_(kaddr_t *ucred, kaddr_t proc) {
	assert(proc != 0);
	bool success = kernel_call(ucred, sizeof(*ucred), _proc_ucred, 1, &proc);
	if (!success) {
		ERROR_CALL(_proc_ucred);
	}
	return success;
}

static bool
proc_set_ucred_(kaddr_t proc, kaddr_t ucred) {
	assert(proc != 0);
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
	assert(proc != 0);
	bool success = kernel_call(cred, sizeof(*cred), _kauth_cred_proc_ref, 1, &proc);
	if (!success) {
		ERROR_CALL(_kauth_cred_proc_ref);
	}
	return success;
}

static bool
kauth_cred_unref_(kaddr_t cred) {
	assert(cred != 0);
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
	assert(cred != 0);
	kword_t args[3] = { cred, 0, 0 };
	bool success = kernel_call(newcred, sizeof(*newcred), _kauth_cred_setsvuidgid, 3, args);
	if (!success) {
		ERROR_CALL(_kauth_cred_setsvuidgid);
	}
	return success;
}

static bool
task_reference_(kaddr_t task) {
	assert(task != 0);
	bool success = kernel_call(NULL, 0, _task_reference, 1, &task);
	if (!success) {
		ERROR_CALL(_task_reference);
	}
	return success;
}

static bool
convert_task_to_port_(kaddr_t *ipc_port, kaddr_t task) {
	assert(task != 0);
	bool success = kernel_call(ipc_port, sizeof(*ipc_port), _convert_task_to_port, 1, &task);
	if (!success) {
		ERROR_CALL(_convert_task_to_port);
	}
	return success;
}

static bool
get_task_ipcspace_(kaddr_t *ipc_space, kaddr_t task) {
	assert(task != 0);
	bool success = kernel_call(ipc_space, sizeof(*ipc_space), _get_task_ipcspace, 1, &task);
	if (!success) {
		ERROR_CALL(_get_task_ipcspace);
	}
	return success;
}

static bool
ipc_port_copyout_send_(mach_port_t *port_name, kaddr_t send_right, kaddr_t ipc_space) {
	assert(send_right != 0 && ipc_space != 0);
	kword_t args[] = { send_right, ipc_space };
	bool success = kernel_call(port_name, sizeof(*port_name), _ipc_port_copyout_send, 2, args);
	if (!success) {
		ERROR_CALL(_ipc_port_copyout_send);
	}
	return success;
}

static bool
task_to_task_port_(mach_port_t *task_port, kaddr_t task, kaddr_t sender) {
	// TODO: This doesn't work as might be expected for kernel_task because the XNU function
	// convert_port_to_task will not return the kernel_task.
	bool success = task_reference(task);
	if (!success) {
		return false;
	}
	kaddr_t send_right;
	success = convert_task_to_port(&send_right, task);
	if (!success) {
		return false;
	}
	kaddr_t ipc_space;
	success = get_task_ipcspace(&ipc_space, sender);
	if (!success) {
		return false;
	}
	mach_port_t port_name;
	success = ipc_port_copyout_send(&port_name, send_right, ipc_space);
	if (!success) {
		return false;
	}
	*task_port = port_name;
	return true;
}

static bool
proc_to_task_port_(mach_port_t *task_port, kaddr_t proc) {
	kaddr_t task;
	bool success = proc_task(&task, proc);
	if (!success) {
		return false;
	}
	return task_to_task_port(task_port, task, currenttask);
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

/*
 * initialize_offsets
 *
 * Description:
 * 	Initialize the offsets needed by the specialized process routines.
 */
static void
initialize_offsets() {
	if (OFFSET(proc, p_ucred).valid == 0) {
		initialize_p_ucred_offset();
	}
}

void
process_init() {
	error_stop();
#define RESOLVE_KERNEL(sym)							\
	if (sym == 0) {								\
		(void)kernel_symbol(#sym, &sym, NULL);				\
	}
#define READ(sym, val)								\
	if (val == 0) {								\
		RESOLVE_KERNEL(sym);						\
		if (sym != 0) {							\
			(void)kernel_read_word(kernel_read_unsafe,		\
					sym, &val, sizeof(val), 0);		\
		}								\
	}
#define RESOLVE(sym, fn, impl)							\
	if (fn == NULL) {							\
		RESOLVE_KERNEL(sym);						\
		if (sym != 0) {							\
			fn = impl;						\
		}								\
	}
#define RESOLVE1(fn)	RESOLVE(_##fn, fn, fn##_)
	READ(_kernproc, kernproc);
	RESOLVE1(current_proc);
	RESOLVE1(proc_find);
	RESOLVE1(proc_rele);
	RESOLVE1(proc_lock);
	RESOLVE1(proc_unlock);
	RESOLVE1(proc_task);
	RESOLVE1(proc_ucred);
	RESOLVE1(kauth_cred_proc_ref);
	RESOLVE1(kauth_cred_unref);
	RESOLVE1(kauth_cred_setsvuidgid);
	RESOLVE1(task_reference);
	RESOLVE1(convert_task_to_port);
	RESOLVE1(get_task_ipcspace);
	RESOLVE1(ipc_port_copyout_send);
#undef RESOLVE_KERNEL
#undef READ
#undef RESOLVE
#undef RESOLVE1
	if (currentproc == 0 && current_proc != NULL) {
		current_proc(&currentproc);
	}
	if (currentproc != 0 && proc_task != NULL) {
		proc_task(&currenttask, currentproc);
	}
	initialize_offsets();
	if (proc_set_ucred == NULL && OFFSET(proc, p_ucred).valid > 0) {
		proc_set_ucred = proc_set_ucred_;
	}
	if (proc_find_path == NULL
			&& proc_find != NULL && proc_rele != NULL) {
		proc_find_path = proc_find_path_;
	}
	if (task_to_task_port == NULL
			&& task_reference != NULL && convert_task_to_port != NULL
			&& get_task_ipcspace != NULL && ipc_port_copyout_send != NULL) {
		task_to_task_port = task_to_task_port_;
	}
	if (proc_to_task_port == NULL
			&& proc_task != NULL && currenttask != 0 && task_to_task_port != NULL) {
		proc_to_task_port = proc_to_task_port_;
	}
	error_start();
}
