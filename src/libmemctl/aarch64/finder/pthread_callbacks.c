#include "aarch64/finder/pthread_callbacks.h"

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"

#include "memctl/aarch64/ksim.h"

// A struct to hold symbol information.
struct symbol {
	unsigned index;
	const char *name;
};

// The list of symbols.
static struct symbol symbols[] = {
	{ 19, "_proc_lock"               },
	{ 20, "_proc_unlock"             },
	{ 32, "_get_task_ipcspace"       },
	{ 33, "_ipc_port_copyout_send"   },
	{ 38, "_thread_exception_return" },
};

// The number of symbols.
static const size_t symbol_count = sizeof(symbols) / sizeof(symbols[0]);

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
extract_symbols(struct kext *kernel, kaddr_t _pthread_callbacks) {
	const size_t head = sizeof(int) + sizeof(uint32_t);
	const int static_version = 1;
	const struct load_command *sc = macho_segment_containing_address(&kernel->macho,
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
	macho_segment_data(&kernel->macho, sc, &data, &address, &size);
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
		kaddr_t address = fns[symbols[i].index];
		symbol_table_add_symbol(&kernel->symtab, symbols[i].name, address);
	}
	return true;
}

void
kernel_find_pthread_callbacks(struct kext *kernel) {
#define NOSYM(sym)	memctl_warning("could not find %s", #sym)
	// Get the address of the pthread_kext_register function.
	kaddr_t _pthread_kext_register = ksim_symbol(NULL, "_pthread_kext_register");
	if (_pthread_kext_register == 0) {
		NOSYM(_pthread_kext_register);
		return;
	}
	// Get the address of the pthread_callbacks structure.
	struct ksim sim;
	ksim_init_sim(&sim, _pthread_kext_register);
	kaddr_t _pthread_callbacks = 0;
	ksim_exec_until_store(&sim, NULL, AARCH64_X1, &_pthread_callbacks, 20);
	if (_pthread_callbacks == 0) {
		NOSYM(_pthread_callbacks);
		return;
	}
	symbol_table_add_symbol(&kernel->symtab, "_pthread_callbacks", _pthread_callbacks);
	// Get the useful symbols.
	extract_symbols(kernel, _pthread_callbacks);
}
