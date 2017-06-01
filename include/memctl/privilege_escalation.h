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
 * 	True if no errors were encountered.:w
 *
 * Dependencies:
 * 	kernel_task
 * 	kernel_call
 */
bool setuid_root(void);

#endif
