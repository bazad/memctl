#include "memory.h"

#include "memctl/kernel_memory.h"

bool safe_memory;

bool
read_memory(kaddr_t address, size_t *size, void *data, bool physical, size_t access) {
	kernel_read_fn fn;
	if (physical) {
		fn = physical_read_unsafe;
	} else {
		fn = (safe_memory ? kernel_read_safe : kernel_read_all);
		if (fn == NULL) {
			fn = (safe_memory ? kernel_read_heap : kernel_read_unsafe);
		}
	}
	if (fn == NULL) {
		error_functionality_unavailable("cannot read kernel memory");
		return false;
	}
	return (fn(address, size, data, access, NULL) == KERNEL_IO_SUCCESS);
}

bool
write_memory(kaddr_t address, size_t *size, const void *data, bool physical, size_t access) {
	kernel_write_fn fn;
	if (physical) {
		fn = physical_write_unsafe;
	} else {
		fn = (safe_memory ? kernel_write_all : kernel_write_safe);
		if (fn == NULL) {
			fn = (safe_memory ? kernel_write_heap : kernel_write_unsafe);
		}
	}
	if (fn == NULL) {
		error_functionality_unavailable("cannot write kernel memory");
		return false;
	}
	return (fn(address, size, data, access, NULL) == KERNEL_IO_SUCCESS);
}
