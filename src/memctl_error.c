#include "memctl_error.h"

#include <assert.h>
#include <string.h>

static size_t
len(const char *str) {
	return (str == NULL ? 0 : strlen(str) + 1);
}

void
error_open(const char *path, int errnum) {
	assert(path != NULL);
	size_t path_len = len(path);
	struct open_error *e = error_push_data(open_error, sizeof(*e) + path_len, NULL);
	if (e != NULL) {
		char *epath = (char *)(e + 1);
		memcpy(epath, path, path_len);
		e->path = epath;
		e->errnum = errnum;
	}
}

void
error_io(const char *path) {
	assert(path != NULL);
	size_t path_len = len(path);
	struct io_error *e = error_push_data(io_error, sizeof(*e) + path_len, NULL);
	if (e != NULL) {
		char *epath = (char *)(e + 1);
		memcpy(epath, path, path_len);
		e->path = epath;
	}
}

void
error_interrupt() {
	error_push(interrupt_error);
}

void
error_internal(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(internal_error, format, ap);
	va_end(ap);
}

void
error_initialization(const char *subsystem, const char *function) {
	struct initialization_error *e = error_push_data(initialization_error, sizeof(*e), NULL);
	if (e != NULL) {
		e->subsystem = subsystem;
		e->function = function;
	}
}

void
error_kernel_io(kaddr_t address) {
	struct kernel_io_error *e = error_push_data(kernel_io_error, sizeof(*e), NULL);
	if (e != NULL) {
		e->address = address;
	}
}

void
error_address_protection(kaddr_t address) {
	struct address_protection_error *e = error_push_data(address_protection_error, sizeof(*e),
			NULL);
	if (e != NULL) {
		e->address = address;
	}
}

void
error_api_unavailable(const char *function) {
	assert(function != NULL);
	size_t function_len = len(function);
	struct api_unavailable_error *e = error_push_data(api_unavailable_error,
			sizeof(*e) + function_len, NULL);
	if (e != NULL) {
		char *efunction = (char *)(e + 1);
		memcpy(efunction, function, function_len);
		e->function = efunction;
	}
}

void
error_functionality_unavailable(const char *message, ...) {
	va_list ap;
	va_start(ap, message);
	error_push_printf(functionality_unavailable_error, message, ap);
	va_end(ap);
}

void
error_address_unmapped(kaddr_t address) {
	struct address_unmapped_error *e = error_push_data(address_unmapped_error, sizeof(*e),
			NULL);
	if (e != NULL) {
		e->address = address;
	}
}

void
error_address_inaccessible(kaddr_t address) {
	struct address_inaccessible_error *e = error_push_data(address_inaccessible_error,
			sizeof(*e), NULL);
	if (e != NULL) {
		e->address = address;
	}
}

void
error_kext_not_found(const char *bundle_id) {
	assert(bundle_id != NULL);
	size_t bundle_id_len = len(bundle_id);
	struct kext_not_found_error *e = error_push_data(kext_not_found_error,
			sizeof(*e) + bundle_id_len, NULL);
	if (e != NULL) {
		char *ebundle_id = (char *)(e + 1);
		memcpy(ebundle_id, bundle_id, bundle_id_len);
		e->bundle_id = ebundle_id;
	}
}

void
error_kext_no_symbols(const char *bundle_id) {
	assert(bundle_id != NULL);
	size_t bundle_id_len = len(bundle_id);
	struct kext_no_symbols_error *e = error_push_data(kext_no_symbols_error,
			sizeof(*e) + bundle_id_len, NULL);
	if (e != NULL) {
		char *ebundle_id = (char *)(e + 1);
		memcpy(ebundle_id, bundle_id, bundle_id_len);
		e->bundle_id = ebundle_id;
	}
}

void
error_kext_symbol_not_found(const char *bundle_id, const char *symbol) {
	assert(symbol != NULL);
	size_t bundle_id_len = len(bundle_id);
	size_t symbol_len = len(symbol);
	struct kext_symbol_not_found_error *e = error_push_data(kext_symbol_not_found_error,
			sizeof(*e) + bundle_id_len + symbol_len, NULL);
	if (e != NULL) {
		char *ebundle_id = (char *)(e + 1);
		char *esymbol = ebundle_id + bundle_id_len;
		memcpy(ebundle_id, bundle_id, bundle_id_len);
		memcpy(esymbol, symbol, symbol_len);
		e->bundle_id = (bundle_id == NULL ? NULL : ebundle_id);
		e->symbol = esymbol;
	}
}

void
error_macho(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(macho_error, format, ap);
	va_end(ap);
}

void
error_kernelcache(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(kernelcache_error, format, ap);
	va_end(ap);
}

void
error_core(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(core_error, format, ap);
	va_end(ap);
}
