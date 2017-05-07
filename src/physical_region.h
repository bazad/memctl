#ifndef MEMCTL__PHYSICAL_REGION_H_
#define MEMCTL__PHYSICAL_REGION_H_

#include "memctl_types.h"

/*
 * struct physical_region
 *
 * Description:
 * 	A range of physical memory with special access requirements, for example, memory mapped
 * 	registers.
 */
struct physical_region {
	// The name of the region, if it is known.
	char *name;
	// The first physical address of the region.
	paddr_t start;
	// The last physical address of the region.
	paddr_t end;
	// The permissible access width, or 0 if this region cannot be accessed.
	size_t access;
};

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
const struct physical_region *physical_region_find(paddr_t physaddr, size_t size);

#endif
