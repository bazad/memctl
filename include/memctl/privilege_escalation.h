#ifndef MEMCTL__PRIVILEGE_ESCALATION_H_
#define MEMCTL__PRIVILEGE_ESCALATION_H_

#include "memctl/memctl_types.h"

/*
 * setuid_root
 *
 * Description:
 * 	Set the real, saved, and effective UIDs and GIDs of the current process to 0.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Dependencies:
 * 	kernel_task
 * 	process_init
 */
bool setuid_root(void);

/*
 * proc_copy_credentials
 *
 * Description:
 * 	Copy the in-kernel credentials structure from the source process to the destination
 * 	process.
 *
 * Parameters:
 * 		to_proc				The proc struct of the destination process.
 * 		from_proc			The proc struct of the source process.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Dependencies:
 * 	kernel_task
 * 	process_init
 */
bool proc_copy_credentials(kaddr_t to_proc, kaddr_t from_proc);

#endif
