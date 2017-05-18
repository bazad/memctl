#include "memctl/platform.h"

#include <assert.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

struct platform platform;

void
platform_init() {
	if (platform.release.major != 0) {
		return;
	}
	struct utsname u;
	int err = uname(&u);
	assert(err == 0);
	int matched = sscanf(u.release, "%u.%u.%u",
			&platform.release.major, &platform.release.minor, &platform.release.patch);
	assert(matched == 3);
	char *version = strstr(u.version, "root:");
	assert(version != NULL);
	size_t len = strlen(version);
	assert(len < sizeof(platform.version));
	strncpy(platform.version, version, sizeof(platform.version));
	len = strlen(u.machine);
	assert(len < sizeof(platform.machine));
	strncpy(platform.machine, u.machine, sizeof(platform.machine));
	mach_port_t host = mach_host_self();
	if (host != MACH_PORT_NULL) {
		host_basic_info_data_t basic_info;
		mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
		kern_return_t kr = host_info(host, HOST_BASIC_INFO, (host_info_t) &basic_info,
				&count);
		if (kr == KERN_SUCCESS) {
			platform.cpu_type     = basic_info.cpu_type;
			platform.cpu_subtype  = basic_info.cpu_subtype;
			platform.physical_cpu = basic_info.physical_cpu;
			platform.logical_cpu  = basic_info.logical_cpu;
			platform.memory       = basic_info.max_mem;
		}
		mach_port_deallocate(mach_task_self(), host);
	}
}
