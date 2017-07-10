#include "aarch64/pthread_callbacks_finder.h"

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"

#include "memctl/aarch64/disasm.h"
#include "memctl/aarch64/ksim.h"

static kaddr_t _get_task_ipcspace;
static kaddr_t _ipc_port_copyout_send;
static kaddr_t _thread_exception_return;

/*
 * find__pthread_kext_register
 *
 * Description:
 * 	Find the _pthread_kext_register function.
 */
static kaddr_t
find__pthread_kext_register() {
	kaddr_t _pthread_kext_register;
	kext_result kr = kernel_symbol("_pthread_kext_register", &_pthread_kext_register, NULL);
	// Subtract out the slide, since we're simulating using the static addresses.
	return (kr == KEXT_SUCCESS ? _pthread_kext_register - kernel.slide : 0);
}

#define MAX__pthread_kext_register_INSTRUCTIONS 20

/*
 * get_str_x1
 *
 * Description:
 * 	Stop at any store to x1, saving the stored value in the ksim context.
 */
static bool
get_str_x1(struct ksim *ksim, uint32_t ins) {
	struct aarch64_ins_ldr_im str;
	if (!aarch64_decode_ldr_ui(ins, &str) || str.load || str.size != 3 || str.wb || str.post
			|| str.Xn != AARCH64_X1) {
		return false;
	}
	uint64_t value;
	if (ksim_reg(ksim, str.Rt, &value)) {
		*(kaddr_t *)ksim->context = value;
	}
	return true;
}

/*
 * find__pthread_callbacks
 *
 * Description:
 * 	Find the _pthread_callbacks structure.
 */
static kaddr_t
find__pthread_callbacks(kaddr_t _pthread_kext_register) {
	struct ksim ksim;
	kaddr_t _pthread_callbacks = 0;
	if (!ksim_init_kext(&ksim, &kernel, _pthread_kext_register)) {
		return 0;
	}
	ksim.context = &_pthread_callbacks;
	ksim.max_instruction_count = MAX__pthread_kext_register_INSTRUCTIONS;
	ksim.stop_before = get_str_x1;
	if (!ksim_run(&ksim)) {
		return 0;
	}
	return _pthread_callbacks;
}

/*
 * get_functions
 *
 * Description:
 * 	Get the useful functions from the pthread_callbacks structure.
 *
 * Notes:
 * 	No validation is performed on the symbol values. If the pthread_callbacks structure
 * 	changes, then the offsets used in this function will be incorrect.
 */
void
get_functions(kaddr_t _pthread_callbacks) {
	const size_t head = sizeof(int) + sizeof(uint32_t);
	const size_t task_get_ipcspace_idx       = 32;
	const size_t ipc_port_copyout_send_idx   = 33;
	const size_t thread_exception_return_idx = 38;
	const struct load_command *sc = macho_segment_containing_address(&kernel.macho,
			_pthread_callbacks);
	assert(sc != NULL);
	const void *data;
	uint64_t address;
	size_t size;
	macho_segment_data(&kernel.macho, sc, &data, &address, &size);
	kaddr_t *fns = (kword_t *)((uintptr_t)data + (_pthread_callbacks - address) + head);
	_get_task_ipcspace       = fns[task_get_ipcspace_idx];
	_ipc_port_copyout_send   = fns[ipc_port_copyout_send_idx];
	_thread_exception_return = fns[thread_exception_return_idx];
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
	kaddr_t static_addr;
	if (strcmp(symbol, "_get_task_ipcspace") == 0) {
		static_addr = _get_task_ipcspace;
	} else if (strcmp(symbol, "_ipc_port_copyout_send") == 0) {
		static_addr = _ipc_port_copyout_send;
	} else if (strcmp(symbol, "_thread_exception_return") == 0) {
		static_addr = _thread_exception_return;
	} else {
		return KEXT_NOT_FOUND;
	}
	*addr = static_addr + kext->slide;
	// We have no idea what the real size is, so don't set anything.
	return KEXT_SUCCESS;
}

void
kernel_symbol_finder_init_pthread_callbacks(void) {
#define WARN(sym)	memctl_warning("could not find %s", #sym)
	error_stop();
	// Get the address of the pthread_kext_register function.
	kaddr_t _pthread_kext_register = find__pthread_kext_register();
	if (_pthread_kext_register == 0) {
		WARN(_pthread_kext_register);
		goto abort;
	}
	// Get the address of the pthread_callbacks structure.
	kaddr_t _pthread_callbacks = find__pthread_callbacks(_pthread_kext_register);
	if (_pthread_callbacks == 0) {
		WARN(_pthread_callbacks);
		goto abort;
	}
	// Get the useful callbacks.
	get_functions(_pthread_callbacks);
	// Add the symbol finder.
	kext_add_symbol_finder(KERNEL_ID, find_symbol);
abort:
	error_start();
}
