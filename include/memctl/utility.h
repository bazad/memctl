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
 * MACRO ones
 *
 * Description:
 * 	Returns a mask of the given number of bits.
 */
#define ones(n)								\
	({ unsigned _n = (n);						\
	   unsigned _bits = sizeof(uintmax_t) * 8;			\
	   (_n == _bits ? (uintmax_t)(-1) : ((uintmax_t)1 << _n) - 1); })

/*
 * MACRO testbit
 *
 * Description:
 * 	Returns true if bit n is set in x.
 */
#define testbit(x, n)		(((x) & ((uintmax_t)1 << (n))) != 0)

/*
 * MACRO bext
 *
 * Description:
 * 	Extract bits lo to hi of x, inclusive, sign extending the result if sign is 1. Return the
 * 	extracted value shifted left by shift bits.
 */
#define bext(x, sign, hi, lo, shift)					\
	({ __typeof__(x) _x = (x);					\
	   unsigned _sign = (sign);					\
	   unsigned _hi = (hi);						\
	   unsigned _lo = (lo);						\
	   unsigned _shift = (shift);					\
	   unsigned _bits = sizeof(uintmax_t) * 8;			\
	   unsigned _d = _bits - (_hi - _lo + 1);			\
	   (_sign							\
	    ? ((((intmax_t)  _x) >> _lo) << _d) >> (_d - _shift)	\
	    : ((((uintmax_t) _x) >> _lo) << _d) >> (_d - _shift)); })

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
pack_uint(void *dest, uint64_t value, unsigned width) {
	switch (width) {
		case 1: *(uint8_t  *)dest = value; break;
		case 2: *(uint16_t *)dest = value; break;
		case 4: *(uint32_t *)dest = value; break;
		case 8: *(uint64_t *)dest = value; break;
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
static inline uint64_t
unpack_uint(const void *src, unsigned width) {
	switch (width) {
		case 1:  return *(uint8_t  *)src;
		case 2:  return *(uint16_t *)src;
		case 4:  return *(uint32_t *)src;
		case 8:  return *(uint64_t *)src;
		default: assert(false);
	}
}

#endif
