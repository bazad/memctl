#include "memory.h"

#include "kernel_memory.h"

bool
read_memory(kaddr_t address, size_t *size, bool physical, size_t access, void *data) {
	if (physical) {
		*size = 0;
		error_internal("cannot read physical memory as of yet");
		return false;
	}
	kernel_io_result result = kernel_read_unsafe(address, size, data, access, NULL);
	return (result == KERNEL_IO_SUCCESS);
}
