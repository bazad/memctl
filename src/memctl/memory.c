#include "memory.h"

#include "memctl/kernel_memory.h"

bool
read_memory(kaddr_t address, size_t *size, void *data, bool physical, size_t access) {
	kernel_read_fn fn = kernel_read_all;
	if (physical) {
		fn = physical_read_unsafe;
	}
	return (fn(address, size, data, access, NULL) == KERNEL_IO_SUCCESS);
}

bool
write_memory(kaddr_t address, size_t *size, const void *data, bool physical, size_t access) {
	kernel_write_fn fn = kernel_write_all;
	if (physical) {
		fn = physical_write_unsafe;
	}
	return (fn(address, size, data, access, NULL) == KERNEL_IO_SUCCESS);
}
