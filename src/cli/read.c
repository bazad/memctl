#include "cli/read.h"

#include "cli/memory.h"

#include "memctl_signal.h"
#include "utility.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

bool
memctl_read(kaddr_t address, size_t size, bool physical, size_t width, size_t access) {
	assert(ispow2(width) && 0 < width && width <= 8);
	assert(ispow2(access) && access <= 8);
	uint8_t data[page_size];
	unsigned n = min(16 / width,  8);
	while (size > 0) {
		size_t readsize = min(size, sizeof(data));
		bool read_success = read_memory(address, &readsize, data, physical, access);
		size_t end = readsize / width;
		for (size_t i = 0; i < end; i++) {
			if (interrupted) {
				error_interrupt();
				return false;
			}
			kword_t value = unpack_uint(data + width * i, width);
			if (i % n == 0) {
				printf("%016llx:  ", address);
			}
			int newline = (((i + 1) % n == 0) || (size == readsize && i == end - 1));
			printf("%0*llx%c", (int)(2 * width), value, (newline ? '\n' : ' '));
			address += width;
		}
		if (!read_success) {
			return false;
		}
		size -= readsize;
	}
	return true;
}

bool
memctl_dump(kaddr_t address, size_t size, bool physical, size_t width, size_t access) {
	assert(ispow2(width) && 0 < width && width <= 8);
	assert(ispow2(access) && access <= 8);
	uint8_t data[page_size];
	uint8_t *p = data;
	uint8_t *end = p;
	width--;
	bool read_success = true;
	/* Iterate one line of output at a time. */
	while (size > 0) {
		char hex[64];
		char ascii[32];
		unsigned hexidx = 0;
		unsigned asciiidx = 0;
		unsigned off = address & 0xf;
		address -= off;
		unsigned i = 0;
		/* Format any leading blanks due to starting on a misaligned address. */
		for (; i < off; i++) {
			hexidx += sprintf(hex + hexidx, "  ");
			if ((i & width) == width) {
				hexidx += sprintf(hex + hexidx, " ");
			}
			asciiidx += sprintf(ascii + asciiidx, " ");
		}
		/* Format the hex and ascii data read from the kernel. */
		for (; size > 0 && i < 16; i++, size--, p++) {
			if (p == end) {
				/* If the last time we grabbed data there was an error, report it
				   now. */
				if (!read_success) {
					return false;
				}
				/* Grab more data from the kernel. */
				size_t readsize = min(size, sizeof(data));
				read_success = read_memory(address + i, &readsize, data, physical,
						access);
				if (interrupted) {
					error_interrupt();
					return false;
				}
				if (readsize == 0) {
					return false;
				}
				p = data;
				end = data + readsize;
			}
			hexidx += sprintf(hex + hexidx, "%02x", *p);
			if ((i & width) == width) {
				hexidx += sprintf(hex + hexidx, " ");
			}
			asciiidx += sprintf(ascii + asciiidx, "%c",
					(isascii(*p) && isprint(*p) ? *p : '.'));
		}
		/* Format any trailing blanks due to ending on a misaligned address. */
		for (; i < 16; i++) {
			hexidx += sprintf(hex + hexidx, "  ");
			if ((i & width) == width) {
				hexidx += sprintf(hex + hexidx, " ");
			}
			asciiidx += sprintf(ascii + asciiidx, " ");
		}
		/* Print the dump line. */
		printf("%016llx:  %s |%s|\n", address, hex, ascii);
		/* Advance. */
		address += 16;
	}
	return true;
}

bool
memctl_read_string(kaddr_t address, size_t size, bool physical, size_t access) {
	assert(ispow2(access) && access <= 8);
	uint8_t data[page_size + 1];
	bool have_printed = false;
	bool read_success = true;
	bool end = false;
	while (!end) {
		size_t readsize = min(size, sizeof(data) - 1);
		read_success = read_memory(address, &readsize, data, physical, access);
		if (interrupted) {
			error_interrupt();
			return false;
		}
		data[readsize] = 0;
		size_t len = strlen((char *)data);
		size -= readsize;
		address += readsize;
		end = (len < readsize || size == 0 || !read_success);
		printf("%s%s", (char *)data, (end && (have_printed || len > 0) ? "\n" : ""));
		have_printed = true;
	}
	return read_success;
}

bool
memctl_dump_binary(kaddr_t address, size_t size, bool physical, size_t access) {
	assert(ispow2(access) && access <= 8);
	uint8_t data[page_size];
	while (size > 0) {
		size_t readsize = min(size, sizeof(data));
		bool read_success = read_memory(address, &readsize, data, physical, access);
		uint8_t *p = data;
		size_t left = readsize;
		while (left > 0) {
			if (interrupted) {
				error_interrupt();
				return false;
			}
			size_t written = fwrite(p, 1, left, stdout);
			if (ferror(stdout)) {
				error_internal("could not write to stdout");
				return false;
			}
			p += written;
			left -= written;
		}
		if (!read_success) {
			return false;
		}
		size -= readsize;
	}
	return true;
}

#if MEMCTL_DISASSEMBLY

bool
memctl_disassemble(kaddr_t address, size_t length, bool physical, size_t access) {
	uint8_t data[page_size];
	for (;;) {
		size_t size = min(length, sizeof(data));
		bool read_success = read_memory(address, &size, data, physical, access);
		if (interrupted) {
			error_interrupt();
			return false;
		}
		size_t count = -1;
		size_t left = size;
		disassemble(data, &left, &count, address);
		if (!read_success) {
			return false;
		}
		// If this is the last data read, we are done.
		if (size == length) {
			return true;
		}
		address += size - left;
		length  -= size - left;
	}
}

#endif // MEMCTL_DISASSEMBLY
