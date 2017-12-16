#include "memctl/error.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MAX_DATA_SIZE	(1024 * 1024)
#define NO_DATA		((void *)-1)

/*
 * struct error_stack
 *
 * Description:
 * 	A stack of errors. The earliest error is at index 0.
 */
struct error_stack {
	struct error *stack;
	unsigned capacity;
	unsigned count;
	unsigned stop_count;
};

/*
 * errors
 *
 * Description:
 * 	A thread-local error stack.
 */
_Thread_local static struct error_stack errors;

/*
 * reserve
 *
 * Description:
 * 	Reserve space for at least 3 items on the error stack.
 *
 * Notes:
 * 	We maintain the invariant that the error stack always has space for at least 1 item on
 * 	it. Presumably, reserve is being called because a new item is about to be added, so we
 * 	reserve space for 2 items in order to maintain the invariant.
 *
 * 	If space cannot be allocated, no state is changed.
 */
static void
reserve() {
	assert(errors.count + 1 <= errors.capacity || errors.capacity == 0);
	if (errors.count + 2 <= errors.capacity) {
		return;
	}
	unsigned new_capacity = errors.capacity + 4;
	assert(new_capacity > errors.capacity);
	struct error *new_stack = realloc(errors.stack, new_capacity * sizeof(*errors.stack));
	assert(new_stack);
	errors.stack = new_stack;
	errors.capacity = new_capacity;
}

/*
 * alloc_data
 *
 * Description:
 * 	Allocate space for error data. This data can be destroyed and freed with a call to
 * 	free_data.
 *
 * Parameters:
 * 	size				The amount of space to reserve for the error data.
 *
 * Returns:
 * 	NULL				Out of memory or size too large
 * 	NO_DATA				size is 0
 * 	data_ptr			A pointer to a block of zeroed memory of the required
 * 					size.
 */
static void *
alloc_data(size_t size) {
	if (size == 0) {
		return NO_DATA;
	}
	if (size > MAX_DATA_SIZE) {
		return NULL;
	}
	return calloc(1, size);
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
free_data(struct error *error) {
	if (error->data != NO_DATA) {
		if (error->type->destroy_error_data != NULL) {
			error->type->destroy_error_data(error->data, error->size);
		}
		free(error->data);
		error->size = 0;
	}
}

/*
 * push_internal
 *
 * Description:
 * 	Push an error onto the stack and return a pointer to the data.
 */
static void *
push_internal(const struct error_type *type, size_t size) {
	assert(errors.count < errors.capacity);
	void *data = alloc_data(size);
	if (data == NULL) {
		return NULL;
	}
	struct error *error = &errors.stack[errors.count];
	error->type = type;
	error->data = data;
	error->size = size;
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
	free_data(&errors.stack[errors.count]);
}

void
error_init() {
	errors.stack      = NULL;
	errors.count      = 0;
	errors.capacity   = 0;
	errors.stop_count = 0;
	reserve();
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
error_push(const struct error_type *type, size_t size) {
	if (errors.stop_count > 0) {
		return NULL;
	}
	reserve();
	return push_internal(type, size);
}

bool
error_push_printf(const struct error_type *type, const char *format, va_list ap) {
	if (errors.stop_count > 0) {
		return false;
	}
	va_list ap2;
	va_copy(ap2, ap);
	int size = vsnprintf(NULL, 0, format, ap2);
	va_end(ap2);
	if (size < 0) {
		return false;
	}
	size += 1;
	char *buf = error_push(type, size);
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
