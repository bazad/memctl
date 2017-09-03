#include "aarch64/finder/zone_element_size.h"

/*
 * Locating _zone_element_size
 * ---------------------------
 *
 *  The zone_element_size function is useful for finding the size of a memory region allocated with
 *  kalloc. This special symbol finder locates _zone_element_size by disassembling __FREE to find a
 *  jump to _kfree_addr, then disassembling _kfree_addr to find a call to _zone_element_size.
 */

#include "memctl/memctl_error.h"
#include "memctl/aarch64/ksim.h"

void
kernel_find_zone_element_size(struct kext *kernel) {
#define NOSYM(sym)	memctl_warning("could not find %s", #sym)
	// Find __FREE.
	kaddr_t __FREE = ksim_symbol(NULL, "__FREE");
	if (__FREE == 0) {
		NOSYM(__FREE);
		return;
	}
	// Find _kfree_addr.
	struct ksim sim;
	ksim_set_pc(&sim, __FREE);
	kaddr_t _kfree_addr = 0;
	ksim_scan_for_jump(&sim, KSIM_FW, 0, NULL, &_kfree_addr, 8);
	if (_kfree_addr == 0) {
		NOSYM(_kfree_addr);
		return;
	}
	symbol_table_add_symbol(&kernel->symtab, "_kfree_addr", _kfree_addr);
	// Find _zone_element_size.
	kaddr_t _zone_element_size = 0;
	ksim_set_pc(&sim, _kfree_addr);
	ksim_scan_for_call(&sim, KSIM_FW, 0, NULL, &_zone_element_size, 12);
	if (_zone_element_size == 0) {
		NOSYM(_zone_element_size);
		return;
	}
	symbol_table_add_symbol(&kernel->symtab, "_zone_element_size", _zone_element_size);
}
