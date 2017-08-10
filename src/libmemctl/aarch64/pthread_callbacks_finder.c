#include "aarch64/pthread_callbacks_finder.h"

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"

#include "memctl/aarch64/ksim.h"

// A struct to hold symbol information.
struct symbol {
	const char *name;
	kaddr_t     address;
};

// The list of symbols.
static struct symbol symbols[] = {
	{ "_proc_lock" },
	{ "_proc_unlock" },
	{ "_get_task_ipcspace" },
	{ "_ipc_port_copyout_send" },
	{ "_thread_exception_return" },
};

// The number of symbols.
static const size_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

// The indices of the above symbols in the pthread_callbacks structure.
static const unsigned symbol_index[symbol_count] = {
	19, 20, 32, 33, 38,
};

/*
 * extract_symbols
 *
 * Description:
 * 	Extract the useful function symbols from the pthread_callbacks structure.
 *
 * Notes:
 * 	Minimal validation is performed on the symbol values. If the pthread_callbacks structure
 * 	changes, then the offsets used in this function will be incorrect.
 */
static bool
extract_symbols(kaddr_t _pthread_callbacks) {
	const size_t head = sizeof(int) + sizeof(uint32_t);
	const int static_version = 1;
	const struct load_command *sc = macho_segment_containing_address(&kernel.macho,
			_pthread_callbacks);
	if (sc == NULL) {
		memctl_warning("invalid pthread_callbacks address 0x%llx: no kernel segment "
		               "contains this address", _pthread_callbacks);
		return false;
	}
	// Get the static pthread_callbacks data.
	const void *data;
	uint64_t address;
	size_t size;
	macho_segment_data(&kernel.macho, sc, &data, &address, &size);
	assert(address <= _pthread_callbacks && _pthread_callbacks < address + size);
	void *pthread_callbacks = (void *)((uintptr_t)data + (_pthread_callbacks - address));
	// Check the version.
	int version = *(int *)pthread_callbacks;
	if (version != static_version) {
		memctl_warning("unrecognized pthread_callbacks version %d", version);
		return false;
	}
	// Get the functions.
	kaddr_t *fns = (kword_t *)((uintptr_t)pthread_callbacks + head);
	for (size_t i = 0; i < symbol_count; i++) {
		symbols[i].address = fns[symbol_index[i]];
	}
	return true;
}

/*
 * find_symbol
 *
 * Description:
 * 	A symbol finder for the pthread_callbacks symbols.
 */
static kext_result
find_symbol(const struct kext *kext, const char *symbol, kaddr_t *addr, size_t *size) {
	assert(strcmp(kext->bundle_id, KERNEL_ID) == 0);
	for (size_t i = 0; i < symbol_count; i++) {
		if (strcmp(symbol, symbols[i].name) == 0) {
			if (symbols[i].address == 0) {
				break;
			}
			// We have no idea what the real size is, so don't set anything.
			*addr = symbols[i].address + kext->slide;
			return KEXT_SUCCESS;
		}
	}
	return KEXT_NOT_FOUND;
}

void
kernel_symbol_finder_init_pthread_callbacks() {
#define WARN(sym)	memctl_warning("could not find %s", #sym)
	error_stop();
	struct ksim sim;
	// Get the address of the pthread_kext_register function.
	kaddr_t _pthread_kext_register = ksim_symbol(NULL, "_pthread_kext_register");
	if (_pthread_kext_register == 0) {
		WARN(_pthread_kext_register);
		goto abort;
	}
	ksim_init_sim(&sim, _pthread_kext_register);
	// Get the address of the pthread_callbacks structure.
	kaddr_t _pthread_callbacks = 0;
	ksim_exec_until_store(&sim, NULL, AARCH64_X1, &_pthread_callbacks, 20);
	if (_pthread_callbacks == 0) {
		WARN(_pthread_callbacks);
		goto abort;
	}
	// Get the useful symbols.
	bool success = extract_symbols(_pthread_callbacks);
	if (!success) {
		goto abort;
	}
	// Add the symbol finder.
	kext_add_symbol_finder(KERNEL_ID, find_symbol);
abort:
	error_start();
}
