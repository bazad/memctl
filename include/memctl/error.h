#ifndef MEMCTL__ERROR_H_
#define MEMCTL__ERROR_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * struct error
 *
 * Description:
 * 	An error object, which consists of a type code and associated data.
 */
struct error {
	const struct error_type * type;
	void *                    data;
	size_t                    size;
};

/*
 * error_handle
 *
 * Description:
 * 	A handle to an error object.
 */
typedef const struct error *error_handle;

/*
 * struct error_type
 *
 * Description:
 * 	An error type object, encapsulating metainformation about a class of errors.
 */
struct error_type {
	// A static string describing the error category.
	const char *static_description;
	// A snprintf-like function to format an error description into a buffer.
	size_t (*format_description)(char *buffer, size_t size, error_handle error);
	// A function to destroy any error-specific data.
	void (*destroy_error_data)(void *data, size_t size);
};

/*
 * error_init
 *
 * Description:
 * 	Initialize the thread-local error system.
 *
 * Notes:
 * 	Calling this function at thread start is optional.
 */
void error_init(void);

/*
 * error_free
 *
 * Description:
 * 	Free all resources used by the thread-local error system.
 *
 * Notes:
 * 	This function must be called before a thread exits. Failing to do so will leak memory.
 *
 * 	After this call, the error stack must be re-initialized with error_init before it can be
 * 	used again.
 */
void error_free(void);

/*
 * error_stop
 *
 * Description:
 * 	Stop error_push functions from pushing new errors onto the error stack until a matching
 * 	call to error_start.
 */
void error_stop(void);

/*
 * error_start
 *
 * Description:
 * 	Allow the error_push functions to push errors onto the stack again.
 */
void error_start(void);

/*
 * error_push
 *
 * Description:
 * 	Push an error onto the error stack and return space to store any associated error data.
 *
 * Parameters:
 * 	type				The error_type struct representing the type of this error.
 * 	size				The size of any associated data.
 * 	destroy				A function to be called when the associated data is no
 * 					longer needed to free any resources. Specify NULL if no
 * 					cleanup is needed.
 *
 * Returns:
 * 	NULL				Out of memory
 * 	NULL				Errors have been stopped with error_stop
 *
 * 	Otherwise, a pointer to a region of memory capable of storing size bytes is returned.
 * 	The memory is initialized to 0. This is the same value that would be obtained from
 * 	error_last()->data.
 */
void *error_push(const struct error_type *type, size_t size);

/*
 * error_push_printf
 *
 * Description:
 * 	Push an error onto the stack and store as its associated data a formatted message.
 *
 * Parameters:
 * 	type				The error_type struct representing the type of this error.
 * 	format				A printf-style format string.
 * 	ap				The variadic arguments list.
 *
 * Returns:
 * 	True if the error was pushed onto the stack.
 */
bool error_push_printf(const struct error_type *type, const char *format, va_list ap);

/*
 * error_pop
 *
 * Description:
 * 	Pop the most recent error off the error stack.
 */
void error_pop(void);

/*
 * error_first
 *
 * Description:
 * 	Get the error handle of the earliest error on the stack.
 */
error_handle error_first(void);

/*
 * error_last
 *
 * Description:
 * 	Get the error handle of the most recent error pushed onto the stack.
 */
error_handle error_last(void);

/*
 * error_at_index
 *
 * Description:
 * 	Get the error handle at the specified index in the error stack. Index 0 corresponds to
 * 	the earliest error in the stack.
 *
 * Parameters:
 * 	index				The index of the error to retrieve.
 */
error_handle error_at_index(size_t index);

/*
 * error_count
 *
 * Description:
 * 	Get the number of errors in the error stack.
 */
size_t error_count(void);

/*
 * error_clear
 *
 * Description:
 * 	Clear the error stack.
 */
void error_clear(void);

#endif
