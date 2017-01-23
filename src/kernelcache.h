#ifndef MEMCTL__KERNELCACHE_H_
#define MEMCTL__KERNELCACHE_H_

#include "macho.h"

typedef enum kernelcache_result {
	KERNELCACHE_SUCCESS,
	KERNELCACHE_ERROR,
} kernelcache_result;

struct kernelcache_kext {
	// TODO
};

struct kernelcache {
	struct macho kernel;
	size_t nkexts;
	struct kernelcache_kext *kexts;
};

/*
 * kernelcache_init_file
 *
 * Description:
 * 	Initialize the kernelcache from the given file.
 *
 * Notes:
 * 	Call kernelcache_deinit to free any resources allocated to the kernelcache object.
 */
kernelcache_result kernelcache_init_file(struct kernelcache *kc, const char *file);

/*
 * kernelcache_init
 *
 * Description:
 * 	Initialize the kernelcache with the given data.
 *
 * Parameters:
 * 	out	kc			The kernelcache to initialize.
 * 		data			The kernelcache data. Must be allocated with mmap.
 * 		size			The size of the kernelcache data.
 *
 * Notes:
 * 	data must be a region allocated with mmap.
 *
 * 	kernelcache_init assumes ownership of data. In particular, if initialization fails, the
 * 	memory region is unmapped.
 */
kernelcache_result kernelcache_init(struct kernelcache *kc, const void *data, size_t size);

/*
 * kernelcache_init_uncompressed
 *
 * Description:
 * 	Initialize the kernelcache with the given uncompressed kernelcache data.
 *
 * Parameters:
 * 	out	kc			The kernelcache to initialize.
 * 		data			The kernelcache data. Must be allocated with mmap.
 * 		size			The size of the kernelcache data.
 *
 * Notes:
 * 	See kernelcache_init.
 */
kernelcache_result kernelcache_init_uncompressed(struct kernelcache *kc, const void *data, size_t size);

/*
 * kernelcache_deinit
 *
 * Description:
 * 	Deinitialize the kernelcache.
 */
void kernelcache_deinit(struct kernelcache *kc);

#endif
