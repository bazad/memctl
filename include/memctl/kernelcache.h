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
	// The following fields are only populated after a call to kernelcache_process().
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
 * Parameters:
 * 	out	kc			The kernelcache to initialize.
 * 		file			The path to the kernelcache file.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_ERROR			Could not open file, file I/O error, out of memory, could
 * 					not decompress, invalid kernelcache, or parse failure.
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
 * 		data			The kernelcache data. The data is copied internally.
 * 		size			The size of the kernelcache data.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_ERROR			Out of memory, could not decompress, invalid kernelcache,
 * 					or parse failure.
 *
 * Notes:
 * 	Call kernelcache_deinit to free any resources allocated to the kernelcache object.
 *
 * 	As with all the kernelcache_init* functions, only the fields data, size, and kernel are
 * 	initialized. In order to initialize the other fields, call kernelcache_process().
 *
 * 	kernelcache_init does not assume ownership of the data; it is safe to modify the data after
 * 	this function returns.
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
 * 		data			The kernelcache data. Will be deallocated with free.
 * 		size			The size of the kernelcache data.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_ERROR			Out of memory, invalid kernelcache, or parse failure.
 *
 * Notes:
 * 	Call kernelcache_deinit to free any resources allocated to the kernelcache object.
 *
 * 	kernelcache_init_uncompressed assumes ownership of the supplied data. This data should not
 * 	be modified after this function is called. This pointer will be deallocated using free()
 * 	during kernelcache_deinit.
 */
kext_result kernelcache_init_uncompressed(struct kernelcache *kc, const void *data, size_t size);

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
 * kernelcache_process
 *
 * Description:
 * 	Process the kernelcache and populate the extra fields in the kernelcache struct. This
 * 	processing includes identifying commonly used segments and parsing the __PRELINK_INFO
 * 	segment (see kernelcache_parse_prelink_info).
 *
 * Parameters:
 * 		kc			The kernelcache struct.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_ERROR			Out of memory, invalid kernelcache, or parse failure.
 *
 * Notes:
 * 	On failure, any fields that could not be initialized are NULL.
 *
 * 	Do not call this function multiple times without deinitializing the kernelcache first.
 */
kext_result kernelcache_process(struct kernelcache *kc);

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
