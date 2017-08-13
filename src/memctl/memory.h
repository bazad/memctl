#ifndef MEMCTL_CLI__MEMORY_H_
#define MEMCTL_CLI__MEMORY_H_

#include "memctl/kernel_memory.h"

/*
 * memflags
 *
 * Description:
 * 	Flags for read_kernel and write_kernel.
 */
typedef uint32_t memflags;

enum {
	// Force direct (unsafe) access to memory. Perform no safety checks.
	MEM_FORCE = 0x1,
	// Access physical memory rather than virtual memory.
	MEM_PHYS  = 0x2,
};

/*
 * safe_memory
 *
 * Description:
 * 	A flag indicating whether safe memory operations should be preferred, possibly at the
 * 	expense of being able to access fewer memory regions. Defaults to false.
 */
extern bool safe_memory;

/*
 * read_kernel
 *
 * Description:
 * 	Read kernel or physical memory.
 *
 * Parameters:
 * 		address			The virtual or physical address to read.
 * 	inout	size			On entry, the number of bytes to read. On return, the
 * 					number of bytes successfully read.
 * 	out	data			On return, the data that was read.
 * 		flags			Memory access flags.
 * 		access			The access width.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
bool read_kernel(kaddr_t address, size_t *size, void *data, memflags flags, size_t access);

/*
 * write_kernel
 *
 * Description:
 * 	Write kernel or physical memory.
 *
 * Parameters:
 * 		address			The virtual or physical address to read.
 * 	inout	size			On entry, the number of bytes to write. On return, the
 * 					number of bytes successfully written.
 * 	out	data			The data to write.
 * 		flags			Memory access flags.
 * 		access			The access width.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
bool write_kernel(kaddr_t address, size_t *size, const void *data, memflags flags, size_t access);

#endif
