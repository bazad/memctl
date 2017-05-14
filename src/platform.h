#ifndef MEMCTL__PLATFORM_H_
#define MEMCTL__PLATFORM_H_

#include <mach/machine.h>
#include <stdlib.h>

/*
 * struct platform
 */
struct platform {
	struct {
		unsigned major;
		unsigned minor;
		unsigned patch;
	} release;
	char version[64];
	char machine[32];
	cpu_type_t    cpu_type;
	cpu_subtype_t cpu_subtype;
	unsigned      physical_cpu;
	unsigned      logical_cpu;
	size_t        memory;
};

extern struct platform platform;

/*
 * platform_init
 *
 * Description:
 * 	Retrieve OS version information.
 *
 * Notes:
 * 	It is safe to call this function multiple times.
 */
void platform_init(void);

#endif
