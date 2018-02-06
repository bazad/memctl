#include "arm64/finder/kauth_cred_setsvuidgid.h"

#include "memctl/arm64/ksim.h"
#include "memctl/memctl_error.h"
#include "memctl/platform.h"


void
kernel_find_kauth_cred_setsvuidgid(struct kext *kernel) {
#define NOSTR(str)	memctl_warning("could not find reference to string '%s'", str)
#define NOSYM(sym)	memctl_warning("could not find %s", #sym)
	// Find the address of the instruction that creates a reference to the string
	// "kauth_cred_setsvuidgid" in register X8.
	const char *reference = "kauth_cred_setsvuidgid";
	kaddr_t strref_ins = ksim_string_reference(NULL, reference);
	if (strref_ins == 0) {
		NOSTR(reference);
		return;
	}
	// Find the instruction that marks the beginning of this function.
	// - iOS 10:    STP   <Xt1>, <Xt2>, [SP, #<imm>]!
	// - iOS 11:    SUB   <SP>, <SP>, #<imm>
	bool ios11 = PLATFORM_XNU_VERSION_GE(17, 0, 0);
	const uint32_t start_ins  = (ios11 ? 0xd10003ff : 0xa98003e0);
	const uint32_t start_mask = (ios11 ? 0xff0003ff : 0xffc003e0);
	struct ksim sim;
	ksim_set_pc(&sim, strref_ins);
	kaddr_t _kauth_cred_setsvuidgid = 0;
	ksim_scan_for(&sim, KSIM_BW, start_ins, start_mask, 0, &_kauth_cred_setsvuidgid, 50);
	if (_kauth_cred_setsvuidgid == 0) {
		NOSYM(_kauth_cred_setsvuidgid);
		return;
	}
	// Add the symbol.
	symbol_table_add_symbol(&kernel->symtab, "_kauth_cred_setsvuidgid",
			_kauth_cred_setsvuidgid);
}
