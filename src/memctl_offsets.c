#include "memctl_offsets.h"

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
	DEFAULT(IORegistryEntry,                reserved,         2 * sizeof(kword_t));
	DEFAULT(IORegistryEntry__ExpansionData, fRegistryEntryID, 1 * sizeof(kword_t));
	// TODO
#undef DEFAULT
}
