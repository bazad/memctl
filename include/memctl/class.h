#ifndef MEMCTL__CLASS_H_
#define MEMCTL__CLASS_H_

#include "memctl/kernel.h"

/*
 * macro VTABLE_OFFSET
 *
 * Description:
 * 	The offset between the vtable symbol and the actual vtable contents, in words.
 */
#define VTABLE_OFFSET	2

/*
 * macro VTABLE_OFFSET_SIZE
 *
 * Description:
 * 	 The offset between the vtable symbol and the actual vtable contents, in bytes.
 */
#define VTABLE_OFFSET_SIZE	(VTABLE_OFFSET * sizeof(kword_t))

/*
 * METACLASS_INSTANCE_NAME
 *
 * Description:
 * 	Each class in the kernel has an OSMetaClass instance describing it. This string is the name
 * 	of each of these instances.
 */
extern const char METACLASS_INSTANCE_NAME[];


/*
 * class_metaclass
 *
 * Description:
 * 	Find the metaclass instance for a given class name.
 *
 * Parameters:
 * 		class_name		The name of the C++ class.
 * 		bundle_id		The name of the kext containing the class. May be NULL.
 * 	out	metaclass		On return, the address of the metaclass instance.
 *
 * Returns:
 * 	KEXT_SUCCESS			Success.
 * 	KEXT_ERROR			An error was encountered.
 * 	KEXT_NO_KEXT			No kext with the given bundle ID was found.
 * 	KEXT_NOT_FOUND			The metaclass instance for the given class could not be
 * 					found.
 *
 * Dependencies:
 * 	kernel subsystem
 */
kext_result class_metaclass(const char *class_name, const char *bundle_id, kaddr_t *metaclass);

/*
 * class_size
 *
 * Description:
 * 	Find the size of a class from its metaclass instance.
 *
 * Parameters:
 * 	out	size			On return, the size of the corresponding class.
 * 		metaclass		The metaclass instance for the class.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * Dependencies:
 * 	kernel subsystem
 * 	kernel_call
 * 	class_init
 */
extern bool (*class_size)(size_t *size, kaddr_t metaclass);

/*
 * class_vtable
 *
 * Description:
 * 	Get the address and size of the vtable for a given class.
 *
 * Parameters:
 * 		class_name		The name of the C++ class.
 * 		bundle_id		The name of the kext containing the class. May be NULL.
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
kext_result class_vtable(const char *class_name, const char *bundle_id, kaddr_t *vtable,
		size_t *size);

/*
 * class_vtable_lookup
 *
 * Description:
 * 	Look up the class name corresponding to the given vtable address.
 *
 * Parameters:
 * 		vtable			The vtable address.
 * 	out	classname		On return, the name of the class. Deallocate with free()
 * 					when no longer needed.
 * 	out	offset			On return, the offset of the address from the start of the
 * 					vtable symbol. Note that the vtable symbol begins
 * 					VTABLE_OFFSET_SIZE bytes before the actual vtable contents.
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
kext_result class_vtable_lookup(kaddr_t vtable, char **class_name, size_t *offset);

/*
 * class_init
 *
 * Description:
 * 	Initialize the class subsystem (the indirect functions in this file).
 *
 * Dependencies:
 * 	kernel subsystem
 * 	kernel_call
 */
void class_init(void);

#endif
