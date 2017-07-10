#ifndef MEMCTL_CLI__STRPARSE_H_
#define MEMCTL_CLI__STRPARSE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * hex_digit
 *
 * Description:
 * 	Convert the given hexadecimal digit into its numeric value.
 *
 * Parameters:
 * 		c			The character to convert.
 *
 * Returns:
 * 	The numeric value of the digit, or -1 if the character is not a valid hexadecimal digit.
 */
int hex_digit(int c);

/*
 * enum strtoint_result
 *
 * Description:
 * 	A return value from strtoint.
 */
enum strtoint_result {
	// Success
	STRTOINT_OK,
	// An invalid digit was encountered. end will point to the invalid digit.
	STRTOINT_BADDIGIT,
	// No digits were found.
	STRTOINT_NODIGITS,
	// A (signed or unsigned) overflow was encountered while processing the integer. end will
	// point to the first digit that caused the overflow.
	STRTOINT_OVERFLOW,
};

/*
 * strtoint
 *
 * Description:
 * 	Parse the given string into an integer.
 *
 * Parameters:
 * 		str			The string to parse.
 * 		sign			Whether the integer to parse is signed or unsigned. If sign
 * 					is true, then the string representation of the integer may
 * 					optionally start with a "+" or "-" to indicate the sign.
 * 		base			The base to parse the integer, if no base prefix is
 * 					present in the string.
 * 	out	value			On return, the integer value that was parsed.
 * 	out	end			On return, the first character that wasn't parsed. If the
 * 					entire string was parsed, then end will point to the null
 * 					terminator.
 *
 * Returns:
 * 	A strtoint_result code.
 *
 * Notes:
 * 	The following base prefixes are recognized by this function:
 * 		0x	Hexadecimal (base 16)
 * 		0o	Octal (base 8)
 * 		0b	Binary (base 2)
 *
 * 	Unfortunately the choice of 0b for binary may lead to unexpected results, since "b" is a
 * 	valid hexadecimal digit. For example, trying to parse the string "0b10" with base set to 16
 * 	will actually return the value 2, not 0xB10, since the leading "0b" will be recognized as
 * 	the base specifier.
 */
enum strtoint_result strtoint(const char *str, bool sign, unsigned base,
		uintmax_t *value, const char **end);

/*
 * enum strtodata_result
 *
 * Description:
 * 	A return value from strtodata.
 */
enum strtodata_result {
	// Success
	STRTODATA_OK,
	// A bad base prefix was encountered. end will point to the base prefix.
	STRTODATA_BADBASE,
	// An invalid digit was encountered. end will point to the invalid digit.
	STRTODATA_BADDIGIT,
	// An invalid digit was encountered but more digits were needed. end will point to the
	// invalid digit.
	STRTODATA_NEEDDIGIT,
	// No digits were found.
	STRTODATA_NODIGITS,
};

/*
 * strtodata
 *
 * Description:
 * 	Parse the given string into a binary data blob.
 *
 * Parameters:
 * 		str			The string to parse.
 * 		base			The base to parse the data, if no base prefix is
 * 					present in the string.
 * 	out	data			The binary data.
 * 	out	size			The size of the data in bytes.
 * 	out	end			On return, the first character that wasn't parsed. If the
 * 					entire string was parsed, then end will point to the null
 * 					terminator.
 *
 * Returns:
 * 	A strtodata_result code.
 *
 * Notes:
 * 	See strtoint.
 *
 * 	The following base prefixes are recognized by this function:
 * 		0x	Hexadecimal (base 16)
 * 		0b	Binary (base 2)
 *
 * 	The only supported bases are 2, 4, and 16.
 */
enum strtodata_result strtodata(const char *str, unsigned base, void *data, size_t *size,
		const char **end);

#endif
