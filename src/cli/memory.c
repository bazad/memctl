#include "memory.h"

#include "kernel_memory.h"

bool
read_memory(kaddr_t address, size_t *size, void *data, bool physical, size_t access) {
	if (physical) {
		*size = 0;
		error_internal("cannot read physical memory as of yet");
		return false;
	}
	kernel_io_result result = kernel_read_all(address, size, data, access, NULL);
	return (result == KERNEL_IO_SUCCESS);
}

bool
write_memory(kaddr_t address, size_t *size, const void *data, bool physical, size_t access) {
	if (physical) {
		*size = 0;
		error_internal("cannot write physical memory as of yet");
		return false;
	}
	kernel_io_result result = kernel_write_all(address, size, data, access, NULL);
	return (result == KERNEL_IO_SUCCESS);
}
