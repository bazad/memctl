#ifndef MEMCTL__MEMCTL_TYPES_H_
#define MEMCTL__MEMCTL_TYPES_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// libmemctl must be compiled for the exact same architecture as the running kernel. It will not
// work as a 32-bit program on a 64-bit kernel.
/*
 * KERNEL_BITS
 *
 * Description:
 * 	The number of bits in a machine word.
 */
#ifndef KERNEL_BITS
# if UINTPTR_MAX == UINT32_MAX
// TODO: Add support for 32-bit kernels on iOS and macOS.
#  error 32-bit kernels are not yet supported.
#  define KERNEL_BITS 32
# elif UINTPTR_MAX == UINT64_MAX
#  define KERNEL_BITS 64
# else
#  error Unsupported kernel word size.
# endif
#endif // KERNEL_BITS

/*
 * KERNELCACHE
 *
 * Description:
 * 	1 if the platform has a kernelcache.
 */
#ifndef KERNELCACHE
# if __arm64__ || __arm__
#  define KERNELCACHE 1
# else
#  define KERNELCACHE 0
# endif
#endif // KERNELCACHE

/*
 * kword_t
 *
 * Description:
 * 	An unsigned integer of the same size as a word in the kernel. This type is interchangeable
 * 	with kaddr_t below.
 */
#if KERNEL_BITS == 64
typedef uint64_t kword_t;
#endif

/*
 * kaddr_t
 *
 * Description:
 * 	An alias for kword_t that indicates that the given integer represents a kernel address.
 */
typedef kword_t kaddr_t;

/*
 * paddr_t
 *
 * Description:
 * 	A physical address (not a physical page number).
 */
typedef kword_t paddr_t;

#endif
