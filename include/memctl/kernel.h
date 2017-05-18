#ifndef MEMCTL__KERNEL_H_
#define MEMCTL__KERNEL_H_

#include "memctl/macho.h"
#include "memctl/memctl_types.h"

#include <CoreFoundation/CoreFoundation.h>

/*
 * KERNEL_ID
 *
 * Description:
 * 	The bundle ID for the kernel.
 */
extern const char KERNEL_ID[];

/*
 * struct kext
 *
 */
struct kext {
	// The kext's Mach-O file.
	struct macho macho;
	// The runtime base address.
	kaddr_t base;
	// The runtime offset between the static addresses in the Mach-O and the addresses in
	// kernel memory.
	kword_t slide;
	// The kext's bundle ID.
	const char *bundle_id;
	// The symtab command, used for resolving symbols.
	const struct symtab_command *symtab;
};

/*
 * KERNEL_PATH
 *
 * Description:
 * 	The path at which the kernel object lives on the filesystem.
 */
#ifndef KERNEL_PATH
# if KERNELCACHE
#  define KERNEL_PATH	"/System/Library/Caches/com.apple.kernelcaches/kernelcache"
# else
#  define KERNEL_PATH	"/System/Library/Kernels/kernel"
# endif
#endif // KERNEL_PATH

/*
 * kernel
 *
 * Description:
 * 	A kext object representing the kernel.
 */
extern struct kext kernel;

#if KERNELCACHE
/*
 * kernelcache
 *
 * Description:
 * 	The kernelcache.
 */
extern struct kernelcache kernelcache;
#endif

/*
 * kernel_init
 *
 * Description:
 * 	Initialize the kernel image subsystem.
 *
 * Parameters:
 * 		kernel_path		The path to the kernel to initialize, or NULL to use the
 * 					default value of KERNEL_PATH. If not NULL, this must point
 * 					to a string which will be live until the subsequent call to
 * 					kernel_deinit.
 *
 * Returns:
 * 	True if the kernel was initialized successfully.
 *
 * Notes:
 * 	This function can be called multiple times. The typical use case is to call this function
 * 	first when the kernel slide is not known, then initialize the kernel slide, then call this
 * 	function again to re-initialize the kernel subsystem with the correct kernel_slide. If the
 * 	kernel subsystem is being re-initialized with the same kernel path, this function always
 * 	succeeds.
 */
bool kernel_init(const char *kernel_path);

/*
 * kernel_deinit
 * 	Clean up resources used by the kernel image subsystem.
 */
void kernel_deinit(void);

/*
 * kext_result
 *
 * Description:
 * 	Result code for kext operations.
 */
typedef enum kext_result {
	KEXT_SUCCESS,
	KEXT_ERROR,
	KEXT_NO_KEXT,
	KEXT_NOT_FOUND,
	KEXT_NO_SYMBOLS,
} kext_result;

/*
 * kext_init_macho
 *
 * Description:
 * 	Initialize the Mach-O file for a kernel extension by bundle ID.
 *
 * Parameters:
 * 	out	macho			The macho struct to initialize.
 * 		bundle_id		The bundle ID of the kernel extension.
 *
 * Returns:
 * 	A kext_result code.
 */
kext_result kext_init_macho(struct macho *macho, const char *bundle_id);

/*
 * kext_deinit_macho
 *
 * Description:
 * 	Clean up a macho struct initialized with kext_init_macho.
 *
 * Parameters:
 * 		macho			The macho struct to clean up.
 */
void kext_deinit_macho(struct macho *macho);

/*
 * kext_find_base
 *
 * Description:
 * 	Find the runtime base address and slide of the given kernel extension.
 *
 * Parameters:
 * 		macho			The macho struct for the kernel extension.
 * 		bundle_id		The bundle ID of the kernel extension.
 * 	out	base			The runtime base address of the kernel extension.
 * 	out	slide			The slide between the static addresses in the Mach-O file
 * 					and the runtime addresses.
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 */
kext_result kext_find_base(struct macho *macho, const char *bundle_id, kaddr_t *base,
		kword_t *slide);

/*
 * kext_init
 *
 * Description:
 * 	Initialize a kext struct for the given kernel extension.
 *
 * Parameters:
 * 	out	kext			The kext struct to initialize.
 * 		bundle_id		The bundle ID of the kernel extension.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 */
kext_result kext_init(struct kext *kext, const char *bundle_id);

/*
 * kext_deinit
 *
 * Description:
 * 	Clean up the resources from kext_init.
 *
 * Parameters:
 * 		kext			The kext struct to clean up.
 */
void kext_deinit(struct kext *kext);

/*
 * kext_resolve_symbol
 *
 * Description:
 * 	Resolve the given symbol in the kext, returning the runtime address and size.
 *
 * Parameters:
 * 		kext			The kext struct for the kernel extension to search.
 * 		symbol			The (mangled) symbol name.
 * 	out	addr			The address of the symbol.
 * 	out	size			A guess of the size of the symbol.
 *
 * Returns:
 * 	KEXT_SUCCESS, KEXT_ERROR, KEXT_NOT_FOUND, or KEXT_NO_SYMBOLS.
 */
kext_result kext_resolve_symbol(const struct kext *kext, const char *symbol, kaddr_t *addr,
		size_t *size);

/*
 * kext_resolve_address
 *
 * Description:
 * 	Find which symbol in the kext contains the given address.
 *
 * Parameters:
 * 		kext			The kernel extension to search.
 * 		addr			The address to resolve.
 * 	out	name			The name of the symbol. Not valid after the kext's macho
 * 					struct has been deinitialized.
 * 	out	size			A guess of the size of the symbol.
 * 	out	offset			The offset from the start of the symbol to the given
 * 					address.
 *
 * Returns:
 * 	A kext_result code.
 */
kext_result kext_resolve_address(const struct kext *kext, kaddr_t addr, const char **name,
		size_t *size, size_t *offset);

/*
 * kext_search_data
 *
 * Description:
 * 	Search the binary data of the kext for the given byte sequence with at least the given
 * 	memory protections.
 *
 * Parameters:
 * 		kext			The kernel extension to search.
 * 		data			The data to search for.
 * 		size			The size of data.
 * 		minprot			The minimum memory protections the data must have.
 * 	out	addr			The runtime address corresponding to the static address of
 * 					the data in the kernel extension's binary.
 * Returns:
 * 	A kext_result code.
 *
 * Notes:
 * 	It is possible that the runtime data will be different from the static data in the binary.
 */
kext_result kext_search_data(const struct kext *kext, const void *data, size_t size,
		int minprot, kaddr_t *addr);

/*
 * kext_for_each_callback_fn
 *
 * Description:
 * 	A callback function for kext_for_each.
 *
 * Parameters:
 * 		context			The callback context.
 * 		bundle_id		The bundle ID of the current kernel extension.
 * 		base			The runtime base address of the kernel extension.
 * 		size			The size of the kernel extension in bytes. This may not be
 * 					correct on platforms with a kernelcache.
 *
 * Returns:
 * 	True to halt the iteration and cause kext_for_each to return, false to continue iteration.
 *
 * Notes:
 * 	On platforms with a kernel cache rather than dynamically loaded kernel extensions, the size
 * 	parameter will be the size of the first segment (before the split) rather than the true
 * 	binary size.
 */
typedef bool (*kext_for_each_callback_fn)(void *context, CFDictionaryRef info,
		const char *bundle_id, kaddr_t base, size_t size);

/*
 * kext_for_each
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
bool kext_for_each(kext_for_each_callback_fn callback, void *context);

/*
 * kext_containing_address
 *
 * Description:
 * 	Get the bundle ID of the kernel extension that contains the specified address.
 *
 * Parameters:
 * 		kaddr			The kernel address.
 * 	out	bundle_id		The bundle ID of the kext containing the address, or NULL.
 * 					The bundle ID should be freed when no longer needed.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 */
kext_result kext_containing_address(kaddr_t kaddr, char **bundle_id);

/*
 * kext_id_resolve_symbol
 *
 * Description:
 * 	Resolve the symbol in the kext with the given bundle identifier.
 *
 * Parameters:
 * 		bundle_id		The bundle ID of the kext.
 * 		symbol			The (mangled) symbol name.
 * 	out	addr			The address of the symbol.
 * 	out	size			A guess of the size of the symbol.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 */
kext_result kext_id_resolve_symbol(const char *bundle_id, const char *symbol, kaddr_t *addr,
		size_t *size);

/*
 * kernel_symbol
 *
 * Description:
 * 	Resolve a kernel symbol, returning the address and size.
 *
 * Returns:
 * 	True if the symbol was successfully resolved.
 */
static inline kext_result
kernel_symbol(const char *symbol, kaddr_t *addr, size_t *size) {
	return kext_resolve_symbol(&kernel, symbol, addr, size);
}

/*
 * kernel_and_kexts_resolve_symbol
 *
 * Description:
 * 	Search through the kernel and all loaded kernel extensions for the given symbol.
 *
 * Parameters:
 * 		symbol			The (mangled) symbol name.
 * 	out	addr			The address of the symbol.
 * 	out	size			A guess of the size of the symbol.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 *
 * Notes:
 * 	Searching through all kernel extensions is a very slow operation.
 */
kext_result kernel_and_kexts_resolve_symbol(const char *symbol, kaddr_t *addr, size_t *size);

/*
 * kernel_and_kexts_search_data
 *
 * Description:
 * 	Search through the binary images of the kernel and all loaded kernel extensions for the
 * 	given data.
 *
 * Parameters:
 * 		data			The data to search for.
 * 		size			The size of data.
 * 		minprot			The minimum memory protections the data must have.
 * 	out	addr			The runtime address corresponding to the static address of
 * 					the data in the kernel extension's binary.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 *
 * Notes:
 * 	Searching through all kernel extensions is a very slow operation.
 *
 * 	It is possible that the runtime data will be different from the static data in the binary.
 */
kext_result kernel_and_kexts_search_data(const void *data, size_t size, int minprot,
		kaddr_t *addr);

/*
 * resolve_symbol
 *
 * Description:
 * 	Search for the given symbol.
 *
 * Parameters:
 * 		bundle_id		The bundle ID of the kext to search in, or NULL to search
 * 					through all kernel extensions.
 * 		symbol			The (mangled) symbol name.
 * 	out	addr			The address of the symbol.
 * 	out	size			A guess of the size of the symbol.
 *
 * Returns:
 * 	A kext_result code.
 *
 * Dependencies:
 * 	kernel_slide
 *
 * Notes:
 * 	See kernel_and_kexts_resolve_symbol.
 */
kext_result resolve_symbol(const char *bundle_id, const char *symbol, kaddr_t *addr, size_t *size);

#endif
