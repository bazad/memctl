#ifndef MEMCTL_CLI__MEMORY_H_
#define MEMCTL_CLI__MEMORY_H_

#include "memctl/kernel_memory.h"

/*
 * safe_memory
 *
 * Description:
 * 	A flag indicating whether safe memory operations should be preferred, possibly at the
 * 	expense of being able to access fewer memory regions. Defaults to false.
 */
extern bool safe_memory;

/*
 * read_memory
 *
 * Description:
 * 	Read kernel or physical memory.
 */
bool read_memory(kaddr_t address, size_t *size, void *data, bool physical, size_t access);

/*
 * write_memory
 *
 * Description:
 * 	Write kernel or physical memory.
 */
bool write_memory(kaddr_t address, size_t *size, const void *data, bool physical, size_t access);

#endif
