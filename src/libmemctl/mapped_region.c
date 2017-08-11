#include "memctl/mapped_region.h"

#include <assert.h>

bool
mapped_region_contains(const struct mapped_region *region, kaddr_t addr, size_t size) {
	return (region->addr <= addr && addr + size <= region->addr + region->size);
}

const void *
mapped_region_get(const struct mapped_region *region, kaddr_t addr, size_t *size) {
	assert(mapped_region_contains(region, addr, 1));
	kaddr_t offset = addr - region->addr;
	const void *data = (const void *)((uintptr_t)region->data + offset);
	if (size != NULL) {
		*size = region->size - offset;
	}
	return data;
}

kaddr_t
mapped_region_address(const struct mapped_region *region, const void *data) {
	assert(region->data <= data);
	uintptr_t offset = (uintptr_t)data - (uintptr_t)region->data;
	assert(offset <= region->size);
	return region->addr + offset;
}
