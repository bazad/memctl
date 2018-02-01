#include "memctl/memory_region.h"

static const struct memory_region physical_regions[] = {
	// TODO: This is a hack. Investigate the correct regions and access widths and list them
	// here.
	// TODO: Sometimes these memory regions are readable, but only the first word (or first few
	// words). It would be nice to support reading those registers, rather than a blanket deny.
	{ "Unknown", 0x200000000, 0x2ffffffff, 0 },
	// TODO: Needed on iPhone 6s.
	{ "Unknown", 0x7c0000000, 0x7c0ffffff, 0 },
};

static const struct memory_region virtual_regions[] = {
	// TODO: Only on iPhone 7.
	// Virtual addresses 0xfffffff0001f8000 and 0xfffffff0001fc000 (and another address, e.g.
	// 0xfffffff08c494000) map to the same physical address (e.g. 0x87c494000), but somehow the
	// first one is readable and the second one triggers a panic (and the third is again
	// readable).
	{ "Unknown", 0xfffffff0001fc000, 0xfffffff0001fffff, 0 },
};

static const size_t nphysical_regions = sizeof(physical_regions) / sizeof(physical_regions[0]);
static const size_t nvirtual_regions = sizeof(virtual_regions) / sizeof(virtual_regions[0]);

const struct memory_region *
virtual_region_find(kaddr_t virtaddr, size_t size) {
	kaddr_t virtlast = virtaddr + size - 1;
	for (size_t i = 0; i < nvirtual_regions; i++) {
		const struct memory_region *vr = &virtual_regions[i];
		if (vr->start > virtlast) {
			break;
		}
		if (vr->end < virtaddr) {
			continue;
		}
		return vr;
	}
	return NULL;
}

const struct memory_region *
physical_region_find(paddr_t physaddr, size_t size) {
	paddr_t physlast = physaddr + size - 1;
	for (size_t i = 0; i < nphysical_regions; i++) {
		const struct memory_region *pr = &physical_regions[i];
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
