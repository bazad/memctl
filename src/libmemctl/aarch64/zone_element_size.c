#include "aarch64/zone_element_size.h"
/*
 * Locating _zone_element_size
 * ---------------------------
 *
 * The zone_element_size function is useful for finding the size of a memory region allocated with
 * kalloc. This special symbol finder locates _zone_element_size by disassembling __FREE to find a
 * call to _kfree_addr, then disassembling _kfree_addr to find a call to _zone_element_size.
 */

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"

#include "memctl/aarch64/disasm.h"
#include "memctl/aarch64/ksim.h"

static kaddr_t _kfree_addr;
static kaddr_t _zone_element_size;

/*
 * get_free_addr
 *
 * Description:
 * 	Get the static address of the __FREE symbol.
 */
static kaddr_t
get_free_addr(void) {
	kaddr_t __FREE;
	kext_result kr = kernel_symbol("__FREE", &__FREE, NULL);
	// Subtract out the slide, since we're simulating using the static addresses.
	return (kr == KEXT_SUCCESS ? __FREE - kernel.slide : 0);
}

/*
 * stop_at_b
 *
 * Description:
 * 	Stop at a B instruction.
 */
static bool
stop_at_b(struct ksim *ksim, uint32_t ins) {
	return AARCH64_INS_TYPE(ins, B_INS);
}

#define MAX_KFREE_ADDR_INSTRUCTIONS 8

/*
 * get_kfree_addr
 *
 * Description:
 * 	Find the address of _kfree_addr by simulating the execution of __FREE.
 */
static bool
get_kfree_addr(kaddr_t __FREE) {
	struct ksim ksim;
	if (!ksim_init_kext(&ksim, &kernel, __FREE)) {
		return false;
	}
	ksim.max_instruction_count = MAX_KFREE_ADDR_INSTRUCTIONS;
	ksim.stop_after = stop_at_b;
	if (!ksim_run(&ksim)) {
		return false;
	}
	_kfree_addr = ksim.sim.PC.value;
	return true;
}

/*
 * get_bl
 *
 * Description:
 * 	Take a branch from a BL instruction then stop.
 */
static bool
get_bl(struct ksim *ksim, uint32_t ins, uint64_t branch_address, bool *take_branch, bool *stop) {
	if (!AARCH64_INS_TYPE(ins, BL_INS) || branch_address == KSIM_PC_UNKNOWN) {
		return false;
	}
	*take_branch = true;
	*stop = true;
	return true;
}

#define MAX_ZONE_ELEMENT_SIZE_INSTRUCTIONS 12

static bool
get_zone_element_size() {
	struct ksim ksim;
	if (!ksim_init_kext(&ksim, &kernel, _kfree_addr)) {
		return false;
	}
	ksim.max_instruction_count = MAX_ZONE_ELEMENT_SIZE_INSTRUCTIONS;
	ksim.handle_branch = get_bl;
	if (!ksim_run(&ksim)) {
		return false;
	}
	_zone_element_size = ksim.sim.PC.value;
	return true;
}

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
	kaddr_t __FREE = get_free_addr();
	if (__FREE == 0) {
		WARN(__FREE);
		goto abort;
	}
	if (!get_kfree_addr(__FREE)) {
		WARN(_kfree_addr);
		goto abort;
	}
	if (!get_zone_element_size()) {
		WARN(_zone_element_size);
		goto abort;
	}
	kext_add_symbol_finder(KERNEL_ID, find_symbol);
abort:
	error_start();
}
