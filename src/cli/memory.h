#ifndef MEMCTL__CLI__MEMORY_H_
#define MEMCTL__CLI__MEMORY_H_

#include "memctl_types.h"

/*
 * read_memory
 *
 * Description:
 * 	Read kernel or physical memory.
 */
bool read_memory(kaddr_t address, size_t *size, bool physical, size_t access, void *data);

#endif
