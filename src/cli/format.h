#ifndef MEMCTL_CLI__FORMAT_H_
#define MEMCTL_CLI__FORMAT_H_

#include "memctl_types.h"

#if KERNEL_BITS == 32
# define KADDR_FMT	"%08x"
# define PADDR_FMT	"%x"
#elif KERNEL_BITS == 64
# define KADDR_FMT	"%016llx"
# define PADDR_FMT	"%llx"
#endif

#define KADDR_XFMT	"0x"KADDR_FMT
#define PADDR_XFMT	"0x"PADDR_FMT

#endif
