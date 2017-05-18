#ifndef MEMCTL_CLI__VMMAP_H_
#define MEMCTL_CLI__VMMAP_H_

#include "memctl/memctl_types.h"

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

#endif
