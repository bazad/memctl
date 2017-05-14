#ifndef MEMCTL__MEMORY_REGION_H_
#define MEMCTL__MEMORY_REGION_H_

#include "memctl_types.h"

/*
 * struct memory_region
 *
 * Description:
 * 	A range of physical or virtual memory with special access requirements, for example, memory
 * 	mapped registers.
 */
struct memory_region {
	// The name of the region, if it is known.
	char *name;
	// The first physical or virtual address of the region.
	kaddr_t start;
	// The last physical or virtual address of the region.
	kaddr_t end;
	// The permissible access width, or 0 if this region cannot be accessed.
	size_t access;
};

/*
 * virtual_region_find
 *
 * Description:
 * 	Find the first special virtual region that intersects the given range of virtual memory.
 *
 * Parameters:
 * 		virtaddr		The start address of the range.
 * 		size			The size of the range in bytes.
 *
 * Returns:
 * 	The special virtual region with the smallest start address that intersects the specified
 * 	range, if one exists.
 */
const struct memory_region *virtual_region_find(kaddr_t virtaddr, size_t size);

/*
 * physical_region_find
 *
 * Description:
 * 	Find the first special physical region that intersects the given range of physical memory.
 *
 * Parameters:
 * 		physaddr		The start address of the range.
 * 		size			The size of the range in bytes.
 *
 * Returns:
 * 	The special physical region with the smallest start address that intersects the specified
 * 	range, if one exists.
 */
const struct memory_region *physical_region_find(paddr_t physaddr, size_t size);

#endif
