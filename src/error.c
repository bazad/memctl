#include "error.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_DATA_SIZE	(1024 * 1024)
#define NO_DATA		((void *)-1)

/*
 * struct header
 *
 * Description:
 * 	The data pointer of each error_handle has a header just before it in memory. The header
 * 	stores a function pointer that should be called to destroy the error data.
 */
struct header {
	void (*destroy)(void *);
	/* ... data ... */
};

/*
 * struct error_stack
 *
 * Description:
 * 	A stack of errors. The earliest error is at index 0.
 */
struct error_stack {
	struct error *stack;
	size_t capacity;
	size_t count;
	size_t stop_count;
};

/*
 * errors
 *
 * Description:
 * 	A global error stack.
 */
static struct error_stack errors;

/*
 * reserve
 *
 * Description:
 * 	Reserve space for at least 3 items on the error stack.
 *
 * Returns:
 * 		true			Success
 * 		false			Out of memory
 *
 * Notes:
 * 	We maintain the invariant that the error stack always has space for at least 2 items on
 * 	it. Presumably, reserve is being called because a new item is about to be added, so we
 * 	reserve space for 3 items in order to maintain the invariant.
 *
 * 	If space cannot be allocated, no state is changed.
 */
static bool
reserve() {
	assert(errors.count + 2 <= errors.capacity || errors.capacity == 0);
	if (errors.count + 3 <= errors.capacity) {
		return true;
	}
	size_t new_capacity = errors.capacity + 4;
	struct error *new_stack = realloc(errors.stack,
			new_capacity * sizeof(*errors.stack));
	if (new_stack == NULL) {
		return false;
	}
	errors.stack = new_stack;
	errors.capacity = new_capacity;
	return true;
}

/*
 * alloc_data
 *
 * Description:
 * 	Allocate space for error data. This data can be destroyed and freed with a call to
 * 	free_data.
 *
 * Parameters:
 * 		size			The amount of space to reserve for the error data.
 * 		destroy			A function to be invoked when the error data is to be
 * 					freed.
 * Returns:
 * 	NULL				Out of memory or size too large
 * 	NO_DATA				size is 0
 * 	data_ptr			A pointer to a block of zeroed memory of the required
 * 					size.
 */
static void *
alloc_data(size_t size, void (*destroy)(void *)) {
	if (size == 0) {
		return NO_DATA;
	}
	if (size > MAX_DATA_SIZE) {
		return NULL;
	}
	struct header *header = malloc(sizeof(*header) + size);
	if (header == NULL) {
		return NULL;
	}
	header->destroy = destroy;
	void *extra = header + 1;
	memset(extra, 0, size);
	return extra;
}

/*
 * free_data
 *
 * Description:
 * 	Destroy and free the error data allocated with alloc_data.
 *
 * Parameters:
 * 	data				The error data allocated with alloc_data.
 */
static void
free_data(void *data) {
	if (data != NO_DATA) {
		struct header *header = (struct header *)data - 1;
		if (header->destroy != NULL) {
			header->destroy(data);
		}
		free(header);
	}
}

/*
 * push_internal
 *
 * Description:
 * 	Push an error onto the stack.
 */
static void *
push_internal(error_type_t type, size_t size, void (*destroy)(void *)) {
	void *data = alloc_data(size, destroy);
	if (data == NULL) {
		return NULL;
	}
	struct error *error = &errors.stack[errors.count];
	error->type = type;
	error->data = data;
	++errors.count;
	return data;
}

/*
 * pop_internal
 *
 * Description:
 * 	Pop the most recent error from the top of the error stack.
 */
static void
pop_internal() {
	--errors.count;
	free_data((void *)errors.stack[errors.count].data);
}

/*
 * error_push_out_of_memory_internal
 *
 * Description:
 * 	Push an out-of-memory error onto the error stack.
 *
 * Returns:
 * 	true				The error was successfully pushed.
 * 	false				There was no space to push an error.
 */
static bool
error_push_out_of_memory_internal() {
	if (errors.count >= errors.capacity) {
		return false;
	}
	push_internal(out_of_memory_error, 0, NULL);
	return true;
}

bool
error_init() {
	errors.stack      = NULL;
	errors.count      = 0;
	errors.capacity   = 0;
	errors.stop_count = 0;
	return reserve();
}

void
error_free() {
	error_clear();
	free(errors.stack);
}

void
error_stop() {
	++errors.stop_count;
}

void
error_start() {
	assert(errors.stop_count > 0);
	--errors.stop_count;
}

void *
error_push_data(error_type_t type, size_t size, void (*destroy)(void *)) {
	if (errors.stop_count > 0) {
		return NULL;
	}
	if (!reserve()) {
		goto fail;
	}
	void *data = push_internal(type, size, destroy);
	if (data == NULL) {
		goto fail;
	}
	return data;
fail:
	error_push_out_of_memory_internal();
	return NULL;
}

bool
error_push_printf(error_type_t type, const char *format, va_list ap) {
	va_list ap2;
	va_copy(ap2, ap);
	int size = vsnprintf(NULL, 0, format, ap2);
	va_end(ap2);
	if (size < 0) {
		return false;
	}
	++size;
	char *buf = error_push_data(type, size, NULL);
	if (buf == NULL) {
		return false;
	}
	size = vsnprintf(buf, size, format, ap);
	return size >= 0;
}

void
error_pop() {
	if (errors.count > 0) {
		pop_internal();
	}
}

error_handle
error_first() {
	return error_at_index(0);
}

error_handle
error_last() {
	return error_at_index(errors.count - 1);
}

error_handle
error_at_index(size_t index) {
	if (index >= errors.count) {
		return NULL;
	}
	return &errors.stack[index];
}

size_t
error_count() {
	return errors.count;
}

void
error_clear() {
	while (errors.count > 0) {
		pop_internal();
	}
}

bool
error_push_out_of_memory() {
	if (errors.stop_count > 0) {
		return false;
	}
	reserve();
	return error_push_out_of_memory_internal();
}
