#include "strparse.h"

#include "memctl/utility.h"

int
hex_digit(int c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	} else if ('A' <= c && c <= 'F') {
		return c - 'A' + 0xa;
	} else if ('a' <= c && c <= 'f') {
		return c - 'a' + 0xa;
	} else {
		return -1;
	}
}

/*
 * handle_base_prefix
 *
 * Description:
 * 	Process the base prefix and set the base according to this prefix.
 */
static void
handle_base_prefix(const char **str0, unsigned *base0) {
	const char *str = *str0;
	unsigned base = *base0;
	assert(2 <= base && base <= 16);
	if (str[0] != '0') {
		return;
	}
	if (str[1] == 'x') {
		base = 16;
	} else if (str[1] == 'o') {
		base = 8;
	} else if (str[1] == 'b') {
		base = 2;
	} else {
		return;
	}
	*str0 = str + 2;
	*base0 = base;
}

enum strtoint_result
strtoint(const char *str, bool sign, unsigned base, uintmax_t *value, const char **end) {
	enum strtoint_result result = STRTOINT_OK;
	// Check for any sign.
	bool negate = false;
	if (sign && (str[0] == '+' || str[0] == '-')) {
		negate = (str[0] == '-');
		str += 1;
	}
	// Check for any base-specifying prefix.
	handle_base_prefix(&str, &base);
	// Process the digits.
	const char *start = str;
	uintmax_t value_ = 0;
	do {
		// Convert this digit.
		int d = hex_digit(str[0]);
		if (d < 0 || d >= base) {
			if (str == start) {
				result = STRTOINT_NODIGITS;
				goto fail;
			}
			result = STRTOINT_BADDIGIT;
			break;
		}
		uintmax_t new_value = value_ * base + d;
		// Check for overflow.
		if (sign) {
			// We need to test if new_value will be between INTMAX_MIN and INTMAX_MAX
			// after any negation. If new_value is not negated, this is simple: we've
			// overflowed if new_value > INTMAX_MAX. This condition is slightly
			// trickier if we are negating the result. We want to ensure that new_value
			// is between 0 and -INTMAX_MIN (since it will be negated). Unfortunately,
			// -INTMAX_MIN is a signed integer overflow and thus undefined behavior.
			// However, in two's complement, INTMAX_MIN has the most significant bit
			// set and all lower bits cleared, which is itself exactly the value we
			// want.
			uintmax_t max = (uintmax_t)(negate ? INTMAX_MIN : INTMAX_MAX);
			if (new_value > max) {
				result = STRTOINT_OVERFLOW;
				break;
			}
		} else if (new_value < value_) {
			result = STRTOINT_OVERFLOW;
			break;
		}
		// Move to the next character.
		value_ = new_value;
		str += 1;
	} while (str[0] != 0);
	// Compute the final value.
	if (negate) {
		value_ = (uintmax_t)(-(intmax_t)value_);
	}
	*value = value_;
fail:
	*end = str;
	return result;
}

enum strtodata_result
strtodata(const char *str, unsigned base, void *data, size_t *size, const char **end) {
	enum strtodata_result result = STRTODATA_OK;
	// Check for any base-specifying prefix.
	assert(base == 2 || base == 4 || base == 16);
	const char *start = str;
	handle_base_prefix(&str, &base);
	if (base != 2 && base != 4 && base != 16) {
		str = start;
		result = STRTODATA_BADBASE;
		goto fail;
	}
	// Process the digits.
	const unsigned bits_per_digit = ilog2(base);
	assert(bits_per_digit != 0 && ispow2(bits_per_digit) && bits_per_digit <= 4);
	start = str;
	uint8_t *p = (uint8_t *)data;
	size_t left = (p == NULL ? 0 : *size);
	size_t realsize = 0;
	do {
		// Grab one byte of data.
		uint8_t byte = 0;
		for (size_t i = 0; i < 8 / bits_per_digit; i++) {
			// Convert this digit.
			int d = hex_digit(str[0]);
			if (d < 0 || d >= base) {
				if (i == 0) {
					if (str == start) {
						result = STRTODATA_NODIGITS;
					} else {
						result = STRTODATA_BADDIGIT;
					}
					goto no_more_digits;
				}
				result = STRTODATA_NEEDDIGIT;
				goto fail;
			}
			// Add these bits to the byte.
			byte |= d << (8 - (i + 1) * bits_per_digit);
			// Move to the next character.
			str += 1;
		}
		realsize += 1;
		// Set this byte.
		if (left > 0) {
			*p = byte;
			p    += 1;
			left -= 1;
		}
	} while (str[0] != 0);
no_more_digits:
	// Return the size.
	*size = realsize;
fail:
	*end = str;
	return result;
}
