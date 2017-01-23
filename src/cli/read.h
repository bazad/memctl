#ifndef MEMCTL__CLI__READ_H_
#define MEMCTL__CLI__READ_H_

#include "kernel_memory.h"
#include "memctl_types.h"
#include "cli/error.h"

/*
 * memctl_read
 *
 * Description:
 * 	Display the output of kernel_read.
 *
 * Parameters:
 * 		kaddr			The kernel address to read
 * 		size			The number of bytes to read
 * 		width			The formatting width
 * 		access			The access width while reading
 *
 * Returns:
 * 	true if the read was successful.
 *
 * Errors:
 * 	interrupt_error			An interrupt was encountered.
 * 	...				Other errors returned from kernel_read
 *
 * Dependencies:	MEMCTL_MEMORY
 */
bool memctl_read(kaddr_t kaddr, size_t size, size_t width, size_t access);

#endif
