#include "memctl_offsets.h"

#include "kernel.h"
#include "memctl_error.h"

DEFINE_OFFSET(IORegistryEntry, reserved);
DEFINE_OFFSET(IORegistryEntry__ExpansionData, fRegistryEntryID);
DEFINE_OFFSET(OSString, string);

void
offsets_default_init() {
#define DEFAULT(base, object, value)			\
	if (OFFSET(base, object).valid == 0) {		\
		OFFSET(base, object).offset = value;	\
		OFFSET(base, object).valid = 1;		\
	}
	DEFAULT(IORegistryEntry,                reserved,         2 * sizeof(kword_t))
	DEFAULT(IORegistryEntry__ExpansionData, fRegistryEntryID, 1 * sizeof(kword_t))
#undef DEFAULT
#define RESOLVE_KERNEL(symbol)								\
	if (ADDRESS(kernel, symbol).valid == 0) {					\
		kaddr_t address;							\
		kext_result kr = kext_resolve_symbol(&kernel, #symbol, &address, NULL);	\
		if (kr == KEXT_ERROR) {							\
			error_clear();							\
		} else if (kr == KEXT_SUCCESS) {					\
			ADDRESS(kernel, symbol).offset = address;			\
			ADDRESS(kernel, symbol).valid = 1;				\
		}									\
	}
#undef RESOLVE_KERNEL
}
