#include "memctl/kernel.h"

#include "memctl/kernel_slide.h"
#include "memctl/memctl_error.h"

#if KERNELCACHE
#include "memctl/kernelcache.h"
#else
#include "memctl/oskext.h"
#endif

#include "memctl_common.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

const char KERNEL_ID[] = "__kernel__";

struct kext kernel;
#if KERNELCACHE
struct kernelcache kernelcache;
#endif

/*
 * struct kext_analyzers
 *
 * Description:
 * 	A struct to keep track of the analyzers for a particular kernel extension.
 */
struct kext_analyzers {
	// The bundle ID. This string is managed by the caller who created this analyzer.
	const char *bundle_id;
	// An array of symbol finders.
	kext_find_symbol_fn *find_symbol_fn;
	// The number of elements in the find_symbol_fn array.
	size_t find_symbol_count;
};

// The path of the currently initialized kernel.
static const char *initialized_kernel = NULL;

// The array of kext analyzers.
struct kext_analyzers *all_analyzers;

// The number of kext analyzers.
size_t all_analyzers_count;

/*
 * is_kernel_id
 *
 * Description:
 * 	Returns true if the given kext bundle identifier is for the kernel.
 */
static bool
is_kernel_id(const char *bundle_id) {
	return (strcmp(bundle_id, KERNEL_ID) == 0);
}

/*
 * clear_kext_analyzers
 *
 * Description:
 * 	Clear the specified kext_analyzers struct and free associated resources.
 */
static void
clear_kext_analyzers(struct kext_analyzers *ka) {
	free(ka->find_symbol_fn);
	ka->find_symbol_fn    = NULL;
	ka->find_symbol_count = 0;
}

/*
 * clear_all_analyzers
 *
 * Description:
 * 	Remove all analyzers and free associated resources.
 */
static void
clear_all_analyzers() {
	for (size_t i = 0; i < all_analyzers_count; i++) {
		clear_kext_analyzers(&all_analyzers[i]);
	}
	free(all_analyzers);
	all_analyzers       = NULL;
	all_analyzers_count = 0;
}

#if KERNELCACHE

/*
 * kernel_init_kernelcache
 *
 * Description:
 * 	Initialize kernelcache, kernel.macho, and kernel.base.
 */
static bool
kernel_init_kernelcache(const char *kernelcache_path) {
	kext_result kr = kernelcache_init_file(&kernelcache, kernelcache_path);
	if (kr != KEXT_SUCCESS) {
		assert(kr == KEXT_ERROR);
		return false;
	}
	kernel.macho = kernelcache.kernel;
	kernel.base = kernelcache.text->vmaddr + kernel_slide;
	return true;
}

#else

/*
 * kernel_init_macos
 *
 * Description:
 * 	Initialize kernel.macho and kernel.base.
 */
static bool
kernel_init_macos(const char *kernel_path) {
	// mmap the kernel file.
	if (!mmap_file(kernel_path, (const void **)&kernel.macho.mh, &kernel.macho.size)) {
		// kernel_deinit will be called.
		assert(kernel.macho.mh == NULL);
		return false;
	}
	macho_result mr = macho_validate(kernel.macho.mh, kernel.macho.size);
	if (mr != MACHO_SUCCESS) {
		error_internal("%s is not a valid Mach-O file", kernel_path);
		return false;
	}
	// Set the runtime base and slide.
	uint64_t static_base;
	mr = macho_find_base(&kernel.macho, &static_base);
	if (mr != MACHO_SUCCESS) {
		error_internal("%s does not have a Mach-O base address", kernel_path);
		return false;
	}
	kernel.base = static_base + kernel_slide;
	return true;
}

#endif

bool
kernel_init(const char *kernel_path) {
	if (kernel_path == NULL) {
		kernel_path = KERNEL_PATH;
	}
	if (initialized_kernel != NULL) {
		if (strcmp(kernel_path, initialized_kernel) == 0) {
			// Reset the runtime base and slide based on the new value of kernel_slide.
			kaddr_t static_base = kernel.base - kernel.slide;
			kernel.base  = static_base + kernel_slide;
			kernel.slide = kernel_slide;
			return true;
		}
		kernel_deinit();
	}
#if KERNELCACHE
	if (!kernel_init_kernelcache(kernel_path)) {
		goto fail;
	}
#else
	if (!kernel_init_macos(kernel_path)) {
		goto fail;
	}
#endif
	kernel.slide = kernel_slide;
	kernel.bundle_id = KERNEL_ID;
	// Initialize the symtab.
	kernel.symtab = (const struct symtab_command *)macho_find_load_command(&kernel.macho, NULL,
			LC_SYMTAB);
	initialized_kernel = kernel_path;
	return true;
fail:
	kernel_deinit();
	return false;
}

void
kernel_deinit() {
	initialized_kernel = NULL;
#if KERNELCACHE
	if (kernelcache.kernel.mh != NULL) {
		kernelcache_deinit(&kernelcache);
		kernel.macho.mh = NULL;
	}
#else
	if (kernel.macho.mh != NULL) {
		munmap(kernel.macho.mh, kernel.macho.size);
		kernel.macho.mh = NULL;
	}
#endif
	clear_all_analyzers();
}


kext_result
kext_init_macho(struct macho *macho, const char *bundle_id) {
	assert(bundle_id != NULL);
	if (is_kernel_id(bundle_id)) {
		*macho = kernel.macho;
		return KEXT_SUCCESS;
	}
#if KERNELCACHE
	return kernelcache_kext_init_macho(&kernelcache, macho, bundle_id);
#else
	return oskext_init_macho(macho, bundle_id);
#endif
}

void
kext_deinit_macho(struct macho *macho) {
#if !KERNELCACHE
	if (macho->mh != kernel.macho.mh) {
		oskext_deinit_macho(macho);
	}
#endif
}

kext_result
kext_find_base(struct macho *macho, const char *bundle_id, kaddr_t *base, kword_t *slide) {
	assert(macho->mh != kernel.macho.mh);
	uint64_t static_base;
	macho_result mr = macho_find_base(macho, &static_base);
	if (mr != MACHO_SUCCESS) {
		error_internal("could not find Mach-O base address for kext %s", bundle_id);
		return KEXT_ERROR;
	}
#if KERNELCACHE
	*base = static_base + kernel_slide;
	*slide = kernel_slide;
#else
	// oskext_get_address depends on kernel_slide.
	kaddr_t runtime_base;
	kext_result kr = oskext_get_address(bundle_id, &runtime_base, NULL);
	if (kr != KEXT_SUCCESS) {
		return kr;
	}
	*base = runtime_base;
	*slide = runtime_base - static_base;
#endif
	return KEXT_SUCCESS;
}

kext_result
kext_init(struct kext *kext, const char *bundle_id) {
	assert(bundle_id != NULL);
	if (is_kernel_id(bundle_id)) {
		*kext = kernel;
		return KEXT_SUCCESS;
	}
	kext_result kr = kext_init_macho(&kext->macho, bundle_id);
	if (kr != KEXT_SUCCESS) {
		return kr;
	}
	kr = kext_find_base(&kext->macho, bundle_id, &kext->base, &kext->slide);
	if (kr != KEXT_SUCCESS) {
		goto fail;
	}
	kext->bundle_id = strdup(bundle_id);
	if (kext->bundle_id == NULL) {
		error_out_of_memory();
		kr = KEXT_ERROR;
		goto fail;
	}
	kext->symtab = (const struct symtab_command *)macho_find_load_command(&kext->macho, NULL,
			LC_SYMTAB);
	return KEXT_SUCCESS;
fail:
	kext_deinit_macho(&kext->macho);
	return KEXT_ERROR;
}

void
kext_deinit(struct kext *kext) {
	if (kext->macho.mh == kernel.macho.mh) {
		return;
	}
	kext_deinit_macho(&kext->macho);
	free((char *)kext->bundle_id);
}

/*
 * find_kext_analyzers
 *
 * Description:
 * 	Find the existing kext_analyzers struct for the given bundle ID.
 */
static struct kext_analyzers *
find_kext_analyzers(const char *bundle_id, size_t *insert_index) {
	struct kext_analyzers *ka = all_analyzers;
	size_t count = all_analyzers_count;
	ssize_t left  = 0;
	ssize_t right = count - 1;
	for (;;) {
		if (right < left) {
			if (insert_index != NULL) {
				*insert_index = left;
			}
			return NULL;
		}
		ssize_t mid = (left + right) / 2;
		int cmp = strcmp(bundle_id, ka[mid].bundle_id);
		if (cmp < 0) {
			right = mid - 1;
		} else if (cmp > 0) {
			left = mid + 1;
		} else {
			// There's only one value that's equal.
			return &ka[mid];
		}
	}
}

/*
 * create_kext_analyzers
 *
 * Description:
 * 	Find the kext_analyzers struct for the given bundle ID, or create one if it doesn't already
 * 	exist.
 */
static struct kext_analyzers *
create_kext_analyzers(const char *bundle_id) {
	if (bundle_id == NULL) {
		bundle_id = "";
	}
	// Try to find the kext_analyzers struct if it already exists.
	size_t insert_index;
	struct kext_analyzers *ka = find_kext_analyzers(bundle_id, &insert_index);
	if (ka != NULL) {
		return ka;
	}
	// Grow the all_analyzers array by one.
	size_t count = all_analyzers_count + 1;
	struct kext_analyzers *analyzers = realloc(all_analyzers, count * sizeof(*analyzers));
	if (analyzers == NULL) {
		error_out_of_memory();
		return NULL;
	}
	all_analyzers = analyzers;
	all_analyzers_count = count;
	// Make room for the new kext_analyzers in the array.
	ka = &analyzers[insert_index];
	size_t shift_count = (count - 1) - insert_index;
	memmove(ka + 1, ka, shift_count * sizeof(*analyzers));
	// Fill in the new kext_analyzers.
	ka->bundle_id         = bundle_id;
	ka->find_symbol_fn    = NULL;
	ka->find_symbol_count = 0;
	return ka;
}

/*
 * kext_analyzers_insert_symbol_finder
 *
 * Description:
 * 	Add a symbol finder to the kext_analyzers struct.
 */
static bool
kext_analyzers_insert_symbol_finder(struct kext_analyzers *ka, kext_find_symbol_fn find_symbol) {
	// Allocate space for the new symbol finder.
	size_t count = ka->find_symbol_count + 1;
	kext_find_symbol_fn *fn = realloc(ka->find_symbol_fn, count * sizeof(*fn));
	if (fn == NULL) {
		error_out_of_memory();
		return false;
	}
	// Insert the symbol finder.
	ka->find_symbol_fn    = fn;
	ka->find_symbol_count = count;
	fn[count - 1] = find_symbol;
	return true;
}

bool
kext_add_symbol_finder(const char *bundle_id, kext_find_symbol_fn find_symbol) {
	struct kext_analyzers *ka = create_kext_analyzers(bundle_id);
	if (ka == NULL) {
		return false;
	}
	return kext_analyzers_insert_symbol_finder(ka, find_symbol);
}

/*
 * run_symbol_finders
 *
 * Description:
 * 	Run the symbol finders from the given kext_analyzers struct.
 */
static kext_result
run_symbol_finders(const struct kext_analyzers *ka, const struct kext *kext,
		const char *symbol, kaddr_t *addr, size_t *size) {
	kext_find_symbol_fn *find_symbol = ka->find_symbol_fn;
	kext_find_symbol_fn *end = find_symbol + ka->find_symbol_count;
	kext_result kr = KEXT_NOT_FOUND;
	for (; kr == KEXT_NOT_FOUND && find_symbol < end; find_symbol++) {
		kr = (*find_symbol)(kext, symbol, addr, size);
	}
	return kr;
}

/*
 * run_matching_symbol_finders
 *
 * Description:
 * 	Run the symbol finders matching the given kext.
 */
static kext_result
run_matching_symbol_finders(const struct kext *kext, const char *symbol,
		kaddr_t *addr, size_t *size) {
	assert(kext->bundle_id != NULL);
	const char *bundle_ids[2] = { kext->bundle_id, "" };
	kext_result kr = KEXT_NOT_FOUND;
	for (size_t i = 0; kr == KEXT_NOT_FOUND && i < 2; i++) {
		struct kext_analyzers *ka = find_kext_analyzers(bundle_ids[i], NULL);
		if (ka != NULL) {
			kr = run_symbol_finders(ka, kext, symbol, addr, size);
		}
	}
	return kr;
}

kext_result
kext_find_symbol(const struct kext *kext, const char *symbol, kaddr_t *addr, size_t *size) {
	kext_result kr = kext_resolve_symbol(kext, symbol, addr, size);
	if (kr == KEXT_NOT_FOUND || kr == KEXT_NO_SYMBOLS) {
		kr = run_matching_symbol_finders(kext, symbol, addr, size);
	}
	return kr;
}

kext_result
kext_resolve_symbol(const struct kext *kext, const char *symbol, kaddr_t *addr, size_t *size) {
	if (kext->symtab == NULL) {
		return KEXT_NO_SYMBOLS;
	}
	uint64_t static_addr;
	macho_result mr = macho_resolve_symbol(&kext->macho, kext->symtab, symbol, &static_addr,
			size);
	if (mr != MACHO_SUCCESS) {
		if (mr == MACHO_NOT_FOUND) {
			return KEXT_NOT_FOUND;
		}
		assert(mr == MACHO_ERROR);
		return KEXT_ERROR;
	}
	*addr = static_addr + kext->slide;
	return KEXT_SUCCESS;
}

kext_result
kext_resolve_address(const struct kext *kext, kaddr_t addr, const char **name, size_t *size,
		size_t *offset) {
	if (kext->symtab == NULL) {
		return KEXT_NO_SYMBOLS;
	}
	uint64_t static_addr = addr - kext->slide;
	macho_result mr = macho_resolve_address(&kext->macho, kext->symtab, static_addr, name,
			size, offset);
	if (mr != MACHO_SUCCESS) {
		if (mr == MACHO_NOT_FOUND) {
			*name = NULL;
			*size = 0;
			*offset = addr - kext->base;
			return KEXT_NOT_FOUND;
		}
		assert(mr == MACHO_ERROR);
		return KEXT_ERROR;
	}
	return KEXT_SUCCESS;
}

kext_result
kext_search_data(const struct kext *kext, const void *data, size_t size, int minprot, kaddr_t *addr) {
	uint64_t static_addr;
	macho_result mr = macho_search_data(&kext->macho, data, size, minprot, &static_addr);
	if (mr != MACHO_SUCCESS) {
		if (mr == MACHO_NOT_FOUND) {
			return KEXT_NOT_FOUND;
		}
		assert(mr == MACHO_ERROR);
		return KEXT_ERROR;
	}
	*addr = static_addr + kext->slide;
	return KEXT_SUCCESS;
}

#if KERNELCACHE

/*
 * struct kext_for_each_kernelcache_context
 *
 * Description:
 * 	Callback context for kext_for_each.
 */
struct kext_for_each_kernelcache_context {
	kext_for_each_callback_fn callback;
	void *context;
};

/*
 * kext_for_each_kernelcache_callback
 *
 * Description:
 * 	Callback for kext_for_each. This callback only exists in order to add the kernel_slide to
 * 	the base parameter.
 */
static bool
kext_for_each_kernelcache_callback(void *context, CFDictionaryRef info,
		const char *bundle_id, kaddr_t base, size_t size) {
	struct kext_for_each_kernelcache_context *c = context;
	if (base != 0) {
		base += kernel_slide;
	}
	return c->callback(c->context, info, bundle_id, base, size);
}

#endif

bool
kext_for_each(kext_for_each_callback_fn callback, void *context) {
#if KERNELCACHE
	// TODO: This indirection is annoying. I've implemented it this way rather than having
	// kernelcache_for_each add the kernel_slide automatically because kernelcache is supposed
	// to handle only static information, it should know nothing about the runtime.
	struct kext_for_each_kernelcache_context context0 = { callback, context };
	kernelcache_for_each(&kernelcache, kext_for_each_kernelcache_callback, &context0);
	return true;
#else
	return oskext_for_each(callback, context);
#endif
}

kext_result
kext_containing_address(kaddr_t kaddr, char **bundle_id) {
#if KERNELCACHE
	return kernelcache_find_containing_address(&kernelcache, kaddr - kernel_slide, bundle_id,
			NULL);
#else
	return oskext_find_containing_address(kaddr, bundle_id, NULL, NULL);
#endif
}

kext_result
kext_id_resolve_symbol(const char *bundle_id, const char *symbol, kaddr_t *addr, size_t *size) {
	struct kext kext;
	kext_result kr = kext_init(&kext, bundle_id);
	if (kr == KEXT_SUCCESS) {
		kr = kext_resolve_symbol(&kext, symbol, addr, size);
		kext_deinit(&kext);
	}
	return kr;
}

/*
 * struct kernel_and_kexts_resolve_symbol_context
 *
 * Description:
 * 	Callback context for kernel_and_kexts_resolve_symbol.
 */
struct kernel_and_kexts_resolve_symbol_context {
	const char *symbol;
	kaddr_t addr;
	size_t size;
};

/*
 * kernel_and_kexts_resolve_symbol_callback
 *
 * Description:
 * 	A callback for kernel_and_kexts_resolve_symbol that tries to resolve a symbol in the given
 * 	kernel extension.
 */
static bool
kernel_and_kexts_resolve_symbol_callback(void *context, CFDictionaryRef info,
		const char *bundle_id, kaddr_t base0, size_t size0) {
	if (bundle_id == NULL) {
		return false;
	}
	struct kernel_and_kexts_resolve_symbol_context *c = context;
	error_stop();
	kext_result kr = kext_id_resolve_symbol(bundle_id, c->symbol, &c->addr, &c->size);
	error_start();
	return (kr == KEXT_SUCCESS);
}

kext_result
kernel_and_kexts_resolve_symbol(const char *symbol, kaddr_t *addr, size_t *size) {
	struct kernel_and_kexts_resolve_symbol_context context = { symbol };
	bool success = kext_for_each(kernel_and_kexts_resolve_symbol_callback, &context);
	if (!success) {
		return KEXT_ERROR;
	}
	if (context.addr == 0 && context.size == 0) {
		return KEXT_NOT_FOUND;
	}
	*addr = context.addr;
	*size = context.size;
	return KEXT_SUCCESS;
}

/*
 * struct kernel_and_kexts_search_data_context
 *
 * Description:
 * 	Callback context for kernel_and_kexts_search_data.
 */
struct kernel_and_kexts_search_data_context {
	const void *data;
	size_t size;
	int minprot;
	kaddr_t addr;
};

/*
 * kernel_and_kexts_search_data_callback
 *
 * Description:
 * 	A callback for kernel_and_kexts_search_data that tries to find the data in the given
 * 	kernel extension.
 */
static bool
kernel_and_kexts_search_data_callback(void *context, CFDictionaryRef info, const char *bundle_id,
		kaddr_t base0, size_t size0) {
	if (bundle_id == NULL) {
		return false;
	}
	struct kernel_and_kexts_search_data_context *c = context;
	struct kext kext;
	error_stop();
	kext_result kr = kext_init(&kext, bundle_id);
	if (kr != KEXT_SUCCESS) {
		goto fail;
	}
	kr = kext_search_data(&kext, c->data, c->size, c->minprot, &c->addr);
	kext_deinit(&kext);
	if (kr != KEXT_SUCCESS) {
		goto fail;
	}
	error_start();
	return true;
fail:
	error_start();
	return false;

}

kext_result
kernel_and_kexts_search_data(const void *data, size_t size, int minprot, kaddr_t *addr) {
	struct kernel_and_kexts_search_data_context context = { data, size, minprot };
	bool success = kext_for_each(kernel_and_kexts_search_data_callback, &context);
	if (!success) {
		return KEXT_ERROR;
	}
	if (context.addr == 0) {
		return KEXT_NOT_FOUND;
	}
	*addr = context.addr;
	return KEXT_SUCCESS;
}

kext_result
resolve_symbol(const char *bundle_id, const char *symbol, kaddr_t *addr, size_t *size) {
	if (bundle_id == NULL) {
		return kernel_and_kexts_resolve_symbol(symbol, addr, size);
	} else {
		return kext_id_resolve_symbol(bundle_id, symbol, addr, size);
	}
}
