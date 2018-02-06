#include "error.h"

#include "format.h"

#include "memctl/kernel.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t
format(char *buffer, size_t size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(buffer, size, fmt, ap);
	va_end(ap);
	return (len > 0 ? len : 0);
}

static size_t
format_data_as_string(char *buffer, size_t size, error_handle error) {
	assert(error->size > 0);
	return format(buffer, size, "%s", (const char *) error->data);
}

static size_t
format_message_error(char *buffer, size_t size, error_handle error) {
	return format_data_as_string(buffer, size, error);
}

static size_t
format_usage_error(char *buffer, size_t size, error_handle error) {
	struct usage_error *e = error->data;
	assert(error->size >= sizeof(*e));
	if (e->command == NULL) {
		return format(buffer, size, "%s", e->message);
	} else if (e->option == NULL || e->option[0] == 0) {
		return format(buffer, size, "command %s: %s", e->command, e->message);
	} else {
		return format(buffer, size, "command %s: option %s: %s", e->command, e->option,
				e->message);
	}
}

static size_t
format_execve_error(char *buffer, size_t size, error_handle error) {
	struct execve_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "%s: %s", e->path, e->reason);
}

static size_t
format_kext_not_found_error(char *buffer, size_t size, error_handle error) {
	struct kext_not_found_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "no loaded kext matches bundle ID '%s'", e->bundle_id);
}

static size_t
format_kext_symbol_not_found_error(char *buffer, size_t size, error_handle error) {
	struct kext_symbol_not_found_error *e = error->data;
	assert(error->size >= sizeof(*e));
	if (e->bundle_id == NULL) {
		return format(buffer, size, "no kext defines symbol '%s'", e->symbol);
	} else if (strcmp(e->bundle_id, KERNEL_ID) == 0) {
		return format(buffer, size, "kernel symbol '%s' not found", e->symbol);
	} else {
		return format(buffer, size, "symbol '%s' not found in kext %s", e->symbol,
				e->bundle_id);
	}
}

struct error_type message_error = {
	.static_description = "error",
	.format_description = format_message_error,
};

struct error_type usage_error = {
	.static_description = "error",
	.format_description = format_usage_error,
};

struct error_type execve_error = {
	.static_description = "error",
	.format_description = format_execve_error,
};

struct error_type kext_not_found_error = {
	.static_description = "error",
	.format_description = format_kext_not_found_error,
};

struct error_type kext_symbol_not_found_error = {
	.static_description = "error",
	.format_description = format_kext_symbol_not_found_error,
};

static size_t
len(const char *str) {
	return (str == NULL ? 0 : strlen(str) + 1);
}

void
error_message(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(&message_error, format, ap);
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
	struct usage_error *e = error_push(&usage_error, sizeof(*e) + message_len);
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
	struct execve_error *e = error_push(&execve_error, sizeof(*e) + path_len + reason_len);
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
	struct kext_not_found_error *e = error_push(&kext_not_found_error,
			sizeof(*e) + bundle_id_len);
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
	struct kext_symbol_not_found_error *e = error_push(&kext_symbol_not_found_error,
			sizeof(*e) + bundle_id_len + symbol_len);
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
	char stack_buffer[512];
	char *buffer = stack_buffer;
	size_t size = error_description(error, NULL, 0);
	size += 1;
	if (size > sizeof(stack_buffer)) {
		buffer = malloc(size);
		assert(buffer != NULL);
	}
	error_description(error, buffer, size);
	fprintf(stderr, "error: %s\n", buffer);
	if (buffer != stack_buffer) {
		free(buffer);
	}
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
