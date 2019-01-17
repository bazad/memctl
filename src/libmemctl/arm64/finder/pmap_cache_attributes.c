#include "arm64/finder/pmap_cache_attributes.h"

#include "memctl/memctl_error.h"
#include "memctl/arm64/ksim.h"

void
kernel_find_pmap_cache_attributes(struct kext *kernel) {
#define NOSYM(sym)	memctl_warning("could not find %s", #sym)
	// Get the address of the bzero_phys function.
	kaddr_t _bzero_phys = ksim_symbol(NULL, "_bzero_phys");
	if (_bzero_phys == 0) {
		NOSYM(_bzero_phys);
		return;
	}
	// Find the first function call, which is to pmap_cache_attributes.
	struct ksim sim;
	ksim_set_pc(&sim, _bzero_phys);
	kaddr_t _pmap_cache_attributes = 0;
	ksim_scan_for_call(&sim, KSIM_FW, 0, NULL, &_pmap_cache_attributes, 32);
	if (_pmap_cache_attributes == 0) {
		NOSYM(_pmap_cache_attributes);
		return;
	}
	// Add the symbol.
	symbol_table_add_symbol(&kernel->symtab, "_pmap_cache_attributes", _pmap_cache_attributes);
}
