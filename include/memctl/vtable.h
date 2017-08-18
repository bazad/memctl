#ifndef MEMCTL__VTABLE_H_
#define MEMCTL__VTABLE_H_

#include "memctl/kernel.h"

/*
 * vtable_for_class
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
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_ERROR			An error was encountered.
 * 	KEXT_NO_KEXT			No kext with the given bundle ID was found.
 * 	KEXT_NOT_FOUND			The vtable for the given class could not be found.
 *
 * Dependencies:
 * 	kernel subsystem
 */
kext_result vtable_for_class(const char *class_name, const char *bundle_id, kaddr_t *vtable,
		size_t *size);

/*
 * vtable_lookup
 *
 * Description:
 * 	Look up the class name corresponding to the given vtable address.
 *
 * Parameters:
 * 		vtable			The vtable address.
 * 	out	classname		On return, the name of the class. Deallocate with free()
 * 					when no longer needed.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_NO_KEXT			No kernel component contains the given address.
 * 	KEXT_NOT_FOUND			The address does not look like a vtable or the class name
 * 					could not be found.
 * 	KEXT_ERROR			Internal error.
 *
 * Dependencies:
 * 	kernel subsystem
 */
kext_result vtable_lookup(kaddr_t vtable, char **class_name);

#endif
