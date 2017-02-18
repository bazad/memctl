#ifndef MEMCTL__UTILITY_H_
#define MEMCTL__UTILITY_H_

#include <assert.h>
#include <stdint.h>

/*
 * MACRO howmany_up
 *
 * Description:	Computes how many elements of size `b` fit in a block of size `a`, rounding up.
 *
 * Parameters:	a			The total size
 * 		b			The size of each element
 */
#define howmany_up(a, b)					\
	({ __typeof__(a) a0 = (a);				\
	   __typeof__(b) b0 = (b);				\
	   (a0 % b0 == 0) ? (a0 / b0) : (a0 / b0) + 1; })

/*
 * MACRO howmany2_up
 *
 * Description:	Computes how many elements of size `b` fit in a block of size `a`, rounding up.
 * `b` must be a power of 2.
 *
 * Parameters:	a			The total size
 * 		b			The size of each element, which must be a power of 2
 */
#define howmany2_up(a, b)					\
	({ __typeof__(b) b0 = (b);				\
	   ((a) + b0 - 1) / b0; })

/*
 * MACRO round2_down
 *
 * Description:	Rounds `a` down to the nearest multiple of `b`, which must be a power of 2.
 *
 * Parameters:	a			The value to round
 * 		b			The rounding granularity
 */
#define round2_down(a, b)	((a) & ~((b) - 1))

/*
 * MACRO round2_up
 *
 * Description:	Rounds `a` up to the nearest multiple of `b`, which must be a power of 2.
 *
 * Parameters:	a			The value to round
 * 		b			The rounding granularity
 */
#define round2_up(a, b)						\
	({ __typeof__(b) b0 = (b);				\
	   round2_down((a) + b0 - 1, b0); })

/*
 * MACRO min
 *
 * Description:	Return the minimum of the two arguments.
 */
#define min(a, b)						\
	({ __typeof__(a) a0 = (a);				\
	   __typeof__(b) b0 = (b);				\
	   (a0 < b0 ? a0 : b0); })

/*
 * MACRO max
 *
 * Description:	Return the maximum of the two arguments.
 */
#define max(a, b)						\
	({ __typeof__(a) a0 = (a);				\
	   __typeof__(b) b0 = (b);				\
	   (a0 > b0 ? a0 : b0); })

/*
 * MACRO ispow2
 *
 * Description:	Returns whether the argument is a power of 2 or 0.
 */
#define ispow2(x)						\
	({ __typeof__(x) x0 = (x);				\
	   ((x0 & (x0 - 1)) == 0); })

/*
 * lsl
 *
 * Description:
 * 	Logical shift left.
 *
 * Parameters:
 * 		x			The value to shift.
 * 		shift			The amount to shift.
 * 		width			The width of x in bits. Cannot be 0.
 */
static inline uint64_t
lsl(uint64_t x, unsigned shift, unsigned width) {
	return (x << shift) & ((2 << (width - 1)) - 1);
}

/*
 * lsr
 *
 * Description:
 * 	Logical shift right.
 *
 * Parameters:
 * 		x			The value to shift.
 * 		shift			The amount to shift.
 */
static inline uint64_t
lsr(uint64_t x, unsigned shift) {
	return (x >> shift);
}

/*
 * asr
 *
 * Description:
 * 	Arithmetic shift right.
 *
 * Parameters:
 * 		x			The value to shift.
 * 		shift			The amount to shift.
 * 		width			The width of x in bits. Cannot be 0.
 */
static inline uint64_t
asr(uint64_t x, unsigned shift, unsigned width) {
	return ((int64_t)x << (64 - width)) >> (64 - width + shift);
}

/*
 * ror
 *
 * Description:
 * 	Rotate right.
 *
 * Parameters:
 * 		x			The value to rotate.
 * 		shift			The amount to shift.
 * 		width			The width of x in bits. Cannot be 0.
 */
static inline uint64_t
ror(uint64_t x, unsigned shift, unsigned width) {
	unsigned s = shift % width;
	return lsl(x, width - s, width) | lsr(x, s);
}

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
