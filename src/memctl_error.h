#ifndef MEMCTL__MEMCTL_ERROR_H_
#define MEMCTL__MEMCTL_ERROR_H_

#include "error.h"
#include "memctl_types.h"

enum {
	open_error                       = 8,
	io_error                         = 9,
	interrupt_error                  = 10,

	internal_error                   = 32,
	initialization_error             = 33,
	api_unavailable_error            = 34,
	kernel_io_error                  = 35,
	address_protection_error         = 36,
	address_unmapped_error           = 37,
	address_inaccessible_error       = 38,
	kext_not_found_error             = 39,
	kext_no_symbols_error            = 40,
	macho_error                      = 41,

	core_error                       = 64,
};

/*
 * out_of_memory_error
 */
static inline void
error_out_of_memory() {
	error_push_out_of_memory();
}

/*
 * open_error
 */
struct open_error {
	const char *path;
	int errnum;
};

void error_open(const char *path, int errnum);

/*
 * io_error
 */
struct io_error {
	const char *path;
};

void error_io(const char *path);

/*
 * interrupt_error
 */
void error_interrupt(void);

/*
 * internal_error
 */
void error_internal(const char *format, ...);

/*
 * initialization_error
 */
struct initialization_error {
	const char *subsystem;
	const char *function;
};

void error_initialization(const char *subsystem, const char *function);

/*
 * api_unavailable_error
 */
struct api_unavailable_error {
	const char *function;
};

void error_api_unavailable(const char *function);

/*
 * kernel_io_error
 */
struct kernel_io_error {
	kaddr_t address;
};

void error_kernel_io(kaddr_t address);

/*
 * address_protection_error
 */
struct address_protection_error {
	kaddr_t address;
};

void error_address_protection(kaddr_t address);

/*
 * address_unmapped_error
 */
struct address_unmapped_error {
	kaddr_t address;
};

void error_address_unmapped(kaddr_t address);

/*
 * address_inaccessible_error
 */
struct address_inaccessible_error {
	kaddr_t address;
};

void error_address_inaccessible(kaddr_t address);

/*
 * kext_not_found_error
 */
struct kext_not_found_error {
	const char *bundle_id;
};

void error_kext_not_found(const char *bundle_id);

/*
 * kext_no_symbols_error
 */
struct kext_no_symbols_error {
	const char *bundle_id;
};

void error_kext_no_symbols(const char *bundle_id);

/*
 * macho_error
 */
void error_macho(const char *format, ...);

/*
 * core_error
 */
void error_core(const char *format, ...);

#endif
