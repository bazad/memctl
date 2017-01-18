#ifndef MEMCTL__PLATFORM_H_
#define MEMCTL__PLATFORM_H_

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
