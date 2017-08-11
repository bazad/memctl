#ifndef MEMCTL__KERNELCACHE_H_
#define MEMCTL__KERNELCACHE_H_

#include "memctl/kernel.h"
#include "memctl/macho.h"

#include <CoreFoundation/CoreFoundation.h>

extern const CFStringRef kCFPrelinkInfoDictionaryKey;
extern const CFStringRef kCFPrelinkExecutableLoadKey;
extern const CFStringRef kCFPrelinkExecutableSizeKey;

/*
 * struct kernelcache
 *
 * Description:
 * 	A parsed kernelcache.
 */
struct kernelcache {
	const void *data;
	size_t size;
	struct macho kernel;
	CFDictionaryRef prelink_info;
	const struct segment_command_64 *text;
	const struct segment_command_64 *prelink_text;
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
kext_result kernelcache_init_file(struct kernelcache *kc, const char *file);

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
kext_result kernelcache_init(struct kernelcache *kc, const void *data, size_t size);

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
kext_result kernelcache_init_uncompressed(struct kernelcache *kc, const void *data,
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
 * 	A kext_result code.
 */
kext_result kernelcache_parse_prelink_info(const struct macho *kernel,
		CFDictionaryRef *prelink_info);

/*
 * kernelcache_kext_for_each
 *
 * Description:
 * 	Call the given callback function with the specified context for each kernel extension
 * 	(including pseudoextensions) in the kernelcache. This does not include the kernel itself.
 *
 * Parameters:
 * 		kc			The kernelcache.
 * 		callback		The callback function.
 * 		context			A context passed to the callback.
 */
void kernelcache_kext_for_each(const struct kernelcache *kc, kext_for_each_callback_fn callback,
		void *context);

/*
 * kernelcache_for_each
 *
 * Description:
 * 	Call the given callback function with the specified context for each kernel extension
 * 	(including pseudoextensions and the kernel itself) in the kernelcache.
 *
 * Parameters:
 * 		kc			The kernelcache.
 * 		callback		The callback function.
 * 		context			A context passed to the callback.
 */
void kernelcache_for_each(const struct kernelcache *kc, kext_for_each_callback_fn callback,
		void *context);

/*
 * kernelcache_get_address
 *
 * Description:
 * 	Find the static base address and size of the given kext in the kernelcache.
 *
 * Parameters:
 * 		kc			The kernelcache.
 * 		bundle_id		The bundle identifier of the kext.
 * 	out	base			On return, the unslid base address of the kext.
 * 	out	size			On return, the size of the initial segment of the kext.
 * 					This may be less than the full size if the kext is split.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NO_KEXT			The kext was not found.
 */
kext_result kernelcache_get_address(const struct kernelcache *kc,
		const char *bundle_id, kaddr_t *base, size_t *size);

/*
 * kernelcache_find_containing_address
 *
 * Description:
 * 	Find the kext containing the given unslid kernel virtual address, and return a pointer to
 * 	the kext identifier in `kext`. The caller must free the returned string.
 *
 * Parameters:
 * 		kc			The kenelcache.
 * 		kaddr			The kernel virtual address
 * 	out	bundle_id		The bundle identifier of the kext. The caller is
 * 					responsible for freeing this string. May be NULL.
 * 	out	base			The base address of the kext. May be NULL.
 * 	out	macho			On return, an initialized macho struct describing the kext.
 * 					See kernelcache_kext_init_macho_at_address.
 *
 * Returns:
 * 	A kext_result code. An error is only generated if bundle_id is non-NULL and an
 * 	out-of-memory condition is encountered.
 */
kext_result kernelcache_find_containing_address(const struct kernelcache *kc, kaddr_t kaddr,
		char **bundle_id, kaddr_t *base, struct macho *macho);

/*
 * kernelcache_kext_init_macho
 *
 * Description:
 * 	Initialize a macho struct with the contents of the kernel extension's Mach-O binary from
 * 	the kernelcache.
 *
 * Parameters:
 * 		kc			The kernelcache.
 * 	out	macho			The macho struct to initialize.
 * 		bundle_id		The bundle ID of the kext.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NO_KEXT			The kext was not found.
 *
 * Notes:
 * 	This function is less efficient than kernelcache_kext_init_macho_at_address.
 */
kext_result kernelcache_kext_init_macho(const struct kernelcache *kc, struct macho *macho,
		const char *bundle_id);

/*
 * kernelcache_kext_init_macho_at_address
 *
 * Description:
 * 	Initialize a macho struct with the contents of the Mach-O file at the given binary address.
 *
 * Parameters:
 * 		kc			The kernelcache.
 * 	out	macho			The macho struct to initialize.
 * 		base			The static address of the Mach-O header.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NO_KEXT			The kext was not found.
 *
 * Notes:
 * 	Because kexts in the kernelcache are split, the returned macho struct will describe a
 * 	region of memory that also encompasses many more kernel extensions. The size will be an
 * 	overestimate of the true size of the Mach-O describing the kernel extension.
 */
kext_result kernelcache_kext_init_macho_at_address(const struct kernelcache *kc,
		struct macho *macho, kaddr_t base);

#endif
