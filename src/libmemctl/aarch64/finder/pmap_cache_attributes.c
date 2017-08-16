#include "aarch64/finder/pmap_cache_attributes.h"

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"

#include "memctl/aarch64/ksim.h"

static kaddr_t _pmap_cache_attributes;

/*
 * find_symbol
 *
 * Description:
 * 	A symbol finder for pmap_cache_attributes.
 */
static kext_result
find_symbol(const struct kext *kext, const char *symbol, kaddr_t *addr, size_t *size) {
	assert(strcmp(kext->bundle_id, KERNEL_ID) == 0);
	if (strcmp(symbol, "_pmap_cache_attributes") != 0) {
		return KEXT_NOT_FOUND;
	}
	*addr = _pmap_cache_attributes + kext->slide;
	return KEXT_SUCCESS;
}

void
kernel_symbol_finder_init_pmap_cache_attributes() {
#define WARN(sym)	memctl_warning("could not find %s", #sym)
	error_stop();
	struct ksim sim;
	// Get the address of the bzero_phys function.
	kaddr_t _bzero_phys = ksim_symbol(NULL, "_bzero_phys");
	if (_bzero_phys == 0) {
		WARN(_bzero_phys);
		goto abort;
	}
	// Find the first function call, which is to pmap_cache_attributes.
	ksim_set_pc(&sim, _bzero_phys);
	ksim_scan_for_call(&sim, KSIM_FW, 0, NULL, &_pmap_cache_attributes, 24);
	if (_pmap_cache_attributes == 0) {
		WARN(_pmap_cache_attributes);
		goto abort;
	}
	kext_add_symbol_finder(KERNEL_ID, find_symbol);
abort:
	error_start();
}
