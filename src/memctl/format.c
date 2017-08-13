#include "format.h"

#include <assert.h>
#include <mach/mach.h>
#include <stdio.h>

void
format_display_size(char buf[5], uint64_t size) {
	const char scale[] = { 'B', 'K', 'M', 'G', 'T', 'P', 'E' };
	double display_size = size;
	unsigned scale_index = 0;
	while (display_size >= 999.5) {
		display_size /= 1024;
		scale_index++;
	}
	assert(scale_index < sizeof(scale) / sizeof(scale[0]));
	int precision = 0;
	if (display_size < 9.95 && display_size - (float)((int)display_size) > 0) {
		precision = 1;
	}
	int len = snprintf(buf, 5, "%.*f%c", precision, display_size, scale[scale_index]);
	assert(len > 0 && len < 5);
}

void
format_memory_protection(char buf[4], int prot) {
	int len = snprintf(buf, 4, "%c%c%c",
			(prot & VM_PROT_READ    ? 'r' : '-'),
			(prot & VM_PROT_WRITE   ? 'w' : '-'),
			(prot & VM_PROT_EXECUTE ? 'x' : '-'));
	assert(len == 3);
}
