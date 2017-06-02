/*
 * A test utility for the AArch64 disassembler.
 */
#include "disassemble.h"

#include "memctl/macho.h"
#include "memctl/memctl_error.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void _Noreturn verror(const char *fmt, va_list ap) {
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	exit(1);
}

static void _Noreturn error(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	verror(fmt, ap);
}

void error_macho(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	verror(fmt, ap);
}

static int read_char() {
retry:;
	int ch = fgetc(stdin);
	if (ch == '#') {
		do {
			ch = fgetc(stdin);
			if (ch == '\n') {
				goto retry;
			}
		} while (ch != EOF);
	}
	return ch;
}

static uint64_t read_pc() {
	char buf[16+1];
	for (size_t i = 0; i < 16; i++) {
		int ch = read_char();
		if (ch == EOF) {
			error("unexpected end-of-file");
		}
		buf[i] = ch;
	}
	buf[16] = 0;
	char *end;
	uint64_t pc = strtoull(buf, &end, 16);
	if (end != buf + 16) {
		error("bad pc value");
	}
	return pc;
}

static int hexdigit(int ch) {
	if ('0' <= ch && ch <= '9') {
		return ch - '0';
	} else if ('A' <= ch && ch <= 'F') {
		return ch - 'A' + 0xa;
	} else if ('a' <= ch && ch <= 'f') {
		return ch - 'a' + 0xa;
	} else {
		return -1;
	}
}

static bool read_hexpair(uint8_t *byte) {
	int ch;
	do {
		ch = read_char();
		if (ch == EOF) {
			return false;
		}
	} while (isspace(ch));
	int b_hi = hexdigit(ch);
	if (b_hi < 0) {
		goto bad_hex;
	}
	ch = read_char();
	if (ch == EOF) {
		error("unexpected end-of-file");
	}
	int b_lo = hexdigit(ch);
	if (b_lo < 0) {
		goto bad_hex;
	}
	*byte = (b_hi << 4) | b_lo;
	return true;
bad_hex:
	error("invalid hex");
}

static bool read_ins(uint32_t *ins) {
	uint8_t b[4];
	if (!read_hexpair(&b[3])) {
		return false;
	}
	if (!read_hexpair(&b[2]) || !read_hexpair(&b[1]) || !read_hexpair(&b[0])) {
		error("unexpected end-of-file");
	}
	*ins = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
	return true;
}

static void disassemble_test() {
	uint64_t pc = read_pc();
	uint32_t ins;
	while (read_ins(&ins)) {
		size_t size = sizeof(ins);
		size_t count = 1;
		disassemble(&ins, &size, &count, pc);
		pc += sizeof(ins);
	}
}

static void read_all(void **data, size_t *size) {
	size_t alloc = 0x1000;
	uint8_t *buf = NULL;
	size_t filled = 0;
	for (;;) {
		buf = realloc(buf, alloc);
		if (buf == NULL) {
			error("out of memory");
		}
		while (filled < alloc) {
			int ch = fgetc(stdin);
			if (ch == EOF) {
				*data = buf;
				*size = filled;
				return;
			}
			buf[filled++] = ch;
		}
		alloc *= 2;
	}
}

static void disassemble_macho() {
	struct macho macho;
	read_all(&macho.mh, &macho.size);
	macho_result mr = macho_validate(macho.mh, macho.size);
	if (mr != MACHO_SUCCESS) {
		error("invalid macho file");
	}
	const struct load_command *lc = NULL;
	for (;;) {
		lc = macho_next_segment(&macho, lc);
		if (lc == NULL) {
			return;
		}
		int initprot;
		if (macho_is_64(&macho)) {
			const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
			initprot = sc->initprot;
		} else {
			const struct segment_command *sc = (const struct segment_command *)lc;
			initprot = sc->initprot;
		}
		if ((initprot & VM_PROT_EXECUTE) == 0) {
			continue;
		}
		const void *data;
		uint64_t addr;
		size_t size;
		macho_segment_data(&macho, lc, &data, &addr, &size);
		size_t count = -1;
		disassemble(data, &size, &count, addr);
	}
}

int main(int argc, const char *argv[]) {
	if (argc == 2 && strcmp(argv[1], "-m") == 0) {
		disassemble_macho();
	} else if (argc == 2 && strcmp(argv[1], "-t") == 0) {
		disassemble_test();
	} else {
		error("invalid argument");
	}
	return 0;
}
