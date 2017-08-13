#include "memory.h"

#include "initialize.h"

#include "memctl/kernel_memory.h"

bool safe_memory;

// A generic function pointer type.
typedef void (*fn_t)(void);

// A helper to select the right function implementation.
static fn_t
select_fn(const char *io, memflags flags,
		fn_t *kernel_unsafe, fn_t *kernel_heap, fn_t *kernel_safe, fn_t *kernel_all,
		fn_t *physical_unsafe) {
#define INIT(features)				\
	if (!initialize(features)) {		\
		return NULL;			\
	}
	fn_t fn;
	bool force    = flags & MEM_FORCE;
	bool physical = flags & MEM_PHYS;
	if (physical) {
		INIT(KERNEL_MEMORY);
		fn = *physical_unsafe;
	} else {
		if (force) {
			INIT(KERNEL_MEMORY_BASIC);
			fn = *kernel_unsafe;
		} else {
			INIT(KERNEL_MEMORY);
			fn = *(safe_memory ? kernel_safe : kernel_all);
			if (fn == NULL) {
				fn = *(safe_memory ? kernel_heap : kernel_unsafe);
			}
		}
	}
	if (fn == NULL) {
		error_functionality_unavailable("cannot %s %s memory", io,
				(physical ? "physical" : "kernel"));
	}
	return fn;
}

#define SELECT_FN(io)								\
	kernel_##io##_fn io = (kernel_##io##_fn)select_fn(#io, flags,		\
			(fn_t *)&kernel_##io##_unsafe,				\
			(fn_t *)&kernel_##io##_heap,				\
			(fn_t *)&kernel_##io##_safe,				\
			(fn_t *)&kernel_##io##_all,				\
			(fn_t *)&physical_##io##_unsafe);			\
	if (io == NULL) {							\
		*size = 0;							\
		return false;							\
	}

bool
read_kernel(kaddr_t address, size_t *size, void *data, memflags flags, size_t access) {
	SELECT_FN(read);
	return (read(address, size, data, access, NULL) == KERNEL_IO_SUCCESS);
}

bool
write_kernel(kaddr_t address, size_t *size, const void *data, memflags flags, size_t access) {
	SELECT_FN(write);
	return (write(address, size, data, access, NULL) == KERNEL_IO_SUCCESS);
}
