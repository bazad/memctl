#include "cli/read.h"

#include "memctl_signal.h"

#include "kernel_memory.h"
#include "utility.h"

#include <assert.h>
#include <stdio.h>

bool
memctl_read(kaddr_t kaddr, size_t size, size_t width, size_t access) {
	assert(ispow2(width) && 0 < width && width <= 8);
	assert(ispow2(access) && access <= 8);
	uint8_t data[page_size];
	unsigned n = min(16 / width,  8);
	while (size > 0) {
		size_t readsize = min(size, sizeof(data));
		kernel_io_result result = kernel_read_unsafe(kaddr, &readsize, data, access, NULL);
		size_t end = readsize / width;
		for (size_t i = 0; i < end; i++) {
			if (interrupted) {
				error_interrupt();
				return false;
			}
			kword_t value = unpack_uint(data + width * i, width);
			if (i % n == 0) {
				printf("%016llx:  ", kaddr);
			}
			int newline = (((i + 1) % n == 0) || (size == readsize && i == end - 1));
			printf("%0*llx%c", (int)(2 * width), value, (newline ? '\n' : ' '));
			kaddr += width;
		}
		if (result != KERNEL_IO_SUCCESS) {
			return false;
		}
		size -= readsize;
	}
	return true;
}
