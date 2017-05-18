#ifndef MEMCTL__OSKEXT_H_
#define MEMCTL__OSKEXT_H_

#include "memctl/kernel.h"
#include "memctl/macho.h"
#include "memctl/memctl_types.h"

/*
 * oskext_get_address
 *
 * Description:
 * 	Find the runtime base address and size of the given kext in the kernel.
 *
 * Parameters:
 * 		bundle_id		The bundle identifier of the kext.
 * 	out	base			On return, the base address of the kext.
 * 	out	size			On return, the size of the kext image in memory.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 *
 * Notes:
 * 	The OSKext API returns the unslid addresses, so the current value of kernel_slide is added
 * 	to the result. If kernel_slide has not yet been initialized, the returned base address will
 * 	be the unslid address, rather than the true runtime address.
 *
 * 	This functionality might not be available on all platforms. On platforms like iOS with
 * 	kernelcaches that split the kexts, the load address and size correspond to the __TEXT
 * 	segment, not any of the other segments.
 *
 */
kext_result oskext_get_address(const char *bundle_id, kaddr_t *base, size_t *size);

/*
 * oskext_for_each
 *
 * Description:
 * 	Call the given callback function with the specified context for each loaded kernel
 * 	extension.
 *
 * Parameters:
 * 		callback		The callback function.
 * 		context			A context passed to the callback.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Dependencies:
 * 	kernel_slide
 */
bool oskext_for_each(kext_for_each_callback_fn callback, void *context);

/*
 * oskext_find_containing_address
 *
 * Description:
 * 	Find the kext containing the given kernel virtual address, and return a pointer to the
 * 	kext identifier in `kext`. The caller must free the returned string.
 *
 * Parameters:
 * 		kaddr			The kernel virtual address.
 * 	out	bundle_id		The bundle identifier of the kext. The caller is
 * 					responsible for freeing this string.
 * 	out	base			On return, the base address of the kext.
 * 	out	size			On return, the size of the kext image in memory.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 *
 * Notes:
 * 	This functionality might not be available on all platforms. On platforms like iOS with
 * 	kernelcaches that split the kexts, only the __TEXT segment will be searched.
 */
kext_result oskext_find_containing_address(kaddr_t kaddr, char **bundle_id,
		kaddr_t *base, size_t *size);

/*
 * oskext_init_macho
 *
 * Description:
 * 	Initialize a macho struct with the contents of the kernel extension's Mach-O binary.
 *
 * Parameters:
 * 	out	macho			The macho struct to initialize.
 * 		bundle_id		The bundle ID of the kext.
 *
 * Returns:
 * 	A kext_result code.
 */
kext_result oskext_init_macho(struct macho *macho, const char *bundle_id);

/*
 * oskext_deinit_macho
 *
 * Description:
 * 	Clean up resources allocated in oskext_init_macho.
 *
 * Parameters:
 * 		macho			The macho struct to clean up.
 */
void oskext_deinit_macho(struct macho *macho);

#endif
