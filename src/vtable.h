#ifndef MEMCTL__VTABLE_H_
#define MEMCTL__VTABLE_H_

#include "memctl_types.h"

/*
 * vtable
 *
 * Description:
 * 	Get the address and size of the vtable for a given class.
 *
 * Parameters:
 * 		class_name		The name of the C++ class.
 * 		bundle_id		The name of the kext containing the class.
 * 	out	vtable			On return, the address of the vtable or 0 if the class's
 * 					vtable was not found.
 * 	out	size			On return, the size of the vtable. May be NULL.
 *
 * Returns:
 * 	true if no errors were encountered.
 *
 * Dependencies:
 * 	kernel subsystem
 */
bool vtable_for_class(const char *class_name, const char *bundle_id, kaddr_t *vtable,
		size_t *size);

#endif
