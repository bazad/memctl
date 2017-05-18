#ifndef MEMCTL__CORE_H_
#define MEMCTL__CORE_H_

#include "memctl/memctl_types.h"

#include <mach/mach.h>
#include <stdint.h>

/*
 * The core is responsible for obtaining the kernel task, possibly using libmemctl's functionality.
 * Each vulnerability is different and has its own best exploitation strategy. It is not memctl's
 * job to figure out how to best exploit a use-after-free or arbitrary kernel write; rather, it's
 * the core's job to turn the vulnerability into a reliable interface for libmemctl to work its
 * post-exploitation magic.
 *
 * If the vulnerability is not 100% reliable (e.g. memory corruption) it's advisable to exploit the
 * bug only once per boot and have the core use a hole the exploit created.
 */

/*
 * kernel_task
 *
 * Description:
 * 	The kernel task, or MACH_PORT_NULL if the kernel task port has not been opened.
 */
extern mach_port_t kernel_task;

/*
 * core_load
 *
 * Description:
 * 	Load the core. This includes retrieving the kernel_task port and initializing any state
 * 	needed by the core.
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
 */
bool core_load(void);

#endif
