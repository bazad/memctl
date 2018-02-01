#ifndef MEMCTL__ARM64__FINDER__PMAP_CACHE_ATTRIBUTES_H_
#define MEMCTL__ARM64__FINDER__PMAP_CACHE_ATTRIBUTES_H_

#include "memctl/kernel.h"

/*
 * kernel_find_pmap_cache_attributes
 *
 * Description:
 * 	A special symbol finder for _pmap_cache_attributes.
 */
void kernel_find_pmap_cache_attributes(struct kext *kernel);

#endif
