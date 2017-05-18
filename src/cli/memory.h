#ifndef MEMCTL__CLI__MEMORY_H_
#define MEMCTL__CLI__MEMORY_H_

#include "memctl/kernel_memory.h"

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
