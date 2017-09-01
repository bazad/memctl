#ifndef MEMCTL__KERNEL_H_
#define MEMCTL__KERNEL_H_
/*
 * Kernel and kernel extension routines.
 *
 * The kernel subsystem maintains a mapping of bundle identifiers to kext structs. Each kext struct
 * stores basic information about the kernel extension (or pseudoextension): the runtime address,
 * the runtime slide, the kext's Mach-O file, the symbol table, etc. This mapping is initialized in
 * kernel_init(). When kernel_kext() is called for a kext that has not yet been initialized, it is
 * added to the mapping. Subsequent calls to kernel_kext will return this previously-created
 * object. kernel_deinit() will free this mapping, and all kext objects will be invalidated.
 */

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
 * Description:
 * 	Basic information about the kernel or a kernel extension.
 */
struct kext {
	// The kext's bundle ID.
	const char *bundle_id;
	// The runtime base address.
	kaddr_t base;
	// The runtime offset between the static addresses in the Mach-O and the addresses in
	// kernel memory.
	kword_t slide;
	// The kext's Mach-O file.
	struct macho macho;
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
 * 	The kernelcache. Only available on platforms with a kernelcache.
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
 * 					to a string which will remain live until the subsequent
 * 					call to kernel_deinit.
 *
 * Returns:
 * 	True if no errors were encountered and the kernel was initialized successfully.
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
 * 	Clean up resources used by the kernel image subsystem. All kext objects are invalidated by
 * 	this call and must not be used afterwards.
 */
void kernel_deinit(void);

/*
 * kext_result
 *
 * Description:
 * 	Result code for kext operations.
 */
typedef enum kext_result {
	// Success.
	KEXT_SUCCESS,
	// An error was encountered, and an error was pushed onto the error stack.
	KEXT_ERROR,
	// The kext was not found.
	KEXT_NO_KEXT,
	// The item was not found in the kext.
	KEXT_NOT_FOUND,
} kext_result;

/*
 * kernel_kext
 *
 * Description:
 * 	Load the kext struct for the kernel or kernel extension with the given bundle ID. If the
 * 	bundle ID has previously been loaded (and has not been invalidated because the kext has
 * 	been unloaded on the system), then this function will always succeed. Once the kext is no
 * 	longer needed, release it with kext_release.
 *
 * Parameters:
 * 	out	kext			On return, a pointer to the kext struct.
 * 		bundle_id		The bundle ID of the kernel or kernel extension to load.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_ERROR			An error was encountered.
 * 	KEXT_NO_KEXT			No kernel extension with the given bundle ID was found.
 *
 * Notes:
 * 	This call is not thread safe.
 */
kext_result kernel_kext(const struct kext **kext, const char *bundle_id);

/*
 * kext_release
 *
 * Description:
 * 	Release a reference to a kext obtained with kernel_kext.
 *
 * Parameters:
 * 		kext			The kext struct.
 */
void kext_release(const struct kext *kext);

/*
 * kext_find_symbol_fn
 *
 * Description:
 * 	The type for a kext analyzer function to find a symbol. Pass a function of this type to
 * 	kext_add_symbol_finder to add additional symbol finding strategies to kext_find_symbol.
 *
 * Parameters:
 * 		kext			The kext.
 * 		symbol			The symbol being resolved.
 * 	out	addr			On return, the runtime address of the symbol, if found.
 * 	out	size			On return, a guess of the size of the symbol, if found. If
 * 					no guess of the symbol's size is available, do not set this
 * 					parameter and a default upper bound will be used.
 *
 * Returns:
 * 	KEXT_SUCCESS, KEXT_ERROR, or KEXT_NOT_FOUND.
 */
typedef kext_result (*kext_find_symbol_fn)(
		const struct kext *kext, const char *symbol, kaddr_t *addr, size_t *size);

/*
 * kext_add_symbol_finder
 *
 * Description:
 * 	Add a special kext symbol finding function to be associated with the specified kext or with
 * 	all kexts if bundle_id is NULL. This allows clients to specify specialized strategies for
 * 	finding symbols that are not present in the symbol table. See kext_find_symbol.
 *
 * Parameters:
 * 		bundle_id		The bundle ID to associate this resolver with, or NULL for
 * 					all kexts. The caller is responsible for ensuring the
 * 					string is live.
 * 		find_symbol		The special symbol finding function.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Notes:
 * 	Currently there is no way to remove a symbol finder added with this function.
 */
bool kext_add_symbol_finder(const char *bundle_id, kext_find_symbol_fn find_symbol);

/*
 * kext_find_symbol
 *
 * Description:
 * 	Try to find the address of a symbol by running additional kext analyzers after traditional
 * 	symbol resolution. If kext_resolve_symbol returns KEXT_NOT_FOUND, then kext_find_symbol
 * 	will run each registered symbol finder matching the specified kext in turn until some
 * 	finder returns KEXT_SUCCESS or KEXT_ERROR.
 *
 * Parameters:
 * 		kext			The kext.
 * 		symbol			The (mangled) symbol name.
 * 	out	addr			The runtime address of the symbol.
 * 	out	size			A guess of the size of the symbol.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NOT_FOUND			The symbol was not found.
 * 	KEXT_ERROR			An error was encountered.
 */
kext_result kext_find_symbol(const struct kext *kext, const char *symbol,
		kaddr_t *addr, size_t *size);

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
 * 					struct has been deinitialized. May be NULL.
 * 	out	size			A guess of the size of the symbol. May be NULL.
 * 	out	offset			The offset from the start of the symbol to the given
 * 					address. May be NULL.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NOT_FOUND			The symbol for the address could not be found.
 * 	KEXT_ERROR			An error was encountered.
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
 * 		address			The kernel address.
 * 	out	bundle_id		The bundle ID of the kext containing the address, or NULL.
 * 					The bundle ID should be freed when no longer needed.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NO_KEXT			No kernel extension contains the given address.
 * 	KEXT_ERROR			An error was encountered.
 *
 * Dependencies:
 * 	kernel_slide
 */
kext_result kext_containing_address(kaddr_t address, char **bundle_id);

// ---- Convenience functions ---------------------------------------------------------------------

/*
 * kernel_kext_containing_address
 *
 * Description:
 * 	Load the kext struct for the kernel or kernel extension containing the specified kernel
 * 	virtual address. The kext must be released with kext_release when no longer needed.
 *
 * Parameters:
 * 	out	kext			On return, a pointer to the kext struct.
 * 		address			The address to search for.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NO_KEXT			No kernel extension contains the given address.
 * 	KEXT_ERROR			An error was encountered.
 *
 * Dependencies:
 * 	kernel_slide
 */
kext_result kernel_kext_containing_address(const struct kext **kext, kaddr_t address);

/*
 * kext_id_find_symbol
 *
 * Description:
 * 	Find the symbol in the kext with the given bundle identifier.
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
kext_result kext_id_find_symbol(const char *bundle_id, const char *symbol, kaddr_t *addr,
		size_t *size);

/*
 * kernel_symbol
 *
 * Description:
 * 	Find a kernel symbol, returning the address and size.
 *
 * Returns:
 * 	KEXT_SUCCESS, KEXT_ERROR, or KEXT_NOT_FOUND.
 */
static inline kext_result
kernel_symbol(const char *symbol, kaddr_t *addr, size_t *size) {
	return kext_find_symbol(&kernel, symbol, addr, size);
}

/*
 * kernel_and_kexts_find_symbol
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
kext_result kernel_and_kexts_find_symbol(const char *symbol, kaddr_t *addr, size_t *size);

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
 * 	Find the given symbol. Special symbol finders are checked in addition to standard symbol
 * 	resolution.
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
 * 	See kernel_and_kexts_find_symbol.
 *
 * 	Even though this function is called "resolve_symbol", the behavior is that of
 * 	"kext_find_symbol": special symbol finders are checked.
 */
kext_result resolve_symbol(const char *bundle_id, const char *symbol, kaddr_t *addr, size_t *size);

#endif
