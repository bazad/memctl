#include "physical_region.h"

static const struct physical_region regions[] = {
	// TODO: This is a hack. Investigate the correct regions and access widths and list them
	// here.
	{ "Unknown", 0x200000000, 0x2ffffffff, 0 },
};

static const size_t nregions = sizeof(regions) / sizeof(regions[0]);

const struct physical_region *
physical_region_find(paddr_t physaddr, size_t size) {
	paddr_t physlast = physaddr + size - 1;
	for (size_t i = 0; i < nregions; i++) {
		const struct physical_region *pr = &regions[i];
		if (pr->start > physlast) {
			break;
		}
		if (pr->end < physaddr) {
			continue;
		}
		return pr;
	}
	return NULL;
}
