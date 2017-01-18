#ifndef MEMCTL_CLI__FORMAT_H_
#define MEMCTL_CLI__FORMAT_H_

#include "memctl_types.h"

#if KERNEL_BITS == 32
# define KADDR_FMT	"0x%08x"
#elif KERNEL_BITS == 64
# define KADDR_FMT	"0x%016llx"
#endif

#endif
