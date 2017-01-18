#include "cli/error.h"

#include "cli/format.h"

#include "kernel.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t
len(const char *str) {
	return (str == NULL ? 0 : strlen(str) + 1);
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

/* Error printing */

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
		case kernel_io_error: {
			struct kernel_io_error *e = error->data;
			PRINT("kernel I/O error at address " KADDR_FMT, e->address);
			} break;
		case address_protection_error: {
			struct address_protection_error *e = error->data;
			PRINT("kernel memory protection error at address " KADDR_FMT, e->address);
			} break;
		case address_unmapped_error: {
			struct address_unmapped_error *e = error->data;
			PRINT("kernel address " KADDR_FMT " is unmapped", e->address);
			} break;
		case address_inaccessible_error: {
			struct address_protection_error *e = error->data;
			PRINT("kernel address " KADDR_FMT " is inaccessible", e->address);
			} break;
		case kext_not_found_error: {
			struct kext_not_found_error *e = error->data;
			PRINT("no loaded kext matches bundle ID '%s'", e->bundle_id);
			} break;
		case kext_no_symbols_error: {
			struct kext_no_symbols_error *e = error->data;
			PRINT("kext %s does not contain symbol information", e->bundle_id);
			} break;
		case core_error: {
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
