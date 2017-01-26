#ifndef MEMCTL__KERNELCACHE_H_
#define MEMCTL__KERNELCACHE_H_

#include "kernel.h"
#include "macho.h"

#include <CoreFoundation/CoreFoundation.h>

extern const CFStringRef kCFPrelinkInfoDictionaryKey;
extern const CFStringRef kCFPrelinkExecutableLoadKey;
extern const CFStringRef kCFPrelinkExecutableSizeKey;

/*
 * enum kernelcache_result
 *
 * Description:
 * 	Result codes for kernelcache functions.
 */
typedef enum kernelcache_result {
	KERNELCACHE_SUCCESS,
	KERNELCACHE_ERROR,
	KERNELCACHE_NOT_FOUND,
} kernelcache_result;

/*
 * struct kernelcache
 *
 * Description:
 * 	A parsed kernelcache.
 */
struct kernelcache {
	struct macho kernel;
	CFDictionaryRef prelink_info;
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
kernelcache_result kernelcache_init_uncompressed(struct kernelcache *kc, const void *data,
		size_t size);

/*
 * kernelcache_deinit
 *
 * Description:
 * 	Deinitialize the kernelcache.
 */
void kernelcache_deinit(struct kernelcache *kc);

/*
 * kernelcache_parse_prelink_info
 *
 * Description:
 * 	Try to find and parse the __PRELINK_INFO segment.
 *
 * Parameters:
 * 		kernel			The kernel Mach-O.
 * 	out	prelink_info		On return, the parsed contents of the __PRELINK_INFO
 * 					segment.
 *
 * Returns:
 * 	A kernelcache_result code.
 */
kernelcache_result kernelcache_parse_prelink_info(const struct macho *kernel,
		CFDictionaryRef *prelink_info);

/*
 * kernelcache_for_each
 *
 * Description:
 * 	Call the given callback function with the specified context for each kernel extension
 * 	(including pseudoextensions) in the kernelcache.
 *
 * Parameters:
 * 		callback		The callback function.
 * 		context			A context passed to the callback.
 *
 * Returns:
 * 	true if no errors were encountered.
 *
 * Dependencies:
 * 	kernel_slide
 * 	TODO
 */
bool kernelcache_for_each(const struct kernelcache *kc, kext_for_each_callback_fn callback,
		void *context);

/*
 * kernelcache_find_containing_address
 *
 * Description:
 * 	Find the kext containing the given kernel virtual address, and return a pointer to the
 * 	kext identifier in `kext`. The caller must free the returned string.
 *
 * Parameters:
 * 		kaddr			The kernel virtual address
 * 	out	bundle_id		The bundle identifier of the kext. The caller is
 * 					responsible for freeing this string.
 *
 * Returns:
 * 	A kernelcache_result code indicating success status.
 *
 * Dependencies:
 * 	kernel_slide
 */
kernelcache_result kernelcache_find_containing_address(const struct kernelcache *kc, kaddr_t kaddr,
		char **bundle_id);

#endif
