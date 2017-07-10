#ifndef MEMCTL__AARCH64__ZONE_ELEMENT_SIZE_FINDER_H_
#define MEMCTL__AARCH64__ZONE_ELEMENT_SIZE_FINDER_H_

/*
 * kernel_symbol_finder_init_zone_element_size
 *
 * Description:
 * 	Add a kernel symbol finder for the _kfree_addr and _zone_element_size functions on AArch64.
 */
void kernel_symbol_finder_init_zone_element_size(void);

#endif
