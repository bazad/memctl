#ifndef MEMCTL__AARCH64__JOP__GADGETS_STATIC_H_
#define MEMCTL__AARCH64__JOP__GADGETS_STATIC_H_
/*
 * Hard-coded JOP gadgets for ARM64.
 */

#include "memctl/memctl_types.h"

/*
 * struct static_gadget
 *
 * Description:
 * 	A structure describing a hard-coded gadget.
 */
struct static_gadget {
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
 * static_gadgets
 *
 * Description:
 * 	The list of hard-coded gadgets.
 */
extern struct static_gadget static_gadgets[];

/*
 * Named indices into the static_gadgets array.
 */
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
	MOV_X28_X2__BLR_X8,
	MOV_X21_X5__BLR_X8,
	MOV_X15_X5__BR_X11,
	MOV_X17_X15__BR_X8,
	MOV_X30_X22__BR_X17,
	STR_X0_X20__LDR_X8_X21__LDR_X8_X8_28__MOV_X0_X21__BLR_X8,

	MOV_X30_X28__BR_X8,
	GADGET_PROLOGUE_1,
	MOV_X23_X0__BLR_X8,
	GADGET_INITIALIZE_X20_1,
	MOV_X25_X0__BLR_X8,
	GADGET_POPULATE_1,
	MOV_X19_X9__BR_X8,
	MOV_X20_X12__BR_X8,

	MOV_X8_X10__BR_X11,
	GADGET_CALL_FUNCTION_1,
	GADGET_STORE_RESULT_1,
	GADGET_EPILOGUE_1,

	STATIC_GADGET_COUNT
};

/*
 * find_static_gadgets
 *
 * Description:
 * 	Find whichever gadgets are present in the kernel.
 *
 * Notes:
 * 	We only scan the executable segments of the kernel Mach-O because all executable segments
 * 	of all kernel extensions lie within the kernel's __PLK_TEXT_EXEC segment.
 */
bool find_static_gadgets(void);

#endif
