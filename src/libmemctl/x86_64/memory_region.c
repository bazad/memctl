#include "memctl/memory_region.h"

static const struct memory_region physical_regions[] = {
	// TODO: These are just guesses. Verify that these regions are correct.
	{ "BIOS",        0xe00f8000,  0xe00f8fff, 4 },
	{ "IO APIC",     0xfec00000,  0xfecfffff, 4 },
	{ "MCH BAR",     0xfed10000,  0xfed17fff, 4 },
	{ "DMI BAR",     0xfed18000,  0xfed18fff, 4 },
	{ "RCBA",        0xfed1c000,  0xfed1ffff, 4 },
	{ "Local APIC",  0xfee00000,  0xfeefffff, 0 },
	{ "MCH BAR",    0xf90140000, 0xf90147fff, 4 },
};

static const struct memory_region virtual_regions[] = {
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
