#ifndef MEMCTL__UTILITY_H_
#define MEMCTL__UTILITY_H_

#include <assert.h>
#include <stdint.h>

/*
 * MACRO howmany_up
 *
 * Description:
 * 	Compute how many elements of size `b` fit in a block of size `a`, rounding up.
 *
 * Parameters:	a			The total size
 * 		b			The size of each element
 */
#define howmany_up(a, b)						\
	({ __typeof__(a) _a = (a);					\
	   __typeof__(b) _b = (b);					\
	   (_a % _b == 0) ? (_a / _b) : (_a / _b) + 1; })

/*
 * MACRO howmany2_up
 *
 * Description:
 * 	Compute how many elements of size `b` fit in a block of size `a`, rounding up. `b` must be
 * 	a power of 2.
 *
 * Parameters:	a			The total size
 * 		b			The size of each element, which must be a power of 2
 */
#define howmany2_up(a, b)						\
	({ __typeof__(a) _a = (a);					\
	   __typeof__(b) _b = (b);					\
	   (_a + _b - 1) / _b; })

/*
 * MACRO round2_down
 *
 * Description:
 * 	Round `a` down to the nearest multiple of `b`, which must be a power of 2.
 *
 * Parameters:	a			The value to round
 * 		b			The rounding granularity
 */
#define round2_down(a, b)	((a) & ~((b) - 1))

/*
 * MACRO round2_up
 *
 * Description:
 * 	Round `a` up to the nearest multiple of `b`, which must be a power of 2.
 *
 * Parameters:	a			The value to round
 * 		b			The rounding granularity
 */
#define round2_up(a, b)							\
	({ __typeof__(a) _a = (a);					\
	   __typeof__(b) _b = (b);					\
	   round2_down(_a + _b - 1, _b); })

/*
 * MACRO min
 *
 * Description:
 * 	Return the minimum of the two arguments.
 */
#define min(a, b)							\
	({ __typeof__(a) _a = (a);					\
	   __typeof__(b) _b = (b);					\
	   (_a < _b ? _a : _b); })

/*
 * MACRO max
 *
 * Description:
 * 	Return the maximum of the two arguments.
 */
#define max(a, b)							\
	({ __typeof__(a) _a = (a);					\
	   __typeof__(b) _b = (b);					\
	   (_a > _b ? _a : _b); })

/*
 * MACRO ispow2
 *
 * Description:
 * 	Returns whether the argument is a power of 2 or 0.
 */
#define ispow2(x)							\
	({ __typeof__(x) _x = (x);					\
	   ((_x & (_x - 1)) == 0); })

/*
 * ones
 *
 * Description:
 * 	Returns a mask of the given number of bits.
 */
static inline uintmax_t
ones(unsigned n) {
	const unsigned bits = sizeof(uintmax_t) * 8;
	return (n == bits ? (uintmax_t)(-1) : ((uintmax_t)1 << n) - 1);
}

/*
 * testbit
 *
 * Description:
 * 	Returns true if bit n is set in x.
 */
static inline unsigned
testbit(uintmax_t x, unsigned n) {
	return ((x & ((uintmax_t)1 << n)) != 0);
}

/*
 * bext
 *
 * Description:
 * 	Extract bits lo to hi of x, inclusive, sign extending the result if sign is 1. Return the
 * 	extracted value shifted left by shift bits.
 */
static inline uintmax_t
bext(uintmax_t x, unsigned sign, unsigned hi, unsigned lo, unsigned shift) {
	const unsigned bits = sizeof(uintmax_t) * 8;
	unsigned d = bits - (hi - lo + 1);
	if (sign) {
		return (((((intmax_t)  x) >> lo) << d) >> (d - shift));
	} else {
		return (((((uintmax_t) x) >> lo) << d) >> (d - shift));
	}
}

/*
 * MACRO popcount
 *
 * Description:
 * 	Returns the popcount (number of 1 bits in the binary representation) of x.
 */
#ifdef __GNUC__
#define popcount(x)	__builtin_popcount(x)
#else
#error popcount not implemented
#endif

/*
 * msb1
 *
 * Description:
 * 	Computes the index (starting at 0) of the most significant 1 bit. If the input is 0, then
 * 	-1 is returned.
 *
 * TODO: Use a faster implementation.
 */
static inline int
msb1(uintmax_t n) {
	unsigned msb = -1;
	while (n > 0) {
		n >>= 1;
		msb += 1;
	}
	return msb;
}


/*
 * MACRO ilog2
 *
 * Description:
 * 	Computes the integer part (floor) of the logarithm base 2 of the given integer. If the
 * 	input is 0, -1 is returned.
 */
static inline int
ilog2(uintmax_t n) {
	return msb1(n);
}

/*
 * MACRO lsl
 *
 * Description:
 * 	Logical shift left.
 *
 * Parameters:
 * 		x			The value to shift.
 * 		shift			The amount to shift.
 * 		width			The width of x in bits. Cannot be 0.
 */
#define lsl(x, shift, width)	(((x) << (shift)) & (((uintmax_t)2 << ((width) - 1)) - 1))

/*
 * MACRO lsr
 *
 * Description:
 * 	Logical shift right.
 *
 * Parameters:
 * 		x			The value to shift.
 * 		shift			The amount to shift.
 */
#define lsr(x, shift)		((x) >> (shift))

/*
 * MACRO asr
 *
 * Description:
 * 	Arithmetic shift right.
 *
 * Parameters:
 * 		x			The value to shift.
 * 		shift			The amount to shift.
 * 		width			The width of x in bits. Cannot be 0.
 */
#define asr(x, shift, width)						\
	({ __typeof__(x) _x = (x);					\
	   unsigned _width = (width);					\
	   unsigned _shift = (shift);					\
	   unsigned _bits = 8 * sizeof(_x);				\
	   ((_x << (_bits - _width)) >> (_bits - _width + _shift)); })

/*
 * MACRO ror
 *
 * Description:
 * 	Rotate right.
 *
 * Parameters:
 * 		x			The value to rotate.
 * 		shift			The amount to shift.
 * 		width			The width of x in bits. Cannot be 0.
 */
#define ror(x, shift, width)						\
	({ __typeof__(x) _x = (x);					\
	   unsigned _shift = (shift);					\
	   unsigned _width = (width);					\
	   unsigned _s = _shift % _width;				\
	   __typeof__(x) _lsl = lsl(_x, _width - _s, _width);		\
	   __typeof__(x) _lsr = lsr(_x, _s);				\
	   _lsl | _lsr; })

/*
 * pack_uint
 *
 * Description:
 * 	Store the integer `value` into `dest` as a `width`-byte integer.
 *
 * Parameters:
 * 	out	dest			The place to store the result
 * 		value			The value to store
 * 		width			The width of the integer to store. `width` must be 1, 2,
 * 					4, or 8.
 */
static inline void
pack_uint(void *dest, uintmax_t value, unsigned width) {
	switch (width) {
		case 1: *(uint8_t  *)dest = value; break;
		case 2: *(uint16_t *)dest = value; break;
		case 4: *(uint32_t *)dest = value; break;
#ifdef UINT64_MAX
		case 8: *(uint64_t *)dest = value; break;
#endif
	}
}

/*
 * unpack_uint
 *
 * Description:
 * 	Extract the integer at the given address as a `width`-byte integer.
 *
 * Parameters:
 * 		src			A pointer to the value.
 * 		width			The width of the integer to read. `width` must be 1, 2,
 * 					4, or 8.
 *
 * Returns:
 * 	The `width`-byte integer in `src` as a uint64_t.
 */
static inline uintmax_t
unpack_uint(const void *src, unsigned width) {
	switch (width) {
		case 1:  return *(uint8_t  *)src;
		case 2:  return *(uint16_t *)src;
		case 4:  return *(uint32_t *)src;
#ifdef UINT64_MAX
		case 8:  return *(uint64_t *)src;
#endif
		default: assert(false);
	}
}

#endif
