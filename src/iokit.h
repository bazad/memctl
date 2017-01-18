#ifndef MEMCTL__IOKIT_H_
#define MEMCTL__IOKIT_H_

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE

#include "external/IOKit/IOKitLib.h"

#else // !TARGET_OS_IPHONE

#include <IOKit/IOKitLib.h>

#endif // TARGET_OS_IPHONE

#endif
