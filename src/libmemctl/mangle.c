#include "mangle.h"

#include "memctl/utility.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MANGLE_PREFIX	"__Z"
#define VTABLE		"TV"
#define NESTED_PREFIX	"N"
#define NESTED_SUFFIX	"E"

// State for buffer writing.
struct wstate {
	char *buf;
	const char *end;
	size_t written;
};

#define INIT_WSTATE(buf, size)	{ buf, buf + size, 0 }

/*
 * writefmt
 *
 * Description:
 * 	Write a formatted string to a buffer.
 */
static void
writefmt(struct wstate *state, const char *fmt, ...) {
	size_t left = state->end - state->buf;
	va_list ap;
	va_start(ap, fmt);
	int written = vsnprintf(state->buf, left, fmt, ap);
	va_end(ap);
	assert(written >= 0);
	state->buf     += min(left, written);
	state->written += written;
}

/*
 * writestr
 *
 * Description:
 * 	Write a literal string.
 */
static void
writestr(struct wstate *state, const char *str) {
	writefmt(state, "%s", str);
}

/*
 * write_nested_name
 *
 * Description:
 * 	Write a nested name to a buffer.
 */
static void
write_nested_name(struct wstate *state, const char *name[], size_t name_count) {
	if (name_count > 1) {
		writestr(state, NESTED_PREFIX);
	}
	for (size_t i = 0; i < name_count; i++) {
		writefmt(state, "%zu%s", strlen(name[i]), name[i]);
	}
	if (name_count > 1) {
		writestr(state, NESTED_SUFFIX);
	}
}

size_t
mangle_class_name(char *buf, size_t size, const char *scoped_class_name[], size_t name_count) {
	assert(name_count >= 1);
	struct wstate state = INIT_WSTATE(buf, size);
	writestr(&state, MANGLE_PREFIX);
	write_nested_name(&state, scoped_class_name, name_count);
	return state.written;
}

size_t
mangle_class_vtable(char *buf, size_t size, const char *scoped_class_name[], size_t name_count) {
	assert(name_count >= 1);
	struct wstate state = INIT_WSTATE(buf, size);
	writestr(&state, MANGLE_PREFIX);
	writestr(&state, VTABLE);
	write_nested_name(&state, scoped_class_name, name_count);
	return state.written;
}

/*
 * readstr
 *
 * Description:
 * 	Read a literal string from a buffer.
 */
static bool
readstr(char **mangled, const char *str) {
	size_t len = strlen(str);
	if (strncmp(*mangled, str, len) != 0) {
		return false;
	}
	*mangled += len;
	return true;
}

/*
 * read_length
 *
 * Read a length from a buffer.
 */
static bool
read_length(char **mangled, size_t *length) {
	char *m = *mangled;
	if (*m == '0') {
		return false;
	}
	size_t len = 0;
	for (;;) {
		char ch = *m;
		if (ch < '0' || ch > '9') {
			break;
		}
		unsigned newlen = len * 10 + (ch - '0');
		if (newlen < len) {
			return false;
		}
		len = newlen;
		m++;
	}
	if (len == 0) {
		return false;
	}
	*mangled = m;
	*length = len;
	return true;
}

/*
 * read_name
 *
 * Description:
 * 	Read a single length-prefixed name.
 */
static bool
read_name(char **mangled, char **name, size_t *length) {
	char *m = *mangled;
	size_t len;
	if (!read_length(&m, &len)) {
		return false;
	}
	if (strnlen(m, len) < len) {
		return false;
	}
	*name    = m;
	*mangled = m + len;
	*length  = len;
	return true;
}

/*
 * read_nested_name_add
 *
 * Description:
 * 	A helper function to parse a single part of a nested name.
 */
static bool
read_nested_name_add(char **mangled, char *nested_name[], char *end[], size_t capacity,
		size_t *count) {
	char *name;
	size_t length;
	if (!read_name(mangled, &name, &length)) {
		return false;
	}
	size_t idx = *count;
	if (idx < capacity) {
		nested_name[idx] = name;
		end[idx]         = name + length;
	}
	*count = idx + 1;
	return true;
}

/*
 * read_nested_name
 *
 * Description:
 * 	Parse a nested name.
 */
static bool
read_nested_name(char **mangled, char *nested_name[], char *end[], size_t *name_count) {
	char *m = *mangled;
	size_t count = 0;
	size_t capacity = *name_count;
	bool nested = readstr(&m, NESTED_PREFIX);
	if (!read_nested_name_add(&m, nested_name, end, capacity, &count)) {
		return false;
	}
	if (nested) {
		for (;;) {
			if (!read_nested_name_add(&m, nested_name, end, capacity, &count)) {
				return false;
			}
			if (readstr(&m, NESTED_SUFFIX)) {
				break;
			}
		}
	}
	*mangled = m;
	*name_count = count;
	return true;
}

size_t
demangle_class_vtable(char *scoped_class_name[], size_t name_count, char *mangled) {
	assert(name_count < 256);
	if (!readstr(&mangled, MANGLE_PREFIX)) {
		return 0;
	}
	if (!readstr(&mangled, VTABLE)) {
		return 0;
	}
	size_t parse_count = name_count;
	char *ends[name_count];
	if (!read_nested_name(&mangled, scoped_class_name, ends, &name_count)) {
		return 0;
	}
	if (*mangled != 0) {
		return 0;
	}
	for (size_t i = 0; i < parse_count; i++) {
		*(ends[i]) = 0;
	}
	return name_count;
}
