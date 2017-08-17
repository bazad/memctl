#ifndef MEMCTL_CLI__VMMAP_H_
#define MEMCTL_CLI__VMMAP_H_

#include "memctl/memctl_types.h"

/*
 * memctl_vmregion
 *
 * Description:
 * 	Return the start address and size of the virtual memory region containing the specified
 * 	address. If address is beyond the end of the last memory region, then true is returned
 * 	without setting start or size.
 *
 * Parameters:
 * 	out	start			On return, the start address. May be NULL.
 * 	out	size			On return, the region size. May be NULL.
 * 		address			The virtual memory address.
 *
 * Returns:
 * 	True if the virtual memory region was found, or if the address is beyond the end of the
 * 	last virtual memory region.
 */
bool memctl_vmregion(kaddr_t *start, size_t *size, kaddr_t address);

/*
 * memctl_vmmap
 *
 * Description:
 * 	Print virtual memory region information, similar to the vmmap utility.
 *
 * Parameters:
 * 		kaddr			The start address.
 * 		end			The end address.
 * 		depth			The region depth.
 *
 * Returns:
 * 	True if memory region information was successfully obtained.
 */
bool memctl_vmmap(kaddr_t kaddr, kaddr_t end, uint32_t depth);

/*
 * memctl_vmprotect
 *
 * Description:
 * 	Set the virtual memory protection flags on a region of memory.
 *
 * Parameters:
 * 		address			The start address.
 * 		size			The size of the region to protect.
 * 		prot			The protection flags to apply.
 *
 * Returns:
 * 	True if no errors were encountered.
 */
bool memctl_vmprotect(kaddr_t address, size_t size, int prot);

#endif
