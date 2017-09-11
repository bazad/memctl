#ifndef MEMCTL__MANGLE_H_
#define MEMCTL__MANGLE_H_

#include <stdbool.h>
#include <stdlib.h>

/*
 * mangle_class_name
 *
 * Description:
 * 	Mangle the scoped class name.
 *
 * Parameters:
 * 	out	buf			On return, contains the mangled symbol.
 * 		size			The size of buf in bytes.
 * 		scoped_class_name	The name of the class, including enclosing scopes.
 * 		name_count		The number of entries in the scoped_class_name array, which
 * 					must be at least 1.
 *
 * Returns:
 * 	The full length of the mangled string, excluding the null terminator. If this is greater
 * 	than or equal to size, then the mangled string was truncated.
 */
size_t mangle_class_name(char *buf, size_t size, const char *scoped_class_name[],
		size_t name_count);

/*
 * mangle_class_vtable
 *
 * Description:
 * 	Mangle the vtable symbol corresponding to the scoped class name.
 *
 * Parameters:
 * 	out	buf			On return, contains the mangled symbol.
 * 		size			The size of buf in bytes.
 * 		scoped_class_name	The name of the class, including enclosing scopes.
 * 		name_count		The number of entries in the scoped_class_name array, which
 * 					must be at least 1.
 *
 * Returns:
 * 	The full length of the mangled string, excluding the null terminator. If this is greater
 * 	than or equal to size, then the mangled string was truncated.
 */
size_t mangle_class_vtable(char *buf, size_t size, const char *scoped_class_name[],
		size_t name_count);

/*
 * demangle_class_vtable
 *
 * Description:
 * 	Demangle a vtable symbol, recovering the scoped class name to which the vtable corresponds.
 *
 * Parameters:
 * 	out	scoped_class_name	On return, contains the scoped class name.
 * 		name_count		The capacity of scoped_name_count.
 * 	inout	mangled			On entry, the mangled vtable symbol. This buffer is
 * 					modified in place to generate the strings which are placed
 * 					in the scoped_class_name array.
 *
 * Returns:
 * 	The number of elements in the scoped_class_name_array. If this is greater than or equal to
 * 	name_count, then the scoped class name was truncated. If the symbol is not a valid vtable
 * 	symbol, 0 is returned.
 */
size_t demangle_class_vtable(char *scoped_class_name[], size_t name_count, char *mangled);

#endif
