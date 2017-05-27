/*
 * A test utility for the AArch64 disassembler.
 */
#include "disassemble.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

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
			fprintf(stderr, "unexpected end-of-file\n");
			exit(1);
		}
		buf[i] = ch;
	}
	buf[16] = 0;
	char *end;
	uint64_t pc = strtoull(buf, &end, 16);
	if (end != buf + 16) {
		fprintf(stderr, "bad pc value\n");
		exit(1);
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
		fprintf(stderr, "unexpected end-of-file\n");
		exit(1);
	}
	int b_lo = hexdigit(ch);
	if (b_lo < 0) {
		goto bad_hex;
	}
	*byte = (b_hi << 4) | b_lo;
	return true;
bad_hex:
	fprintf(stderr, "invalid hex\n");
	exit(1);
}

static bool read_ins(uint32_t *ins) {
	uint8_t b[4];
	if (!read_hexpair(&b[3])) {
		return false;
	}
	if (!read_hexpair(&b[2]) || !read_hexpair(&b[1]) || !read_hexpair(&b[0])) {
		fprintf(stderr, "unexpected end-of-file\n");
		exit(1);
	}
	*ins = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
	return true;
}

int main(int argc, const char *argv[]) {
	uint64_t pc = read_pc();
	uint32_t ins;
	while (read_ins(&ins)) {
		size_t size = sizeof(ins);
		size_t count = 1;
		disassemble(&ins, &size, &count, pc);
		pc += sizeof(ins);
	}
	return 0;
}
