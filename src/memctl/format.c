#include "format.h"

#include <mach/mach.h>
#include <stdio.h>

void
format_display_size(char buf[5], uint64_t size) {
	const char scale[] = { 'B', 'K', 'M', 'G', 'T', 'P', 'E' };
	double display_size = size;
	unsigned scale_index = 0;
	while (display_size >= 1000.0) {
		display_size /= 1024;
		scale_index++;
	}
	int precision = 0;
	if (display_size < 9.95 && display_size - (float)((int)display_size) > 0) {
		precision = 1;
	}
	snprintf(buf, 5, "%.*f%c", precision, display_size, scale[scale_index]);
}

void
format_memory_protection(char buf[4], int prot) {
	snprintf(buf, 4, "%c%c%c",
			(prot & VM_PROT_READ    ? 'r' : '-'),
			(prot & VM_PROT_WRITE   ? 'w' : '-'),
			(prot & VM_PROT_EXECUTE ? 'x' : '-'));
}
