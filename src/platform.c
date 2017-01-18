#include "platform.h"

#include <assert.h>
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
}
