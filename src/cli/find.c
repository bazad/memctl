#include "cli/find.h"

#include "cli/format.h"

#include "kernel_memory.h"
#include "memctl_signal.h"
#include "utility.h"

#include <stdio.h>

bool
memctl_find(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical, bool heap,
		size_t access, size_t alignment) {
	// Select the read function.
	kernel_read_fn read = kernel_read_all;
	if (heap) {
		read = kernel_read_heap;
	} else if (physical) {
		error_internal("physical memory read not implemented"); // TODO
		return false;
	}
	// Initialize the loop.
	start = round2_up(start, alignment);
	if (start + width - 1 >= end) {
		return true;
	}
	uint8_t buf[sizeof(kword_t) + page_size];
	uint8_t *const data = buf + sizeof(kword_t);
	kaddr_t address = start;
	bool spill = false;
	// Iterate over all addresses.
	for (;;) {
		size_t rsize = min(end - address, page_size);
		kaddr_t next;
		error_stop();
		read(address, &rsize, data, access, &next);
		error_start();
		if (interrupted) {
			error_interrupt();
			return false;
		}
		if (rsize > 0) {
			uint8_t *p = (spill ? buf : data);
			uint8_t *const e = data + rsize;
			for (; p + width <= e; p += alignment) {
				if (unpack_uint(p, width) == value) {
					printf(KADDR_XFMT"\n", address + (p - data));
				}
			}
			if (rsize == page_size) {
				*(kword_t *)buf = *(kword_t *)(data + page_size - sizeof(kword_t));
			}
		}
		spill = (rsize == page_size);
		if (next == 0 || next >= end) {
			break;
		}
		address = next;
	}
	return true;
}
