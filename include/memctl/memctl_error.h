#ifndef MEMCTL__MEMCTL_ERROR_H_
#define MEMCTL__MEMCTL_ERROR_H_

#include "memctl/error.h"
#include "memctl/memctl_types.h"


extern struct error_type out_of_memory_error;
extern struct error_type open_error;
extern struct error_type io_error;
extern struct error_type interrupt_error;

extern struct error_type internal_error;
extern struct error_type initialization_error;
extern struct error_type api_unavailable_error;
extern struct error_type functionality_unavailable_error;
extern struct error_type kernel_io_error;
extern struct error_type address_protection_error;
extern struct error_type address_unmapped_error;
extern struct error_type address_inaccessible_error;
extern struct error_type macho_parse_error;
extern struct error_type kernelcache_error;

/*
 * memctl_warning
 */
void memctl_warning(const char *format, ...);


/*
 * out_of_memory_error
 */
void error_out_of_memory();

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
 * functionality_unavailable_error
 *
 * TODO: error_functionality_unavailable should take a function string argument as well.
 */
void error_functionality_unavailable(const char *message, ...);

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
 * kernelcache_error
 */
void error_kernelcache(const char *format, ...);

#endif
