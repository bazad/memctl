#ifndef MEMCTL_CLI__VMMAP_H_
#define MEMCTL_CLI__VMMAP_H_

#include "memctl_types.h"

/*
 * memctl_vmmap
 *
 * Description:
 * 	TODO
 */
bool memctl_vmmap(kaddr_t kaddr, kaddr_t end, uint32_t depth);

#endif
