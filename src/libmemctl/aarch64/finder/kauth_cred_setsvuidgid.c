#include "aarch64/finder/kauth_cred_setsvuidgid.h"

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"

#include "memctl/aarch64/ksim.h"

static kaddr_t _kauth_cred_setsvuidgid;

/*
 * find_symbol
 *
 * Description:
 * 	A symbol finder for kauth_cred_setsvuidgid.
 */
static kext_result
find_symbol(const struct kext *kext, const char *symbol, kaddr_t *addr, size_t *size) {
	assert(strcmp(kext->bundle_id, KERNEL_ID) == 0);
	if (strcmp(symbol, "_kauth_cred_setsvuidgid") != 0) {
		return KEXT_NOT_FOUND;
	}
	*addr = _kauth_cred_setsvuidgid + kext->slide;
	return KEXT_SUCCESS;
}

void
kernel_symbol_finder_init_kauth_cred_setsvuidgid() {
#define NOSTR(str)	memctl_warning("could not find reference to string '%s'", str)
#define WARN(sym)	memctl_warning("could not find %s", #sym)
	error_stop();
	struct ksim sim;
	// Find the address of the instruction that creates a reference to the string
	// "kauth_cred_setsvuidgid" in register X8.
	const char *reference = "kauth_cred_setsvuidgid";
	kaddr_t strref_ins = ksim_string_reference(NULL, reference);
	if (strref_ins == 0) {
		NOSTR(reference);
		goto abort;
	}
	ksim_set_pc(&sim, strref_ins);
	// Find a "STP <Xt1>, <Xt2>, [SP, #<imm>]!" instruction that marks the beginning of this
	// function.
	const uint32_t start_ins  = 0xa98003e0;
	const uint32_t start_mask = 0xffc003e0;
	ksim_scan_for(&sim, KSIM_BW, start_ins, start_mask, 0, &_kauth_cred_setsvuidgid, 50);
	if (_kauth_cred_setsvuidgid == 0) {
		WARN(_kauth_cred_setsvuidgid);
		goto abort;
	}
	kext_add_symbol_finder(KERNEL_ID, find_symbol);
abort:
	error_start();
}
