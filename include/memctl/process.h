#ifndef MEMCTL__PROCESS_H_
#define MEMCTL__PROCESS_H_

#include "memctl/memctl_types.h"
#include "memctl/offset.h"

#include <mach/mach.h>
#include <unistd.h>


DECLARE_OFFSET(proc, p_ucred);

/*
 * kernproc
 *
 * Description:
 * 	XNU's kernproc. The kernel proc struct.
 */
extern kaddr_t kernproc;

/*
 * currentproc
 *
 * Description:
 * 	The current process, as returned by current_proc().
 */
extern kaddr_t currentproc;

/*
 * currenttask
 *
 * Description:
 * 	The current task, as returned by current_task().
 */
extern kaddr_t currenttask;

/*
 * current_proc
 *
 * Description:
 * 	XNU's current_proc. Get the address of the proc struct for the current process.
 *
 * Parameters:
 * 	out	proc			On return, the address of the proc struct for the current
 * 					process.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*current_proc)(kaddr_t *proc);

/*
 * proc_find
 *
 * Description:
 * 	XNU's proc_find. Get the address of the proc struct for the given PID. If release is true,
 * 	the process is returned without an additional reference.
 *
 * Parameters:
 * 	out	proc			On return, the address of the proc struct for the process.
 * 		pid			The PID to find.
 * 		release			The XNU proc_find function adds a reference to the process
 * 					before returning it. If release is true, this reference is
 * 					dropped using proc_rele.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Notes:
 * 	The default behavior of proc_find is to return with the process with an additional
 * 	reference. If release is true, the returned proc pointer may be immediately stale.
 */
extern bool (*proc_find)(kaddr_t *proc, int pid, bool release);

/*
 * proc_rele
 *
 * Description:
 * 	XNU's proc_rele. Release a reference on the given proc struct.
 *
 * Parameters:
 * 		proc			The proc struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*proc_rele)(kaddr_t proc);

/*
 * proc_lock
 *
 * Description:
 * 	XNU's proc_lock. Lock a proc struct. You should already have a reference on this proc.
 *
 * Parameters:
 * 		proc			The proc struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*proc_lock)(kaddr_t proc);

/*
 * proc_unlock
 *
 * Description:
 * 	XNU's proc_unlock. Unlock a proc struct.
 *
 * Parameters:
 * 		proc			The proc struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*proc_unlock)(kaddr_t proc);

/*
 * proc_task
 *
 * Description:
 * 	XNU's proc_task. Returns the task for the given proc struct.
 *
 * Parameters:
 * 	out	task			On return, the proc's task.
 * 		proc			The proc struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*proc_task)(kaddr_t *task, kaddr_t proc);

/*
 * proc_ucred
 *
 * Description:
 * 	XNU's proc_ucred. Returns the ucred struct for the given proc struct.
 *
 * Parameters:
 * 	out	ucred			On return, the proc's ucred.
 * 		proc			The proc struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Notes:
 * 	proc_ucred performs no locking and does not add a reference on the ucred.
 */
extern bool (*proc_ucred)(kaddr_t *ucred, kaddr_t proc);

/*
 * proc_set_ucred
 *
 * Description:
 * 	Set the ucred struct for the given proc struct. No locking is performed.
 *
 * Parameters:
 * 		proc			The proc struct.
 * 		ucred			The ucred to use.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Notes:
 * 	This is not an XNU function. No locking is performed.
 */
extern bool (*proc_set_ucred)(kaddr_t proc, kaddr_t ucred);

/*
 * kauth_cred_proc_ref
 *
 * Description:
 * 	XNU's kauth_cred_proc_ref. Returns the ucred struct for the given proc struct with an
 * 	additional reference.
 *
 * Parameters:
 * 	out	cred			On return, the proc's ucred.
 * 		proc			The proc struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*kauth_cred_proc_ref)(kaddr_t *cred, kaddr_t proc);

/*
 * kauth_cred_unref
 *
 * Description:
 * 	XNU's kauth_cred_unref. Removes a reference from the ucred struct.
 *
 * Parameters:
 * 		cred			The ucred struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Notes:
 * 	The kauth_cred_unref function in XNU takes a pointer to the cred pointer and zeroes out the
 * 	cred pointer on return. This function handles this layer of indirection by allocating
 * 	kernel memory for the cred pointer and passing that address into kauth_cred_unref.
 */
extern bool (*kauth_cred_unref)(kaddr_t cred);

/*
 * kauth_cred_setsvuidgid
 *
 * Description:
 * 	XNU's kauth_cred_setsvuidgid. Create a new ucred based on the original credentials, but
 * 	with the specified saved UID and GID. The original credentials are released and the new
 * 	credentials are referenced.
 *
 * Parameters:
 * 	out	newcred			On return, the new credentials.
 * 		cred			The original credentials.
 * 		uid			The saved UID to set in the new credentials.
 * 		gid			The saved GID to set in the new credentials.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Notes:
 * 	This function is known not to be available on iOS.
 */
extern bool (*kauth_cred_setsvuidgid)(kaddr_t *newcred, kaddr_t cred, uid_t uid, gid_t gid);

/*
 * task_reference
 *
 * Description:
 * 	XNU's task_reference. Add a reference on a task.
 *
 * Parameters:
 * 		task			The task to reference.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*task_reference)(kaddr_t task);

/*
 * convert_task_to_port
 *
 * Description:
 * 	XNU's convert_task_to_port. Converts from a task to a port, consuming a reference on the
 * 	task and producing a naked send right.
 *
 * Parameters:
 * 	out	ipc_port		The ipc_port_t object representing a send right to the
 * 					task.
 * 		task			The task.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*convert_task_to_port)(kaddr_t *ipc_port, kaddr_t task);

/*
 * get_task_ipcspace
 *
 * Description:
 * 	XNU's get_task_ipcspace. Returns the ipc_space pointer for the given task.
 *
 * Parameters:
 * 	out	ipc_space		The ipc_space_t for the task.
 * 		task			The task.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*get_task_ipcspace)(kaddr_t *ipc_space, kaddr_t task);

/*
 * ipc_port_copyout_send
 *
 * Description:
 * 	XNU's ipc_port_copyout_send. Copies a naked send right to the given ipc_space, returning
 * 	the port name.
 *
 * Parameters:
 * 	out	port_name		The name of the newly added port.
 * 		send_right		An ipc_port representing the send right.
 * 		ipc_space		The ipc_space to which to add the send right.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*ipc_port_copyout_send)(
		mach_port_t *port_name,
		kaddr_t send_right,
		kaddr_t ipc_space);

/*
 * task_to_task_port
 *
 * Description:
 * 	Convert a task struct to a task port for the task, and add a send right for that task port
 * 	to the specified sender task.
 *
 * Parameters:
 * 	out	task_port		The Mach port name of a Mach port with a send right to
 * 					task, in sender's IPC space.
 * 		task			The task to which a send right will be created.
 * 		sender			The task which shall receive the send right.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*task_to_task_port)(mach_port_t *task_port, kaddr_t task, kaddr_t sender);

/*
 * proc_to_task_port
 *
 * Description:
 * 	Convert a proc struct to a task port for the process's task. This function wraps
 * 	task_to_task_port.
 *
 * Parameters:
 * 	out	task_port		A mach task port for the process's task.
 * 		proc			The proc struct.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
extern bool (*proc_to_task_port)(mach_port_t *task_port, kaddr_t proc);

/*
 * process_init
 *
 * Description:
 * 	Initialize the XNU process functions.
 */
void process_init(void);

#endif
