#ifndef MEMCTL__MAPPED_REGION_H_
#define MEMCTL__MAPPED_REGION_H_

#include "memctl/memctl_types.h"

/*
 * struct mapped_region
 *
 * Description:
 * 	Records information about a region of memory that has been mapped to a new location.
 */
struct mapped_region {
	const void *data;
	kaddr_t     addr;
	size_t      size;
};

/*
 * region_contains
 *
 * Description:
 * 	Returns true if the region contains the given address.
 *
 * Parameters:
 * 		region			The mapped memory region.
 * 		addr			The address to query.
 * 		size			The number of bytes that must be available.
 *
 * Returns:
 * 	True if the region contains size bytes at address.
 */
bool mapped_region_contains(const struct mapped_region *region, kaddr_t addr, size_t size);

/*
 * region_get
 *
 * Description:
 * 	Retrieves the contents of the region at the given address.
 *
 * Parameters:
 * 		region			The mapped memory region.
 * 		addr			The address to query.
 * 	out	size			The number of bytes available at that address. May be NULL.
 *
 * Returns:
 * 	A pointer to the at that address.
 */
const void *mapped_region_get(const struct mapped_region *region, kaddr_t addr, size_t *size);

/*
 * region_address
 *
 * Description:
 * 	Get the address of the given data pointer in the memory region.
 *
 * Parameters:
 * 		region			The mapped memory region.
 * 		data			A pointer into the data of the region.
 *
 * Returns:
 * 	The address corresponding to the data.
 */
kaddr_t mapped_region_address(const struct mapped_region *region, const void *data);

#endif
