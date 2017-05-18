#ifndef MEMCTL__MEMCTL_OFFSETS_H_
#define MEMCTL__MEMCTL_OFFSETS_H_

#include "memctl/offset.h"

#define DECLARE_OFFSET(base, object)	\
extern struct offset OFFSET(base, object)

#define DEFINE_OFFSET(base, object)	\
struct offset OFFSET(base, object)

#define DECLARE_ADDRESS(kext, object)	\
extern struct offset ADDRESS(kext, object)

#define DEFINE_ADDRESS(kext, object)	\
struct offset ADDRESS(kext, object)

DECLARE_OFFSET(IORegistryEntry, reserved);
DECLARE_OFFSET(IORegistryEntry__ExpansionData, fRegistryEntryID);
DECLARE_OFFSET(OSString, string);

DECLARE_ADDRESS(kernel, _copyout);

/*
 * offsets_default_init
 *
 * Description:
 * 	Initialize any offsets and addresses we can to default values. These are most likely
 * 	correct, but because some offsets are not validated against the kernel binary, there is a
 * 	chance they are incorrect.
 *
 * Notes:
 * 	Clients should initialize any offsets they need and supply correct values to those they
 * 	know are initialized incorrectly.
 */
void offsets_default_init(void);

#endif
