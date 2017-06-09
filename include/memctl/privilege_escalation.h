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
 * use_kernel_credentials
 *
 * Description:
 * 	Set the current process's credentials to the kernel's credentials.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Dependencies:
 * 	kernel_task
 * 	process_init
 */
bool use_kernel_credentials(bool kernel);

#endif
