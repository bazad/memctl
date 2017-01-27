#include "kernel.h"

#include "kernel_slide.h"
#include "memctl_common.h"
#include "memctl_error.h"

#if KERNELCACHE
#include "kernelcache.h"
#else
#include "oskext.h"
#endif

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
 * initialized_kernel
 *
 * Description:
 * 	The path of the currently initialized kernel.
 */
static const char *initialized_kernel = NULL;

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
	kernel.symtab = NULL;
	macho_result mr = macho_find_load_command(&kernel.macho,
			(const struct load_command **)&kernel.symtab, LC_SYMTAB);
	if (mr != MACHO_SUCCESS) {
		assert(mr == MACHO_ERROR);
		goto fail;
	}
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
	kext->symtab = NULL;
	macho_result mr = macho_find_load_command(&kext->macho,
			(const struct load_command **)&kext->symtab, LC_SYMTAB);
	if (mr != MACHO_SUCCESS) {
		assert(mr == MACHO_ERROR);
		kr = KEXT_ERROR;
		goto fail;
	}
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
	return c->callback(c->context, info, bundle_id, base + kernel_slide, size);
}

#endif

bool
kext_for_each(kext_for_each_callback_fn callback, void *context) {
#if KERNELCACHE
	// TODO: This indirection is annoying. I've implemented it this way rather than having
	// kernelcache_for_each add the kernel_slide automatically because kernelcache is supposed
	// to handle only static information, it should know nothing about the runtime.
	return kernelcache_for_each(&kernelcache, kext_for_each_kernelcache_callback, context);
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
	struct kext kext;
	error_stop();
	kext_result kr = kext_init(&kext, bundle_id);
	if (kr != KEXT_SUCCESS) {
		goto fail;
	}
	kr = kext_resolve_symbol(&kext, c->symbol, &c->addr, &c->size);
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
 * kernel_and_kexts_resolve_symbol_callback
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
