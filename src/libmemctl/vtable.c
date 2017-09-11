#include "memctl/vtable.h"

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"
#include "memctl/utility.h"

#include "mangle.h"
#include "memctl_common.h"

#include <stdio.h>
#include <string.h>

/*
 * adjust_vtable_from_symbol
 *
 * Description:
 * 	Adjust the vtable address and size found using a symbol lookup.
 */
static void
adjust_vtable_from_symbol(kaddr_t *vtable, size_t *size) {
	*vtable += VTABLE_OFFSET_SIZE;
	if (size != NULL) {
		*size -= VTABLE_OFFSET_SIZE;
	}
}

kext_result
vtable_for_class(const char *class_name, const char *bundle_id, kaddr_t *vtable, size_t *size) {
	// Generate the vtable symbol.
	size_t symbol_size = mangle_class_vtable(NULL, 0, &class_name, 1) + 1;
	char symbol[symbol_size];
	mangle_class_vtable(symbol, symbol_size, &class_name, 1);
	// Search all the kexts.
	kext_result kr = resolve_symbol(bundle_id, symbol, vtable, size);
	if (kr == KEXT_SUCCESS) {
		adjust_vtable_from_symbol(vtable, size);
	}
	return kr;
}

kext_result
vtable_lookup(kaddr_t vtable, char **class_name, size_t *offset) {
	// Resolve the vtable address into a symbol.
	const struct kext *kext;
	kext_result kr = kernel_kext_containing_address(&kext, vtable);
	if (kr != KEXT_SUCCESS) {
		goto fail_0;
	}
	const char *symbol;
	kr = kext_resolve_address(kext, vtable, &symbol, NULL, offset);
	if (kr != KEXT_SUCCESS) {
		goto fail_1;
	}
	// Introduce a new scope for buf so that we can jump to the final labels.
	{
		// Get the class name from the vtable symbol.
		char buf[strlen(symbol) + 1];
		strcpy(buf, symbol);
		char *classname;
		size_t count = demangle_class_vtable(&classname, 1, buf);
		if (count != 1) {
			assert(count == 0);
			kr = KEXT_NOT_FOUND;
			goto fail_1;
		}
		// Return the class name to the client.
		*class_name = strdup(classname);
	}
	if (*class_name == NULL) {
		error_out_of_memory();
		kr = KEXT_ERROR;
		goto fail_1;
	}
fail_1:
	kext_release(kext);
fail_0:
	return kr;
}
