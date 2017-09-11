#ifndef MEMCTL__AARCH64__FINDER__VTABLES_H_
#define MEMCTL__AARCH64__FINDER__VTABLES_H_

#include "memctl/kernel.h"

/*
 * kext_find_vtables
 *
 * Description:
 * 	A special symbol finder for vtables in the kernel and kernel extensions.
 */
void kext_find_vtables(struct kext *kext);

#endif
