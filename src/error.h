#ifndef MEMCTL__ERROR_H_
#define MEMCTL__ERROR_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * error_type_t
 *
 * Description:
 * 	An error type identifier.
 */
typedef uint64_t error_type_t;

/*
 * struct error
 *
 * Description:
 * 	An error object, which consists of a type code and associated data.
 */
struct error {
	error_type_t type;
	void *       data;
};

/*
 * error_handle
 *
 * Description:
 * 	A handle to an error object.
 */
typedef const struct error *error_handle;

/*
 * error_init
 *
 * Description:
 * 	Initialize the global error system.
 *
 * Returns:
 * 	true if the error system was successfully initialized.
 */
bool error_init(void);

/*
 * error_free
 *
 * Description:
 * 	Clean up internal state.
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
 * error_push_data
 *
 * Description:
 * 	Push an error onto the error stack and return space to store any associated error data.
 *
 * Parameters:
 * 	type				The error type code
 * 	size				The size of any associated data
 * 	destroy				A function to be called when the associated data is no
 * 					longer needed to free any resources. Specify NULL if no
 * 					cleanup is needed.
 *
 * Returns:
 * 	NULL				Out of memory
 * 	NULL				Errors have been stopped with error_stop
 * 	Otherwise, a pointer to a region of memory capable of storing size bytes is returned.
 * 	The memory is initialized to 0.
 */
void *error_push_data(error_type_t type, size_t size, void (*destroy)(void *));

/*
 * error_push_printf
 *
 * Description:
 * 	Push an error onto the stack and store as its associated data a formatted message.
 *
 * Parameters:
 * 	type				The error type code
 * 	format				A printf-style format string
 * 	ap				The variadic arguments list
 *
 * Returns:
 * 	true if the error was pushed onto the stack.
 */
bool error_push_printf(error_type_t type, const char *format, va_list ap);

/*
 * error_push
 *
 * Description:
 * 	A convenience function to push an error type code with no associated data.
 *
 * Parameters:
 * 	type				The error type code
 *
 * Returns:
 * 	true if the error was pushed onto the stack.
 */
static inline bool
error_push(error_type_t type) {
	return error_push_data(type, 0, NULL) != NULL;
}

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
 * 	index				The index of the error to retrieve
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

enum {
	/*
	 * out_of_memory_error
	 *
	 * Description:
	 * 	The type code for an out-of-memory error.
	 */
	out_of_memory_error = 1,
};

/*
 * error_push_out_of_memory
 *
 * Description:
 * 	Push an out-of-memory error in the system domain onto the error stack.
 *
 * Returns:
 * 	true if the error was pushed onto the stack.
 */
bool error_push_out_of_memory(void);

#endif
