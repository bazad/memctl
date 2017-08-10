#include "aarch64/zone_element_size_finder.h"

/*
 * Locating _zone_element_size
 * ---------------------------
 *
 *  The zone_element_size function is useful for finding the size of a memory region allocated with
 *  kalloc. This special symbol finder locates _zone_element_size by disassembling __FREE to find a
 *  jump to _kfree_addr, then disassembling _kfree_addr to find a call to _zone_element_size.
 */

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"

#include "memctl/aarch64/ksim.h"

static kaddr_t _kfree_addr;
static kaddr_t _zone_element_size;

/*
 * find_symbol
 *
 * Description:
 * 	A symbol finder for kfree_addr and zone_element_size in the kernel.
 */
static kext_result
find_symbol(const struct kext *kext, const char *symbol, kaddr_t *addr, size_t *size) {
	assert(strcmp(kext->bundle_id, KERNEL_ID) == 0);
	kaddr_t static_addr;
	if (strcmp(symbol, "_kfree_addr") == 0) {
		static_addr = _kfree_addr;
	} else if (strcmp(symbol, "_zone_element_size") == 0) {
		static_addr = _zone_element_size;
	} else {
		return KEXT_NOT_FOUND;
	}
	*addr = static_addr + kext->slide;
	// We have no idea what the real size is, so don't set anything.
	return KEXT_SUCCESS;
}

void
kernel_symbol_finder_init_zone_element_size() {
#define WARN(sym)	memctl_warning("could not find %s", #sym)
	error_stop();
	struct ksim sim;
	kaddr_t __FREE = ksim_symbol(NULL, "__FREE");
	if (__FREE == 0) {
		WARN(__FREE);
		goto abort;
	}
	ksim_set_pc(&sim, __FREE);
	ksim_scan_for_jump(&sim, KSIM_FW, 0, &_kfree_addr, 8);
	if (_kfree_addr == 0) {
		WARN(_kfree_addr);
		goto abort;
	}
	ksim_set_pc(&sim, _kfree_addr);
	ksim_scan_for_call(&sim, KSIM_FW, 0, &_zone_element_size, 12);
	if (_zone_element_size == 0) {
		WARN(_zone_element_size);
		goto abort;
	}
	kext_add_symbol_finder(KERNEL_ID, find_symbol);
abort:
	error_start();
}
