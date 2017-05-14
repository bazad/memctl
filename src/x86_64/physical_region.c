#include "physical_region.h"

static const struct physical_region regions[] = {
	// TODO: These are just guesses. Verify that these regions are correct.
	{ "BIOS",        0xe00f8000,  0xe00f8fff, 4 },
	{ "IO APIC",     0xfec00000,  0xfecfffff, 4 },
	{ "MCH BAR",     0xfed10000,  0xfed17fff, 4 },
	{ "DMI BAR",     0xfed18000,  0xfed18fff, 4 },
	{ "RCBA",        0xfed1c000,  0xfed1ffff, 4 },
	{ "Local APIC",  0xfee00000,  0xfeefffff, 0 },
	{ "MCH BAR",    0xf90140000, 0xf90147fff, 4 },
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
