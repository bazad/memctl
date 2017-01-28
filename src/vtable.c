#include "vtable.h"

#include "kernel.h"
#include "memctl_error.h"

#include <stdio.h>
#include <string.h>

bool
vtable_for_class(const char *class_name, const char *bundle_id, kaddr_t *vtable, size_t *size) {
	*vtable = 0;
	size_t len = strlen(class_name);
	char *vtable_symbol = NULL;
	asprintf(&vtable_symbol, "__ZTV%zu%s", len, class_name);
	if (vtable_symbol == NULL) {
		error_out_of_memory();
		return false;
	}
	kext_result kr = resolve_symbol(bundle_id, vtable_symbol, vtable, size);
	free(vtable_symbol);
	if (kr != KEXT_SUCCESS) {
		if (kr == KEXT_NOT_FOUND || kr == KEXT_NO_SYMBOLS) {
			return true;
		} else if (kr == KEXT_NO_KEXT) {
			error_kext_not_found(bundle_id);
		}
		return false;
	}
	*vtable += 0x10;
	if (size != NULL) {
		*size -= 0x10;
	}
	return true;
}
