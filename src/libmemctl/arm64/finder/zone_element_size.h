#ifndef MEMCTL__ARM64__FINDER__ZONE_ELEMENT_SIZE_H_
#define MEMCTL__ARM64__FINDER__ZONE_ELEMENT_SIZE_H_

#include "memctl/kernel.h"

/*
 * kernel_find_zone_element_size
 *
 * Description:
 * 	A special symbol finder for _kfree_addr and _zone_element_size.
 */
void kernel_find_zone_element_size(struct kext *kernel);

#endif
