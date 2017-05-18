#ifndef MEMCTL__CLI__FIND_H_
#define MEMCTL__CLI__FIND_H_

#include "memctl/memctl_types.h"

/*
 * memctl_find
 *
 * Description:
 * 	Find occurrences of the given value in kernel memory.
 *
 * Parameters:
 * 		start			The address to start searching.
 * 		end			The address to end searching, exclusive.
 * 		value			The value to find.
 * 		width			The width of the value in bytes.
 * 		physical		Whether to search physical memory.
 * 		heap			Whether to search the heap only.
 * 		access			The access width to use when reading kernel memory.
 * 		alignment		The alignment of the value in kernel memory.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
bool memctl_find(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical, bool heap,
		size_t access, size_t alignment);

#endif
