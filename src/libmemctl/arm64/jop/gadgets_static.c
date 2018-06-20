#include "arm64/jop/gadgets_static.h"

#include "memctl/kernel.h"
#include "memctl/kernel_slide.h"
#include "memctl/memctl_error.h"
#include "memctl/memctl_signal.h"

// NOTE: Keep this list synchronized with the enumeration in the header.
#define GADGET(str, ...)							\
	{ 0, str, sizeof((uint32_t[]) { __VA_ARGS__ }) / sizeof(uint32_t),	\
	  (const uint32_t *) &(const uint32_t[]) { __VA_ARGS__ } }
struct static_gadget static_gadgets[] = {
	GADGET("ldp x2, x1, [x1] ; br x2",      0xa9400422, 0xd61f0040),
	GADGET("mov x12, x2 ; br x3",           0xaa0203ec, 0xd61f0060),
	GADGET("mov x2, x30 ; br x12",          0xaa1e03e2, 0xd61f0180),
	GADGET("mov x8, x4 ; br x5",            0xaa0403e8, 0xd61f00a0),
	GADGET("mov x21, x2 ; br x8",           0xaa0203f5, 0xd61f0100),
	GADGET("mov x20, x0 ; blr x8",          0xaa0003f4, 0xd63f0100),
	GADGET("mov x10, x4 ; br x8",           0xaa0403ea, 0xd61f0100),
	GADGET("mov x9, x10 ; br x8",           0xaa0a03e9, 0xd61f0100),
	GADGET("mov x11, x9 ; br x8",           0xaa0903eb, 0xd61f0100),
	GADGET("ldp x3, x4, [x20, #0x20] ; ldp x5, x6, [x20, #0x30] ; blr x8",
	                                        0xa9421283, 0xa9431a85, 0xd63f0100),
	GADGET("add x20, x20, #0x34 ; br x8",   0x9100d294, 0xd61f0100),
	GADGET("mov x22, x6 ; blr x8",          0xaa0603f6, 0xd63f0100),
	GADGET("mov x24, x4 ; br x8",           0xaa0403f8, 0xd61f0100),
	GADGET("mov x0, x3 ; blr x8",           0xaa0303e0, 0xd63f0100),
	GADGET("mov x28, x0 ; blr x8",          0xaa0003fc, 0xd63f0100),
	GADGET("mov x12, x3 ; br x8",           0xaa0303ec, 0xd61f0100),
	GADGET("mov x0, x5 ; blr x8",           0xaa0503e0, 0xd63f0100),
	GADGET("mov x9, x0 ; br x11",           0xaa0003e9, 0xd61f0160),
	GADGET("mov x7, x9 ; blr x11",          0xaa0903e7, 0xd63f0160),
	GADGET("mov x11, x24 ; br x8",          0xaa1803eb, 0xd61f0100),
	GADGET("mov x1, x9 ; mov x2, x10 ; blr x11",
	                                        0xaa0903e1, 0xaa0a03e2, 0xd63f0160),
	GADGET("mov x30, x28 ; br x12",         0xaa1c03fe, 0xd61f0180),
	GADGET("ldp x8, x1, [x20, #0x10] ; blr x8",
	                                        0xa9410688, 0xd63f0100),
	GADGET("str x0, [x20] ; ldr x8, [x22] ; ldr x8, [x8, #0x28] ; mov x0, x22 ; blr x8",
	                                        0xf9000280, 0xf94002c8, 0xf9401508, 0xaa1603e0,
	                                        0xd63f0100),
	GADGET("mov x30, x21 ; br x8",          0xaa1503fe, 0xd61f0100),
	GADGET("ret",                           0xd65f03c0),
	GADGET("mov x28, x2 ; blr x8",          0xaa0203fc, 0xd63f0100),
	GADGET("mov x21, x5 ; blr x8",          0xaa0503f5, 0xd63f0100),
	GADGET("mov x15, x5 ; br x11",          0xaa0503ef, 0xd61f0160),
	GADGET("mov x17, x15 ; br x8",          0xaa0f03f1, 0xd61f0100),
	GADGET("mov x30, x22 ; br x17",         0xaa1603fe, 0xd61f0220),
	GADGET("str x0, [x20] ; ldr x8, [x21] ; ldr x8, [x8, #0x28] ; mov x0, x21 ; blr x8",
	                                        0xf9000280, 0xf94002a8, 0xf9401508, 0xaa1503e0,
	                                        0xd63f0100),
	GADGET("mov x30, x28 ; br x8",          0xaa1c03fe, 0xd61f0100),
	// GADGET_PROLOGUE_1
	GADGET("stp x28, x27, [sp, #-0x60]! ; "
	       "stp x26, x25, [sp, #0x10] ; "
	       "stp x24, x23, [sp, #0x20] ; "
	       "stp x22, x21, [sp, #0x30] ; "
	       "stp x20, x19, [sp, #0x40] ; "
	       "stp x29, x30, [sp, #0x50] ; "
	       "add x29, sp, #0x50 ; "
	       "sub sp, sp, #0x40 ; "
	       "mov x19, x0 ; "
	       "ldr x8, [x19] ; "
	       "ldr x8, [x8, #0x390] ; "
	       "blr x8",
	                                        0xa9ba6ffc, 0xa90167fa, 0xa9025ff8, 0xa90357f6,
	                                        0xa9044ff4, 0xa9057bfd, 0x910143fd, 0xd10103ff,
	                                        0xaa0003f3, 0xf9400268, 0xf941c908, 0xd63f0100),
	GADGET("mov x23, x0 ; blr x8",          0xaa0003f7, 0xd63f0100),
	// GADGET_INITIALIZE_X20_1
	GADGET("ldr x20, [x19, #0xc0] ; ldr x8, [x0] ; ldr x8, [x8, #0xa0] ; blr x8",
	                                        0xf9406274, 0xf9400008, 0xf9405108, 0xd63f0100),
	GADGET("mov x25, x0 ; blr x8",          0xaa0003f9, 0xd63f0100),
	// GADGET_POPULATE_1
	GADGET("ldp x2, x3, [x23] ; "
	       "ldp x4, x5, [x23, #0x10] ; "
	       "ldp x6, x7, [x23, #0x20] ; "
	       "ldp x9, x10, [x23, #0x30] ; "
	       "ldp x11, x12, [x23, #0x40] ; "
	       "stp x21, x22, [sp, #0x20] ; "
	       "stp x11, x12, [sp, #0x10] ; "
	       "stp x9, x10, [sp] ; "
	       "mov x0, x19 ; "
	       "mov x1, x20 ; "
	       "blr x8",
	                                        0xa9400ee2, 0xa94116e4, 0xa9421ee6, 0xa9432ae9,
	                                        0xa94432eb, 0xa9025bf5, 0xa90133eb, 0xa9002be9,
	                                        0xaa1303e0, 0xaa1403e1, 0xd63f0100),
	GADGET("mov x19, x9 ; br x8",           0xaa0903f3, 0xd61f0100),
	GADGET("mov x20, x12 ; br x8",          0xaa0c03f4, 0xd61f0100),
	GADGET("mov x8, x10 ; br x11",          0xaa0a03e8, 0xd61f0160),
	// GADGET_CALL_FUNCTION_1
	GADGET("blr x24 ; "
	       "mov x19, x0 ; "
	       "ldr x8, [x25] ; "
	       "ldr x8, [x8, #0xd0] ; "
	       "mov x0, x25 ; "
	       "blr x8",
	                                        0xd63f0300, 0xaa0003f3, 0xf9400328, 0xf9406908,
	                                        0xaa1903e0, 0xd63f0100),
	// GADGET_STORE_RESULT_1
	GADGET("str x19, [x0, #0x238] ; "
	       "ldr x0, [x0, #0x218] ; "
	       "ldr x8, [x0] ; "
	       "ldr x8, [x8, #0x140] ; "
	       "blr x8",
	                                        0xf9011c13, 0xf9410c00, 0xf9400008, 0xf940a108,
	                                        0xd63f0100),
	// GADGET_EPILOGUE_1
	GADGET("sub sp, x29, #0x50 ; "
	       "ldp x29, x30, [sp, #0x50] ; "
	       "ldp x20, x19, [sp, #0x40] ; "
	       "ldp x22, x21, [sp, #0x30] ; "
	       "ldp x24, x23, [sp, #0x20] ; "
	       "ldp x26, x25, [sp, #0x10] ; "
	       "ldp x28, x27, [sp], #0x60 ; "
	       "ret",
	                                        0xd10143bf, 0xa9457bfd, 0xa9444ff4, 0xa94357f6,
	                                        0xa9425ff8, 0xa94167fa, 0xa8c66ffc, 0xd65f03c0),
	// GADGET_PROLOGUE_2
	GADGET("sub sp, sp, #0xa0 ; "
	       "stp x28, x27, [sp, #0x40] ; "
	       "stp x26, x25, [sp, #0x50] ; "
	       "stp x24, x23, [sp, #0x60] ; "
	       "stp x22, x21, [sp, #0x70] ; "
	       "stp x20, x19, [sp, #0x80] ; "
	       "stp x29, x30, [sp, #0x90] ; "
	       "add x29, sp, #0x90 ; "
	       "mov x19, x0 ; "
	       "ldr x8, [x19] ; "
	       "ldr x8, [x8, #0x390] ; "
	       "blr x8",
	                                        0xd10283ff, 0xa9046ffc, 0xa90567fa, 0xa9065ff8,
	                                        0xa90757f6, 0xa9084ff4, 0xa9097bfd, 0x910243fd,
	                                        0xaa0003f3, 0xf9400268, 0xf941c908, 0xd63f0100),
	// MOV_X23_X19__BR_X8
	GADGET("mov x23, x19 ; br x8",          0xaa1303f7, 0xd61f0100),
	// MOV_X25_X19__BR_X8
	GADGET("mov x25, x19 ; br x8",          0xaa1303f9, 0xd61f0100),
	// GADGET_POPULATE_2
	GADGET("ldp x2, x3, [x23] ; "
	       "ldp x4, x5, [x23, #0x10] ; "
	       "ldp x6, x7, [x23, #0x20] ; "
	       "ldp x9, x10, [x23, #0x30] ; "
	       "ldp x11, x12, [x23, #0x40] ; "
	       "stp x22, x21, [sp, #0x20] ; "
	       "stp x11, x12, [sp, #0x10] ; "
	       "stp x9, x10, [sp] ; "
	       "mov x0, x19 ; "
	       "mov x1, x20 ; "
	       "blr x8 ; ",
	                                        0xa9400ee2, 0xa94116e4, 0xa9421ee6, 0xa9432ae9,
	                                        0xa94432eb, 0xa90257f6, 0xa90133eb, 0xa9002be9,
	                                        0xaa1303e0, 0xaa1403e1, 0xd63f0100),
	// MOV_X19_X3__BR_X8
	GADGET("mov x19, x3 ; br x8",           0xaa0303f3, 0xd61f0100),
	// MOV_X20_X6__BLR_X8
	GADGET("mov x20, x6 ; blr x8",          0xaa0603f4, 0xd63f0100),
	// MOV_X21_X4__BR_X8
	GADGET("mov x21, x4 ; br x8",           0xaa0403f5, 0xd61f0100),
	// MOV_X21_X4__BLR_X8
	GADGET("mov x21, x4 ; blr x8",          0xaa0403f5, 0xd63f0100),
	// MOV_X22_X12__BLR_X8
	GADGET("mov x22, x12 ; blr x8",         0xaa0c03f6, 0xd63f0100),
	// MOV_X21_X12__BR_X8
	GADGET("mov x21, x12 ; br x8",          0xaa0c03f5, 0xd61f0100),
	// MOV_X22_X21__BR_X8
	GADGET("mov x22, x21 ; br x8",          0xaa1503f6, 0xd61f0100),
	// MOV_X23_X5__BR_X8
	GADGET("mov x23, x5 ; br x8",           0xaa0503f7, 0xd61f0100),
	// MOV_X21_X5__BR_X8
	GADGET("mov x21, x5 ; br x8",           0xaa0503f5, 0xd61f0100),
	// MOV_X23_X21__BR_X8
	GADGET("mov x23, x21 ; br x8",          0xaa1503f7, 0xd61f0100),
	// MOV_X24_X7__BLR_X8
	GADGET("mov x24, x7 ; blr x8",          0xaa0703f8, 0xd63f0100),
	// MOV_X8_X9__BR_X10
	GADGET("mov x8, x9 ; br x10",           0xaa0903e8, 0xd61f0140),
	// GADGET_STORE_RESULT_2
	GADGET("str x19, [x0, #0x288] ; "
	       "ldr x0, [x0, #0x268] ; "
	       "ldr x8, [x0] ; "
	       "ldr x8, [x8, #0x158] ; "
	       "blr x8 ; ",
	                                        0xf9014413, 0xf9413400, 0xf9400008, 0xf940ad08,
	                                        0xd63f0100),
	// GADGET_EPILOGUE_2
	GADGET("ldp x29, x30, [sp, #0x90] ; "
	       "ldp x20, x19, [sp, #0x80] ; "
	       "ldp x22, x21, [sp, #0x70] ; "
	       "ldp x24, x23, [sp, #0x60] ; "
	       "ldp x26, x25, [sp, #0x50] ; "
	       "ldp x28, x27, [sp, #0x40] ; "
	       "add sp, sp, #0xa0 ; "
	       "ret ; ",
	                                        0xa9497bfd, 0xa9484ff4, 0xa94757f6, 0xa9465ff8,
	                                        0xa94567fa, 0xa9446ffc, 0x910283ff, 0xd65f03c0),
	//GADGET_POPULATE_3
	GADGET("ldp x2, x3, [x23] ; "
	       "ldp x4, x5, [x23, #0x10] ; "
	       "ldp x6, x7, [x23, #0x20] ; "
	       "ldr x9, [x23, #0x30] ; "
	       "ldur q0, [x23, #0x38] ; "
	       "ldr x10, [x23, #0x48] ; "
	       "stp x21, x22, [sp, #0x20] ; "
	       "str x10, [sp, #0x18] ; "
	       "stur q0, [sp, #8] ; "
	       "str x9, [sp] ; "
	       "mov x0, x19 ; "
	       "mov x1, x20 ; "
	       "blr x8",
	                                        0xa9400ee2, 0xa94116e4, 0xa9421ee6, 0xf9401ae9,
	                                        0x3cc382e0, 0xf94026ea, 0xa9025bf5, 0xf9000fea,
	                                        0x3c8083e0, 0xf90003e9, 0xaa1303e0, 0xaa1403e1,
	                                        0xd63f0100),
	//MOV_X19_X4__BR_X8
	GADGET("mov x19, x4 ; br x8",           0xaa0403f3, 0xd61f0100),
	//MOV_X20_X7__BR_X8
	GADGET("mov x20, x7 ; br x8",           0xaa0703f4, 0xd61f0100),
	//MOV_X23_X6__BLR_X8
	GADGET("mov x23, x6 ; blr x8",          0xaa0603f7, 0xd63f0100),
	//MOV_X24_X0__BLR_X8
	GADGET("mov x24, x0 ; blr x8",          0xaa0003f8, 0xd63f0100),
	//MOV_X8_X10__BR_X9
	GADGET("mov x8, x10 ; br x9",           0xaa0a03e8, 0xd61f0120),
};
#undef GADGET

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
	for (; ins < end && !interrupted; ins++) {
		struct static_gadget *g_end = static_gadgets + STATIC_GADGET_COUNT;
		for (struct static_gadget *g = static_gadgets; g < g_end; g++) {
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
		}
	}
}

bool
find_static_gadgets() {
	const struct load_command *lc = NULL;
	for (;;) {
		lc = macho_next_segment(&kernel.macho, lc);
		if (lc == NULL) {
			break;
		}
		const int prot = VM_PROT_READ | VM_PROT_EXECUTE;
		const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
		if ((sc->initprot & prot) != prot || (sc->maxprot & prot) != prot) {
			continue;
		}
		const void *data;
		uint64_t address;
		size_t size;
		macho_segment_data(&kernel.macho, lc, &data, &address, &size);
		find_gadgets_in_data(data, address, size);
		if (interrupted) {
			error_interrupt();
			return false;
		}
	}
	return true;
}
