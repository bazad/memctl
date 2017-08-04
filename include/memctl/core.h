#ifndef MEMCTL__CORE_H_
#define MEMCTL__CORE_H_

#include "memctl/memctl_types.h"

#include <mach/mach.h>

/*
 * kernel_task
 *
 * Description:
 * 	The kernel task port, or MACH_PORT_NULL if the current task does not yet have a send right
 * 	to the kernel task.
 */
extern mach_port_t kernel_task;

/*
 * core_load
 *
 * Description:
 * 	Load the core. This includes retrieving the kernel_task port and initializing any state
 * 	needed by the core. libmemctl does not provide an implementation of this function.
 *
 * Returns:
 * 	True if the core was successfully loaded.
 *
 * Errors:
 * 	core_error			A core-specific error condition was encountered.
 * 	...				Other errors
 *
 * Notes:
 * 	It is safe to call this function multiple times.
 *
 * 	libmemctl provides the API for obtaining the kernel task port so that each core depends
 * 	only on libmemctl, regardless of what client program is using it.
 */
extern bool core_load(void);

#endif
