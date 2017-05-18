#ifndef MEMCTL_CLI__FORMAT_H_
#define MEMCTL_CLI__FORMAT_H_

#include "memctl/memctl_types.h"

#if KERNEL_BITS == 32
# define KADDR_FMT	"%08x"
# define PADDR_FMT	"%x"
#elif KERNEL_BITS == 64
# define KADDR_FMT	"%016llx"
# define PADDR_FMT	"%llx"
#endif

#define KADDR_XFMT	"0x"KADDR_FMT
#define PADDR_XFMT	"0x"PADDR_FMT

/*
 * format_display_size
 *
 * Description:
 * 	Format the given size in bytes as a short display size. The resulting string is
 * 	guaranteed to be no more than 4 characters.
 *
 * Parameters:
 * 		buf			A 5-character buffer to be filled with the formatted size.
 * 		size			The size to format.
 */
void format_display_size(char buf[5], uint64_t size);

/*
 * format_memory_protection
 *
 * Description:
 * 	Format the given memory protection in the form "rwx", where non-present permissions are
 * 	replaced with "-".
 *
 * Parameters:
 * 		buf			A 4-character buffer to be filled with the formatted memory
 * 					permissions.
 * 		prot			The memory permissions to format.
 */
void format_memory_protection(char buf[4], int prot);

#endif
