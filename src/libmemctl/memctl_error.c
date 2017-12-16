#include "memctl/memctl_error.h"

#include "memctl/macho.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if KERNEL_BITS == 32
# define ADDR	"0x%08x"
#else
# define ADDR	"0x%016llx"
#endif

static size_t
format(char *buffer, size_t size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(buffer, size, fmt, ap);
	va_end(ap);
	return (len > 0 ? len : 0);
}

static size_t
format_static_description(char *buffer, size_t size, error_handle error) {
	return format(buffer, size, "%s", error->type->static_description);
}

static size_t
format_data_as_string(char *buffer, size_t size, error_handle error) {
	assert(error->size > 0);
	return format(buffer, size, "%s", (const char *) error->data);
}

static size_t
format_out_of_memory_error(char *buffer, size_t size, error_handle error) {
	return format_static_description(buffer, size, error);
}

static size_t
format_open_error(char *buffer, size_t size, error_handle error) {
	struct open_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "could not open '%s': %s", e->path, strerror(e->errnum));
}

static size_t
format_io_error(char *buffer, size_t size, error_handle error) {
	struct io_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "I/O error while processing path '%s'", e->path);
}

static size_t
format_interrupt_error(char *buffer, size_t size, error_handle error) {
	return format_static_description(buffer, size, error);
}

static size_t
format_internal_error(char *buffer, size_t size, error_handle error) {
	return format_data_as_string(buffer, size, error);
}

static size_t
format_initialization_error(char *buffer, size_t size, error_handle error) {
	struct initialization_error *e = error->data;
	assert(error->size >= sizeof(*e));
	assert(e->subsystem != NULL);
	if (e->function == NULL) {
		return format(buffer, size, "could not initialize the '%s' subsystem",
				e->subsystem);
	}
	return format(buffer, size, "could not initialize function '%s' of the '%s' subsystem",
			e->function, e->subsystem);
}

static size_t
format_api_unavailable_error(char *buffer, size_t size, error_handle error) {
	struct api_unavailable_error *e = error->data;
	assert(error->size >= sizeof(*e));
	assert(e->function != NULL);
	return format(buffer, size, "%s not available", e->function);
}

static size_t
format_functionality_unavailable_error(char *buffer, size_t size, error_handle error) {
	return format_data_as_string(buffer, size, error);
}

static size_t
format_kernel_io_error(char *buffer, size_t size, error_handle error) {
	struct kernel_io_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "kernel I/O error at address "ADDR, e->address);
}

static size_t
format_address_protection_error(char *buffer, size_t size, error_handle error) {
	struct address_protection_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "kernel memory protection error at address "ADDR, e->address);
}

static size_t
format_address_unmapped_error(char *buffer, size_t size, error_handle error) {
	struct address_unmapped_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "kernel address "ADDR" is unmapped", e->address);
}

static size_t
format_address_inaccessible_error(char *buffer, size_t size, error_handle error) {
	struct address_protection_error *e = error->data;
	assert(error->size >= sizeof(*e));
	return format(buffer, size, "kernel address "ADDR" is inaccessible", e->address);
}

static size_t
format_macho_parse_error(char *buffer, size_t size, error_handle error) {
	return format_data_as_string(buffer, size, error);
}

static size_t
format_kernelcache_error(char *buffer, size_t size, error_handle error) {
	return format_data_as_string(buffer, size, error);
}

struct error_type out_of_memory_error = {
	.static_description = "out of memory",
	.format_description = format_out_of_memory_error,
};

struct error_type open_error = {
	.static_description = "could not open file",
	.format_description = format_open_error,
};

struct error_type io_error = {
	.static_description = "I/O error",
	.format_description = format_io_error,
};

struct error_type interrupt_error = {
	.static_description = "interrupted",
	.format_description = format_interrupt_error,
};

struct error_type internal_error = {
	.static_description = "internal error",
	.format_description = format_internal_error,
};

struct error_type initialization_error = {
	.static_description = "initialization error",
	.format_description = format_initialization_error,
};

struct error_type api_unavailable_error = {
	.static_description = "API unavailable",
	.format_description = format_api_unavailable_error,
};

struct error_type functionality_unavailable_error = {
	.static_description = "functionality unavailable",
	.format_description = format_functionality_unavailable_error,
};

struct error_type kernel_io_error = {
	.static_description = "kernel I/O error",
	.format_description = format_kernel_io_error,
};

struct error_type address_protection_error = {
	.static_description = "kernel address protection error",
	.format_description = format_address_protection_error,
};

struct error_type address_unmapped_error = {
	.static_description = "kernel address unmapped",
	.format_description = format_address_unmapped_error,
};

struct error_type address_inaccessible_error = {
	.static_description = "kernel address inaccessible",
	.format_description = format_address_inaccessible_error,
};

struct error_type macho_parse_error = {
	.static_description = "Mach-O parse failure",
	.format_description = format_macho_parse_error,
};

struct error_type kernelcache_error = {
	.static_description = "kernelcache processing error",
	.format_description = format_kernelcache_error,
};

static size_t
len(const char *str) {
	return (str == NULL ? 0 : strlen(str) + 1);
}

void
error_out_of_memory() {
	error_push(&out_of_memory_error, 0);
}

void
error_open(const char *path, int errnum) {
	assert(path != NULL);
	size_t path_len = len(path);
	struct open_error *e = error_push(&open_error, sizeof(*e) + path_len);
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
	struct io_error *e = error_push(&io_error, sizeof(*e) + path_len);
	if (e != NULL) {
		char *epath = (char *)(e + 1);
		memcpy(epath, path, path_len);
		e->path = epath;
	}
}

void
error_interrupt() {
	error_push(&interrupt_error, 0);
}

void
error_internal(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(&internal_error, format, ap);
	va_end(ap);
}

void
error_initialization(const char *subsystem, const char *function) {
	struct initialization_error *e = error_push(&initialization_error, sizeof(*e));
	if (e != NULL) {
		e->subsystem = subsystem;
		e->function = function;
	}
}

void
error_kernel_io(kaddr_t address) {
	struct kernel_io_error *e = error_push(&kernel_io_error, sizeof(*e));
	if (e != NULL) {
		e->address = address;
	}
}

void
error_address_protection(kaddr_t address) {
	struct address_protection_error *e = error_push(&address_protection_error, sizeof(*e));
	if (e != NULL) {
		e->address = address;
	}
}

void
error_api_unavailable(const char *function) {
	assert(function != NULL);
	size_t function_len = len(function);
	struct api_unavailable_error *e = error_push(&api_unavailable_error,
			sizeof(*e) + function_len);
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
	error_push_printf(&functionality_unavailable_error, message, ap);
	va_end(ap);
}

void
error_address_unmapped(kaddr_t address) {
	struct address_unmapped_error *e = error_push(&address_unmapped_error, sizeof(*e));
	if (e != NULL) {
		e->address = address;
	}
}

void
error_address_inaccessible(kaddr_t address) {
	struct address_inaccessible_error *e = error_push(&address_inaccessible_error,
			sizeof(*e));
	if (e != NULL) {
		e->address = address;
	}
}

void
macho_error(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(&macho_parse_error, format, ap);
	va_end(ap);
}

void
error_kernelcache(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	error_push_printf(&kernelcache_error, format, ap);
	va_end(ap);
}
