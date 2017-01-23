#ifndef MEMCTL__MEMCTL_COMMON_H_
#define MEMCTL__MEMCTL_COMMON_H_

#include "memctl_types.h"

/*
 * mmap_file
 *
 * Description:
 * 	Memory map the given file read-only.
 *
 * Parameters:
 * 		file			The file to memory map.
 * 	out	data			The address of the file data.
 * 	out	size			The size of the file data.
 *
 * Returns:
 * 	true if the mapping was successful.
 */
bool mmap_file(const char *file, const void **data, size_t *size);

#endif
