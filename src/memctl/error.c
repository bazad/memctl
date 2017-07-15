#include "error.h"

#include "format.h"

#include "memctl/kernel.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t
len(const char *str) {
	return (str == NULL ? 0 : strlen(str) + 1);
}

void
error_message(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(message_error, format, ap);
	va_end(ap);
}

void
error_usage(const char *command, const char *option, const char *format, ...) {
	assert(format != NULL);
	va_list ap;
	va_start(ap, format);
	char *message;
	vasprintf(&message, format, ap);
	va_end(ap);
	size_t message_len = len(message);
	struct usage_error *e = error_push_data(usage_error, sizeof(*e) + message_len, NULL);
	if (e != NULL) {
		char *emessage = (char *)(e + 1);
		memcpy(emessage, message, message_len);
		e->message = emessage;
		e->command = command;
		e->option = option;
	}
	free(message);
}

void
error_execve(const char *path, const char *reason) {
	assert(path != NULL);
	assert(reason != NULL);
	size_t path_len = len(path);
	size_t reason_len = len(reason);
	struct execve_error *e = error_push_data(execve_error,
			sizeof(*e) + path_len + reason_len, NULL);
	if (e != NULL) {
		char *epath = (char *)(e + 1);
		char *ereason = epath + path_len;
		memcpy(epath, path, path_len);
		memcpy(ereason, reason, reason_len);
		e->path = epath;
		e->reason = ereason;
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

/* Error printing */

void
memctl_warning(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	char *message;
	vasprintf(&message, format, ap);
	if (message != NULL) {
		fprintf(stderr, "warning: %s\n", message);
		free(message);
	}
	va_end(ap);
}

/*
 * print_error
 *
 * Description:
 * 	Print an error message for the specified error.
 */
static void
print_error(error_handle error) {
#define PRINT(fmt, ...) fprintf(stderr, "error: " fmt "\n", ##__VA_ARGS__)
	switch (error->type) {
		case out_of_memory_error: {
			PRINT("out of memory");
			} break;
		case open_error: {
			struct open_error *e = error->data;
			PRINT("could not open '%s': %s", e->path, strerror(e->errnum));
			} break;
		case io_error: {
			struct io_error *e = error->data;
			PRINT("I/O error while processing path '%s'", e->path);
			} break;
		case interrupt_error: {
			PRINT("interrupted");
			} break;
		case internal_error: {
			PRINT("%s", (char *)error->data);
			} break;
		case initialization_error: {
			struct initialization_error *e = error->data;
			assert(e->subsystem != NULL);
			if (e->function == NULL) {
				PRINT("could not initialize the '%s' subsystem", e->subsystem);
			} else {
				PRINT("could not initialize function '%s' of the '%s' subsystem",
						e->function, e->subsystem);
			}
			} break;
		case api_unavailable_error: {
			struct api_unavailable_error *e = error->data;
			PRINT("%s not available", e->function);
			} break;
		case functionality_unavailable_error: {
			PRINT("%s", (char *)error->data);
			} break;
		case kernel_io_error: {
			struct kernel_io_error *e = error->data;
			PRINT("kernel I/O error at address " KADDR_XFMT, e->address);
			} break;
		case address_protection_error: {
			struct address_protection_error *e = error->data;
			PRINT("kernel memory protection error at address " KADDR_XFMT, e->address);
			} break;
		case address_unmapped_error: {
			struct address_unmapped_error *e = error->data;
			PRINT("kernel address " KADDR_XFMT " is unmapped", e->address);
			} break;
		case address_inaccessible_error: {
			struct address_protection_error *e = error->data;
			PRINT("kernel address " KADDR_XFMT " is inaccessible", e->address);
			} break;
		case kext_not_found_error: {
			struct kext_not_found_error *e = error->data;
			PRINT("no loaded kext matches bundle ID '%s'", e->bundle_id);
			} break;
		case kext_symbol_not_found_error: {
			struct kext_symbol_not_found_error *e = error->data;
			if (e->bundle_id == NULL) {
				PRINT("no kext defines symbol '%s'", e->symbol);
			} else if (strcmp(e->bundle_id, KERNEL_ID) == 0) {
				PRINT("kernel symbol '%s' not found", e->symbol);
			} else {
				PRINT("symbol '%s' not found in kext %s", e->symbol, e->bundle_id);
			}
			} break;
		case macho_parse_error: {
			PRINT("%s", (char *)error->data);
			} break;
		case kernelcache_error: {
			PRINT("%s", (char *)error->data);
			} break;
		case core_error: {
			PRINT("%s", (char *)error->data);
			} break;
		case message_error: {
			PRINT("%s", (char *)error->data);
			} break;
		case usage_error: {
			struct usage_error *e = error->data;
			if (e->command == NULL) {
				PRINT("%s", e->message);
			} else if (e->option == NULL || e->option[0] == 0) {
				PRINT("command %s: %s", e->command, e->message);
			} else {
				PRINT("command %s: option %s: %s", e->command, e->option, e->message);
			}
			} break;
		case execve_error: {
			struct execve_error *e = error->data;
			PRINT("%s: %s", e->path, e->reason);
			} break;
		default: {
			PRINT("unknown error code %lld", error->type);
			} break;
	}
#undef PRINT
}

void
print_errors() {
	size_t end = error_count();
	for (size_t i = 0; i < end; i++) {
		error_handle error = error_at_index(i);
		print_error(error);
	}
	error_clear();
}
