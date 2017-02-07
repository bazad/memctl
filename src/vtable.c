#include "vtable.h"

#include "kernel.h"
#include "memctl_error.h"

#include <stdio.h>
#include <string.h>

#if KERNELCACHE

/*
 * vtable_for_class_kernelcache
 *
 * Description:
 * 	Scan the kernelcache looking for the vtable for the given class.
 */
static bool
vtable_for_class_kernelcache(const char *class_name, const char *bundle_id, kaddr_t *vtable,
		size_t *size) {
	*vtable = 0;
	error_internal("not implemented");// TODO
	return false;
}

#else

/*
 * vtable_symbol
 *
 * Description:
 * 	Generate the symbol name for the vtable of the specified class.
 */
char *
vtable_symbol(const char *class_name) {
	char *symbol;
	asprintf(&symbol, "__ZTV%zu%s", strlen(class_name), class_name);
	if (symbol == NULL) {
		error_out_of_memory();
	}
	return symbol;
}

/*
 * vtable_for_class_macos
 *
 * Description:
 * 	Look up the vtable for the given class in the macOS kernel by its symbol.
 */
static bool
vtable_for_class_macos(const char *class_name, const char *bundle_id, kaddr_t *vtable,
		size_t *size) {
	*vtable = 0;
	// Generate the vtable symbol.
	char *symbol = vtable_symbol(class_name);
	if (symbol == NULL) {
		return false;
	}
	kext_result kr = resolve_symbol(bundle_id, symbol, vtable, size);
	free(symbol);
	if (kr != KEXT_SUCCESS) {
		if (kr == KEXT_NOT_FOUND || kr == KEXT_NO_SYMBOLS) {
			return true;
		} else if (kr == KEXT_NO_KEXT) {
			error_kext_not_found(bundle_id);
		}
		return false;
	}
	*vtable += 2 * sizeof(kword_t);
	if (size != NULL) {
		*size -= 0x10;
	}
	return true;
}

#endif

bool
vtable_for_class(const char *class_name, const char *bundle_id, kaddr_t *vtable, size_t *size) {
#if KERNELCACHE
	return vtable_for_class_kernelcache(class_name, bundle_id, vtable, size);
#else
	return vtable_for_class_macos(class_name, bundle_id, vtable, size);
#endif
}
