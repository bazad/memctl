#ifndef MEMCTL__MACH_VM_H_
#define MEMCTL__MACH_VM_H_

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE

// TODO: this should just be #include "mach/mach_vm.h"
#include "external/mach/mach_vm.h"

#else // !TARGET_OS_IPHONE

#include <mach/mach_vm.h>

#endif // TARGET_OS_IPHONE

#endif
