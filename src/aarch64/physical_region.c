#include "physical_region.h"

static const struct physical_region regions[] = {
	// TODO: This is a hack. Investigate the correct regions and access widths and list them
	// here.
	// TODO: Sometimes these memory regions are readable, but only the first word (or first few
	// words). It would be nice to support reading those registers, rather than a blanket deny.
	{ "Unknown", 0x200000000, 0x2ffffffff, 0 },
	// TODO: Needed on iPhone 6s.
	{ "Unknown", 0x7c0000000, 0x7c0ffffff, 0 },
	// TODO: Only on iPhone 7; really we want to block reads to virtual address
	// 0xfffffff0001fc000, since physical address 0x87c494000 is readable at
	// 0xfffffff0001f8000. No idea how this works, but it motivates a significantly more
	// powerful memory regions implementation that handles virtual as well as physical regions,
	// and supports regions on specific devices.
	{ "Unknown", 0x87c494000, 0x87c497fff, 0 },
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
