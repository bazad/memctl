#ifndef MEMCTL__AARCH64__DISASM__H_
#define MEMCTL__AARCH64__DISASM__H_

// Source:
//     ARM Architecture Reference Manual
//     ARMv8, for ARMv8-A architecture profile
//     https://static.docs.arm.com/ddi0487/a/DDI0487A_k_armv8_arm.pdf

#include <stdbool.h>
#include <stdint.h>

/*
 * AARCH64_ZR_INS
 *
 * Description:
 * 	When this flag is set in an aarch64_reg, it indicates that the register should be
 * 	interpreted as ZR rather than SP in the context of the decoded instruction.
 */
#define AARCH64_ZR_INS 0x80

/*
 * aarch64_reg
 *
 * Description:
 * 	An AArch64 register identifier.
 */
typedef uint8_t aarch64_reg;

enum {
	AARCH64_R0  =                        0,
	AARCH64_R1  =                        1,
	AARCH64_R2  =                        2,
	AARCH64_R3  =                        3,
	AARCH64_R4  =                        4,
	AARCH64_R5  =                        5,
	AARCH64_R6  =                        6,
	AARCH64_R7  =                        7,
	AARCH64_R8  =                        8,
	AARCH64_R9  =                        9,
	AARCH64_R10 =                       10,
	AARCH64_R11 =                       11,
	AARCH64_R12 =                       12,
	AARCH64_R13 =                       13,
	AARCH64_R14 =                       14,
	AARCH64_R15 =                       15,
	AARCH64_R16 =                       16,
	AARCH64_R17 =                       17,
	AARCH64_R18 =                       18,
	AARCH64_R19 =                       19,
	AARCH64_R20 =                       20,
	AARCH64_R21 =                       21,
	AARCH64_R22 =                       22,
	AARCH64_R23 =                       23,
	AARCH64_R24 =                       24,
	AARCH64_R25 =                       25,
	AARCH64_R26 =                       26,
	AARCH64_R27 =                       27,
	AARCH64_R28 =                       28,
	AARCH64_R29 =                       29,
	AARCH64_R30 =                       30,
	AARCH64_RZR = AARCH64_ZR_INS |      31,
	AARCH64_RSP =                       31,
	AARCH64_W0  =                  32 |  0,
	AARCH64_W1  =                  32 |  1,
	AARCH64_W2  =                  32 |  2,
	AARCH64_W3  =                  32 |  3,
	AARCH64_W4  =                  32 |  4,
	AARCH64_W5  =                  32 |  5,
	AARCH64_W6  =                  32 |  6,
	AARCH64_W7  =                  32 |  7,
	AARCH64_W8  =                  32 |  8,
	AARCH64_W9  =                  32 |  9,
	AARCH64_W10 =                  32 | 10,
	AARCH64_W11 =                  32 | 11,
	AARCH64_W12 =                  32 | 12,
	AARCH64_W13 =                  32 | 13,
	AARCH64_W14 =                  32 | 14,
	AARCH64_W15 =                  32 | 15,
	AARCH64_W16 =                  32 | 16,
	AARCH64_W17 =                  32 | 17,
	AARCH64_W18 =                  32 | 18,
	AARCH64_W19 =                  32 | 19,
	AARCH64_W20 =                  32 | 20,
	AARCH64_W21 =                  32 | 21,
	AARCH64_W22 =                  32 | 22,
	AARCH64_W23 =                  32 | 23,
	AARCH64_W24 =                  32 | 24,
	AARCH64_W25 =                  32 | 25,
	AARCH64_W26 =                  32 | 26,
	AARCH64_W27 =                  32 | 27,
	AARCH64_W28 =                  32 | 28,
	AARCH64_W29 =                  32 | 29,
	AARCH64_W30 =                  32 | 30,
	AARCH64_WZR = AARCH64_ZR_INS | 32 | 31,
	AARCH64_WSP =                  32 | 31,
	AARCH64_X0  =                  64 |  0,
	AARCH64_X1  =                  64 |  1,
	AARCH64_X2  =                  64 |  2,
	AARCH64_X3  =                  64 |  3,
	AARCH64_X4  =                  64 |  4,
	AARCH64_X5  =                  64 |  5,
	AARCH64_X6  =                  64 |  6,
	AARCH64_X7  =                  64 |  7,
	AARCH64_X8  =                  64 |  8,
	AARCH64_X9  =                  64 |  9,
	AARCH64_X10 =                  64 | 10,
	AARCH64_X11 =                  64 | 11,
	AARCH64_X12 =                  64 | 12,
	AARCH64_X13 =                  64 | 13,
	AARCH64_X14 =                  64 | 14,
	AARCH64_X15 =                  64 | 15,
	AARCH64_X16 =                  64 | 16,
	AARCH64_X17 =                  64 | 17,
	AARCH64_X18 =                  64 | 18,
	AARCH64_X19 =                  64 | 19,
	AARCH64_X20 =                  64 | 20,
	AARCH64_X21 =                  64 | 21,
	AARCH64_X22 =                  64 | 22,
	AARCH64_X23 =                  64 | 23,
	AARCH64_X24 =                  64 | 24,
	AARCH64_X25 =                  64 | 25,
	AARCH64_X26 =                  64 | 26,
	AARCH64_X27 =                  64 | 27,
	AARCH64_X28 =                  64 | 28,
	AARCH64_X29 =                  64 | 29,
	AARCH64_X30 =                  64 | 30,
	AARCH64_XZR = AARCH64_ZR_INS | 64 | 31,
	AARCH64_SP  =                  64 | 31,
};

/*
 * macro AARCH64_REGSIZE
 *
 * Description:
 * 	Return the width of the given register.
 */
#define AARCH64_REGSIZE(reg)	((reg) & (64 | 32))

/*
 * macro AARCH64_REGNAME
 *
 * Description:
 * 	Return the name of the given register.
 */
#define AARCH64_REGNAME(reg)	((reg) & 0x1f)

/*
 * macro AARCH64_REGZR
 *
 * Description:
 * 	Return nonozero if the given register should be interpreted as ZR in the context of the
 * 	decoded instruction.
 */
#define AARCH64_REGZR(reg)	((reg) & AARCH64_ZR_INS)

/*
 * aarch64_shift
 *
 * Description:
 * 	The type of shift to apply.
 */
typedef uint8_t aarch64_shift;

enum {
	AARCH64_SHIFT_LSL = 0,
	AARCH64_SHIFT_LSR = 1,
	AARCH64_SHIFT_ASR = 2,
	AARCH64_SHIFT_ROR = 3,
};

/*
 * aarch64_extend
 *
 * Description:
 * 	The type of extension to apply.
 */
typedef uint8_t aarch64_extend;

enum {
	AARCH64_EXTEND_UXTB = 0,
	AARCH64_EXTEND_UXTH = 1,
	AARCH64_EXTEND_UXTW = 2,
	AARCH64_EXTEND_UXTX = 3,
	AARCH64_EXTEND_SXTB = 4,
	AARCH64_EXTEND_SXTH = 5,
	AARCH64_EXTEND_SXTW = 6,
	AARCH64_EXTEND_SXTX = 7,
	AARCH64_EXTEND_LSL  = 8,
};

/*
 * macro AARCH64_EXTEND_TYPE
 *
 * Description:
 * 	Returns the type of extension.
 */
#define AARCH64_EXTEND_TYPE(ext)	((ext) & 0x7)

/*
 * macro AARCH64_EXTEND_IS_LSL
 *
 * Description:
 * 	Returns whether the extension should be formatted as LSL in assembly.
 */
#define AARCH64_EXTEND_IS_LSL(ext)	((ext) & AARCH64_EXTEND_LSL)

/*
 * macro AARCH64_INS_TYPE
 *
 * Description:
 * 	Test if the AArch64 instruction is of the specified type.
 */
#define AARCH64_INS_TYPE(ins, type) \
	(((ins) & AARCH64_##type##_MASK) == AARCH64_##type##_BITS)


// ---- ADC, ADCS, SBC, SBCS ----
// ---- NGC, NGCS ----

#define AARCH64_ADC_MASK 0x1fe0fc00
#define AARCH64_ADC_BITS 0x1a000000

struct aarch64_ins_adc {
	enum {
		AARCH64_INS_ADC_OP_ADC = 0,
		AARCH64_INS_ADC_OP_SBC = 1,
	}           op;
	bool        setflags;
	aarch64_reg Rd;
	aarch64_reg Rn;
	aarch64_reg Rm;
};

bool aarch64_decode_adc(uint32_t ins, struct aarch64_ins_adc *adc);

// ADC
//   ADC <Wd>, <Wn>, <Wm>
//   ADC <Xd>, <Xn>, <Xm>
#define AARCH64_ADC_INS_MASK 0x7fe0fc00
#define AARCH64_ADC_INS_BITS 0x1a000000

// ADCS
//   ADCS <Wd>, <Wn>, <Wm>
//   ADCS <Xd>, <Xn>, <Xm>
#define AARCH64_ADCS_INS_MASK 0x7fe0fc00
#define AARCH64_ADCS_INS_BITS 0x3a000000

// NGC : SBC
//   NGC <Wd>, <Wm>
//   NGC <Xd>, <Xm>
bool aarch64_alias_ngc(struct aarch64_ins_adc *sbc);

// NGCS : SBCS
//   NGCS <Wd>, <Wm>
//   NGCS <Xd>, <Xm>
bool aarch64_alias_ngcs(struct aarch64_ins_adc *sbcs);

// SBC
//   SBC <Wd>, <Wn>, <Wm>
//   SBC <Xd>, <Xn>, <Xm>
#define AARCH64_SBC_INS_MASK 0x7fe0fc00
#define AARCH64_SBC_INS_BITS 0x5a000000

// SBCS
//   SBCS <Wd>, <Wn>, <Wm>
//   SBCS <Xd>, <Xn>, <Xm>
#define AARCH64_SBCS_INS_MASK 0x7fe0fc00
#define AARCH64_SBCS_INS_BITS 0x7a000000


// ---- ADD extended register, ADDS extended register, SUB extended register, SUBS extended
//      register ----
// ---- CMN extended register, CMP extended register ----

#define AARCH64_ADD_XR_MASK 0x1fe00000
#define AARCH64_ADD_XR_BITS 0x0b200000

struct aarch64_ins_add_xr {
	enum {
		AARCH64_INS_ADD_XR_OP_ADD = 0,
		AARCH64_INS_ADD_XR_OP_SUB = 1,
	}              op;
	bool           setflags;
	aarch64_reg    Rd;
	aarch64_reg    Rn;
	aarch64_reg    Rm;
	aarch64_extend extend;
	uint8_t        amount;
};

bool aarch64_decode_add_xr(uint32_t ins, struct aarch64_ins_add_xr *add_xr);

// ADD extended register
//   ADD <Wd|WSP>, <Wn|WSP>, <Wm>{, <extend> {#<amount>}}
//   ADD <Xd|SP>, <Xn|SP>, <R><m>{, <extend> {#<amount>}}
#define AARCH64_ADD_XR_INS_MASK 0x7fe00000
#define AARCH64_ADD_XR_INS_BITS 0x0b200000

// ADDS extended register
//   ADDS <Wd>, <Wn|WSP>, <Wm>{, <extend> {<amount>}}
//   ADDS <Xd>, <Xn|SP>, <R><m>{, <extend> {<amount>}}
#define AARCH64_ADDS_XR_INS_MASK 0x7fe00000
#define AARCH64_ADDS_XR_INS_BITS 0x2b200000

// CMN extended register : ADDS extended register
//   CMN <Wn|WSP>, <Wm>{, <extend> {<amount>}}
//   CMN <Xn|SP>, <R><m>{, <extend> {<amount>}}
bool aarch64_alias_cmn_xr(struct aarch64_ins_add_xr *adds_xr);

// CMP extended register : SUBS extended register
//   CMP <Wn|WSP>, <Wm>{, <extend> {#<amount>}}
//   CMP <Xn|SP>, <R><m>{, <extend> {#<amount>}}
bool aarch64_alias_cmp_xr(struct aarch64_ins_add_xr *subs_xr);

// SUB extended register
//   SUB <Wd|WSP>, <Wn|WSP>, <Wm>{, <extend> {<amount>}}
//   SUB <Xd|SP>, <Xn|SP>, <R><m>{, <extend> {<amount>}}
#define AARCH64_SUB_XR_INS_MASK 0x7fe00000
#define AARCH64_SUB_XR_INS_BITS 0x4b200000

// SUBS extended register
//   SUBS <Wd>, <Wn|WSP>, <Wm>{, <extend> {<amount>}}
//   SUBS <Xd>, <Xn|SP>, <R><m>{, <extend> {<amount>}}
#define AARCH64_SUBS_XR_INS_MASK 0x7fe00000
#define AARCH64_SUBS_XR_INS_BITS 0x6b200000


// ---- ADD immediate, ADDS immediate, SUB immediate, SUBS immediate ----
// ---- CMN immediate, CMP immediate, MOV to/from SP ----

#define AARCH64_ADD_IM_MASK 0x1f000000
#define AARCH64_ADD_IM_BITS 0x11000000

struct aarch64_ins_add_im {
	enum {
		AARCH64_ADD_IM_OP_ADD = 0,
		AARCH64_ADD_IM_OP_SUB = 1,
	}           op;
	bool        setflags;
	aarch64_reg Rd;
	aarch64_reg Rn;
	uint16_t    imm;
	uint8_t     shift;
};

bool aarch64_decode_add_im(uint32_t ins, struct aarch64_ins_add_im *add_im);

// ADD immediate
//   ADD <Wd|WSP>, <Wn|WSP>, #<imm>{, LSL #<shift>}
//   ADD <Xd|SP>, <Xn|SP>, #<imm>{, LSL #<shift>}
#define AARCH64_ADD_IM_INS_MASK 0x7f000000
#define AARCH64_ADD_IM_INS_BITS 0x11000000

// ADDS immediate
//   ADDS <Wd>, <Wn|WSP>, #<imm>{, LSL #<shift>}
//   ADDS <Xd>, <Xn|SP>, #<imm>{, LSL #<shift>}
#define AARCH64_ADDS_IM_INS_MASK 0x7f000000
#define AARCH64_ADDS_IM_INS_BITS 0x31000000

// CMN immediate : ADDS immediate
//   CMN <Wn|WSP>, #<imm>{, LSL #<shift>}
//   CMN <Xn|SP>, #<imm>{, LSL #<shift>}
bool aarch64_alias_cmn_im(struct aarch64_ins_add_im *adds_im);

// CMP immediate : SUBS immediate
//   CMP <Wn|WSP>, #<imm>{, LSL #<shift>}
//   CMP <Xn|SP>, #<imm>{, LSL #<shift>}
bool aarch64_alias_cmp_im(struct aarch64_ins_add_im *subs_im);

// MOV to/from SP : ADD immediate
//   MOV <Wd|WSP>, <Wn|WSP>
//   MOV <Xd|SP>, <Xn|SP>
bool aarch64_alias_mov_sp(struct aarch64_ins_add_im *add_im);

// SUB immediate
//   SUB <Wd|WSP>, <Wn|WSP>, #<imm>{, LSL #<shift>}
//   SUB <Xd|SP>, <Xn|SP>, #<imm>{, LSL #<shift>}
#define AARCH64_SUB_IM_INS_MASK 0x7f000000
#define AARCH64_SUB_IM_INS_BITS 0x51000000

// SUBS immediate
//   SUBS <Wd>, <Wn|WSP>, #<imm>{, LSL #<shift>}
//   SUBS <Xd>, <Xn|SP>, #<imm>{, LSL #<shift>}
#define AARCH64_SUBS_IM_INS_MASK 0x7f000000
#define AARCH64_SUBS_IM_INS_BITS 0x71000000


// ---- ADD shifted register, ADDS shifted register, SUB shifted register, SUBS shifted
//      register ----
// ---- CMN shifted register, CMP shifted register, NEG shifted register, NEGS shifted
//      register ----

#define AARCH64_ADD_SR_MASK 0x1f200000
#define AARCH64_ADD_SR_BITS 0x0b000000

struct aarch64_ins_add_sr {
	enum {
		AARCH64_ADD_SR_OP_ADD = 0,
		AARCH64_ADD_SR_OP_SUB = 1,
	}             op;
	bool          setflags;
	aarch64_reg   Rd;
	aarch64_reg   Rn;
	aarch64_reg   Rm;
	aarch64_shift shift;
	uint8_t       amount;
};

bool aarch64_decode_add_sr(uint32_t ins, struct aarch64_ins_add_sr *add_sr);

// ADD shifted register
//   ADD <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   ADD <Xd>, <Xn>, <Xm>{, <shift> #<amount>}

#define AARCH64_ADD_SR_INS_MASK 0x7f200000
#define AARCH64_ADD_SR_INS_BITS 0x0b000000

// ADDS shifted register
//   ADDS <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   ADDS <Xd>, <Xn>, <Xm>{, <shift> #<amount>}

#define AARCH64_ADDS_SR_INS_MASK 0x7f200000
#define AARCH64_ADDS_SR_INS_BITS 0x2b000000

// CMN shifted register : ADDS shifted register
//   CMN <Wn>, <Wm>{, <shift> #<amount>}
//   CMN <Xn>, <Xm>{, <shift> #<amount>}

bool aarch64_alias_cmn_sr(struct aarch64_ins_add_sr *adds_sr);

// CMP shifted register : SUBS shifted register
//   CMP <Wn>, <Wm>{, <shift> #<amount>}
//   CMP <Xn>, <Xm>{, <shift> #<amount>}

bool aarch64_alias_cmp_sr(struct aarch64_ins_add_sr *subs_sr);

// NEG shifted register : SUB shifted register
//   NEG <Wd>, <Wm>{, <shift> #<amount>}
//   NEG <Xd>, <Xm>{, <shift> #<amount>}

bool aarch64_alias_neg(struct aarch64_ins_add_sr *sub_sr);

// NEGS : SUBS shifted register
//   NEGS <Wd>, <Wm>{, <shift> #<amount>}
//   NEGS <Xd>, <Xm>{, <shift> #<amount>}

bool aarch64_alias_negs(struct aarch64_ins_add_sr *subs_sr);

// SUB shifted register
//   SUB <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   SUB <Xd>, <Xn>, <Xm>{, <shift> #<amount>}

#define AARCH64_SUB_SR_INS_MASK 0x7f200000
#define AARCH64_SUB_SR_INS_BITS 0x4b000000

// SUBS shifted register
//   SUBS <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   SUBS <Xd>, <Xn>, <Xm>{, <shift> #<amount>}

#define AARCH64_SUBS_SR_INS_MASK 0x7f200000
#define AARCH64_SUBS_SR_INS_BITS 0x6b000000


// ADR
//   ADR <Xd>, <label>

#define AARCH64_ADR_INS_MASK 0x9f000000
#define AARCH64_ADR_INS_BITS 0x10000000

struct aarch64_ins_adr_adrp {
	aarch64_reg Xd;
	uint64_t    label;
};

bool aarch64_ins_decode_adr(uint32_t ins, uint64_t pc, struct aarch64_ins_adr_adrp *adr);

// ADRP
//   ADRP <Xd>, <label>

#define AARCH64_ADRP_INS_MASK 0x9f000000
#define AARCH64_ADRP_INS_BITS 0x90000000

bool aarch64_ins_decode_adrp(uint32_t ins, uint64_t pc, struct aarch64_ins_adr_adrp *adrp);

// AND immediate
//   AND <Wd|WSP>, <Wn>, #<imm>
//   AND <Xd|XSP>, <Xn>, #<imm>

#define AARCH64_AND_IM_INS_MASK 0x7f800000
#define AARCH64_AND_IM_INS_BITS 0x12000000

struct aarch64_ins_and_orr_im {
	aarch64_reg Rd;
	aarch64_reg Rn;
	uint64_t    imm;
};

bool aarch64_ins_decode_and_im(uint32_t ins, struct aarch64_ins_and_orr_im *and_im);


// ---- AND shifted register, ANDS shifted register, ORR shifted register ----
// ---- MOV register, TST shifted register ----

#define AARCH64_AND_SR_MASK 0x1f200000
#define AARCH64_AND_SR_BITS 0x0a000000

struct aarch64_ins_and_sr {
	enum {
		AARCH64_AND_SR_OP_AND,
		AARCH64_AND_SR_OP_ORR,
	}             op;
	bool          setflags;
	aarch64_reg   Rd;
	aarch64_reg   Rn;
	aarch64_reg   Rm;
	aarch64_shift shift;
	uint8_t       amount;
};

bool aarch64_decode_and_sr(uint32_t ins, struct aarch64_ins_and_sr *and_sr);

// AND shifted register
//   AND <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   AND <Xd>, <Xn>, <Xm>{, <shift> #<amount>}
#define AARCH64_AND_SR_INS_MASK 0x7f200000
#define AARCH64_AND_SR_INS_BITS 0x0a000000

// ANDS shifted register
//   ANDS <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   ANDS <Xd>, <Xn>, <Xm>{, <shift> #<amount>}
#define AARCH64_ANDS_SR_INS_MASK 0x7f200000
#define AARCH64_ANDS_SR_INS_BITS 0x6a000000

// ORR shifted register
//   ORR <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   ORR <Xd>, <Xn>, <Xm>{, <shift> #<amount>}
#define AARCH64_ORR_SR_INS_MASK 0x7f200000
#define AARCH64_ORR_SR_INS_BITS 0x2a000000

// MOV register : ORR shifted register
//   MOV <Wd>, <Wm>
//   MOV <Xd>, <Xm>
bool aarch64_alias_mov_r(struct aarch64_ins_and_sr *orr_sr);

// TST shifted register : ANDS shifted register
//   ANDS <Wd>, <Wn>, <Wm>{, <shift> #<amount>}
//   ANDS <Xd>, <Xn>, <Xm>{, <shift> #<amount>}
bool aarch64_alias_tst_sr(struct aarch64_ins_and_sr *ands_sr);


// ANDS immediate
//   ANDS <Wd>, <Wn>, #<imm>
//   ANDS <Xd>, <Xn>, #<imm>

#define AARCH64_ANDS_IM_INS_MASK 0x7f800000
#define AARCH64_ANDS_IM_INS_BITS 0x72000000

bool aarch64_ins_decode_ands_im(uint32_t ins, struct aarch64_ins_and_orr_im *ands_im);

// ASR register : ASRV

// ASR immediate : SBFM

// ASRV

// AT : SYS

// B.cond

// B
//   B <label>

#define AARCH64_B_INS_MASK 0xfc000000
#define AARCH64_B_INS_BITS 0x14000000

struct aarch64_ins_b_bl {
	uint64_t label;
};

bool aarch64_ins_decode_b(uint32_t ins, uint64_t pc, struct aarch64_ins_b_bl *b);

// BFI : BFM

// BFM

// BFXIL : BFM

// BIC shifted register

// BICS shifted register

// BL
//   BL <label>

#define AARCH64_BL_INS_MASK 0xfc000000
#define AARCH64_BL_INS_BITS 0x94000000

bool aarch64_ins_decode_bl(uint32_t ins, uint64_t pc, struct aarch64_ins_b_bl *bl);


// ---- BLR, BR ----

#define AARCH64_BR_MASK 0xffdffc1f
#define AARCH64_BR_BITS 0xd61f0000

// BLR
//   BLR <Xn>
#define AARCH64_BLR_INS_MASK 0xfffffc1f
#define AARCH64_BLR_INS_BITS 0xd63f0000

// BR
//   BR <Xn>
#define AARCH64_BR_INS_MASK 0xfffffc1f
#define AARCH64_BR_INS_BITS 0xd61f0000

struct aarch64_ins_br {
	aarch64_reg Xn;
	bool        link;
};

bool aarch64_decode_br(uint32_t ins, struct aarch64_ins_br *br);


// BRK

// CBNZ

// CBZ

// CCMN immediate

// CCMN register

// CCMP immediate

// CCMP register

// CINC : CSINC

// CINV : CSINV

// CLREX

// CLS

// CLZ

// CNEG : CSNEG

// CRC32B, CRC32H, CRC32W, CRC32X

// CRC32CB, CRC32CH, CRC32CW, CRC32CX

// CSEL

// CSET : CSINC

// CSETM : CSINV

// CSINC

// CSINV

// CSNEG

// DC : SYS

// DCPS1

// DCPS2

// DCPS3

// DMB

// DRPS

// DSB

// EON shifted register

// EOR immediate

// EOR shifted register

// ERET

// EXTR

// HINT

// HLT

// HVC

// IC : SYS

// ISB

// LDAR

// LDARB

// LDARH

// LDAXP

// LDAXR

// LDAXRB

// LDAXRH

// LDNP

// LDP, post-index
//   LDP <Wt1>, <Wt2>, [<Xn|SP>], #<imm>
//   LDP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>

#define AARCH64_LDP_POST_INS_MASK 0x7fc00000
#define AARCH64_LDP_POST_INS_BITS 0x28c00000

struct aarch64_ins_ldp_stp {
	aarch64_reg Rt1;
	aarch64_reg Rt2;
	aarch64_reg Xn;
	int16_t     imm;
};

bool aarch64_ins_decode_ldp_post(uint32_t ins, struct aarch64_ins_ldp_stp *ldp_post);

// LDP, pre-index
//   LDP <Wt1>, <Wt2>, [<Xn|SP>, #<imm>]!
//   LDP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!

#define AARCH64_LDP_PRE_INS_MASK 0x7fc00000
#define AARCH64_LDP_PRE_INS_BITS 0x29c00000

bool aarch64_ins_decode_ldp_pre(uint32_t ins, struct aarch64_ins_ldp_stp *ldp_pre);

// LDP, signed offset
//   LDP <Wt1>, <Wt2>, [<Xn|SP>{, #<imm>}]
//   LDP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]

#define AARCH64_LDP_SI_INS_MASK 0x7fc00000
#define AARCH64_LDP_SI_INS_BITS 0x29400000

bool aarch64_ins_decode_ldp_si(uint32_t ins, struct aarch64_ins_ldp_stp *ldp_si);

// LDPSW

// LDR immediate, post-index
//   LDR <Wt>, [<Xn|SP>], #<imm>
//   LDR <Xt>, [<Xn|SP>], #<imm>

#define AARCH64_LDR_POST_INS_MASK 0xbfe00c00
#define AARCH64_LDR_POST_INS_BITS 0x28400400

struct aarch64_ins_ldr_str_ix {
	aarch64_reg Rt;
	aarch64_reg Xn;
	int16_t     imm;
};

bool aarch64_ins_decode_ldr_post(uint32_t ins, struct aarch64_ins_ldr_str_ix *ldr_post);

// LDR immediate, pre-index
//   LDR <Wt>, [<Xn|SP>, #<imm>]!
//   LDR <Xt>, [<Xn|SP>, #<imm>]!

#define AARCH64_LDR_PRE_INS_MASK 0xbfe00c00
#define AARCH64_LDR_PRE_INS_BITS 0xb8400c00

bool aarch64_ins_decode_ldr_pre(uint32_t ins, struct aarch64_ins_ldr_str_ix *ldr_pre);

// LDR immediate, unsigned offset
//   LDR <Wt>, [<Xn|SP>{, #<imm>}]
//   LDR <Xt>, [<Xn|SP>{, #<imm>}]

#define AARCH64_LDR_UI_INS_MASK 0xbfc00000
#define AARCH64_LDR_UI_INS_BITS 0xb9400000

struct aarch64_ins_ldr_str_ui {
	aarch64_reg Rt;
	aarch64_reg Xn;
	uint16_t    imm;
};

bool aarch64_ins_decode_ldr_ui(uint32_t ins, struct aarch64_ins_ldr_str_ui *ldr_ui);

// LDR literal
//   LDR <Wt>, <label>
//   LDR <Xt>, <label>

#define AARCH64_LDR_LIT_INS_MASK 0xbf000000
#define AARCH64_LDR_LIT_INS_BITS 0x18000000

struct aarch64_ins_ldr_lit {
	aarch64_reg Rt;
	uint64_t    label;
};

bool aarch64_ins_decode_ldr_lit(uint32_t ins, uint64_t pc, struct aarch64_ins_ldr_lit *ldr_lit);

// LDR register
//   LDR <Wt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]
//   LDR <Xt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]

#define AARCH64_LDR_R_INS_MASK 0xbfe00c00
#define AARCH64_LDR_R_INS_BITS 0xb8600800

struct aarch64_ins_ldr_str_r {
	aarch64_reg    Rt;
	aarch64_reg    Xn;
	aarch64_reg    Rm;
	aarch64_extend extend;
	uint8_t        amount;
};

bool aarch64_ins_decode_ldr_r(uint32_t ins, struct aarch64_ins_ldr_str_r *ldr_r);

// LDRB immediate

// LDRB register

// LDRH immediate

// LDRH register

// LDRSB immediate

// LDRSB register

// LDRSH immediate

// LDRSH register

// LDRSW immediate

// LDRSW literal

// LDRSW register

// LDTR

// LDTRB

// LDTRH

// LDTRSB

// LDTRSH

// LDTRSW

// LDUR

// LDURB

// LDURH

// LDURSB

// LDURSH

// LDURSW

// LDXP

// LDXR

// LDXRB

// LDXRH

// LSL register : LSLV

// LSL immediate : UBFM

// LSLV

// LSR register : LSLV

// LSR immediate : UBFM

// LSRV

// MADD

// MNEG : MSUB

// MOV inverted wide immediate : MOVN
//   MOV <Wd>, #<imm>
//   MOV <Xd>, #<imm>

struct aarch64_ins_movknz;

bool aarch64_alias_mov_nwi(struct aarch64_ins_movknz *movn);

// MOV wide immediate : MOVZ
//   MOV <Wd>, #<imm>
//   MOV <Xd>, #<imm>

bool aarch64_alias_mov_wi(struct aarch64_ins_movknz *movz);

// MOV bitmask immediate : ORR immediate
//   MOV <Wd|WSP>, #<imm>
//   MOV <Xd|SP>, #<imm>

bool aarch64_alias_mov_bi(struct aarch64_ins_and_orr_im *orr_im);

// MOVK
//   MOVK <Wd>, #<imm>{, LSL #<shift>}
//   MOVK <Xd>, #<imm>{, LSL #<shift>}

#define AARCH64_MOVK_INS_MASK 0x7f800000
#define AARCH64_MOVK_INS_BITS 0x72800000

struct aarch64_ins_movknz {
	aarch64_reg Rd;
	uint16_t    imm;
	uint8_t     shift;
};

bool aarch64_ins_decode_movk(uint32_t ins, struct aarch64_ins_movknz *movk);

// MOVN
//   MOVN <Wd>, #<imm>{, LSL #<shift>}
//   MOVN <Xd>, #<imm>{, LSL #<shift>}

#define AARCH64_MOVN_INS_MASK 0x7f800000
#define AARCH64_MOVN_INS_BITS 0x12800000

bool aarch64_ins_decode_movn(uint32_t ins, struct aarch64_ins_movknz *movn);

// MOVZ
//   MOVZ <Wd>, #<imm>{, LSL #<shift>}
//   MOVZ <Xd>, #<imm>{, LSL #<shift>}

#define AARCH64_MOVZ_INS_MASK 0x7f800000
#define AARCH64_MOVZ_INS_BITS 0x52800000

bool aarch64_ins_decode_movz(uint32_t ins, struct aarch64_ins_movknz *movz);

// MRS

// MSR immediate

// MSR register

// MSUB

// MUL : MADD

// MVN : ORN shifted register

// NOP
//   NOP

#define AARCH64_NOP_INS_MASK 0xffffffff
#define AARCH64_NOP_INS_BITS 0xd503201f

bool aarch64_ins_decode_nop(uint32_t ins);

// ORN shifted register

// ORR immediate
//   ORR <Wd|WSP>, <Wn>, #<imm>
//   ORR <Xd|XSP>, <Xn>, #<imm>

#define AARCH64_ORR_IM_INS_MASK 0x7f800000
#define AARCH64_ORR_IM_INS_BITS 0x32000000

bool aarch64_ins_decode_orr_im(uint32_t ins, struct aarch64_ins_and_orr_im *orr_im);

// PRFM immediate

// PRFM literal

// PRFM register

// PRFM unscaled offset

// RBIT

// RET
//   RET {<Xn>}

#define AARCH64_RET_INS_MASK 0xfffffc1f
#define AARCH64_RET_INS_BITS 0xd65f0000

struct aarch64_ins_ret {
	aarch64_reg Xn;
};

bool aarch64_ins_decode_ret(uint32_t ins, struct aarch64_ins_ret *ret);

// REV

// REV16

// REV32

// ROR immediate : EXTR

// ROR register : RORV

// RORV

// SBFIZ : SBFM

// SBFM

// SBFX : SBFM

// SDIV

// SEV

// SEVL

// SMADDL

// SMC

// SMNEGL : SMSUBL

// SMSUBL

// SMULH

// SMULL : SMADDL

// STLR

// STLRB

// STLRH

// STLXP

// STLXR

// STLXRB

// STLXRH

// STNP

// STP, post-index
//   STP <Wt1>, <Wt2>, [<Xn|SP>], #<imm>
//   STP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>

#define AARCH64_STP_POST_INS_MASK 0x7fc00000
#define AARCH64_STP_POST_INS_BITS 0x28800000

bool aarch64_ins_decode_stp_post(uint32_t ins, struct aarch64_ins_ldp_stp *stp_post);

// STP, pre-index
//   STP <Wt1>, <Wt2>, [<Xn|SP>, #<imm>]!
//   STP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!

#define AARCH64_STP_PRE_INS_MASK 0x7fc00000
#define AARCH64_STP_PRE_INS_BITS 0x29800000

bool aarch64_ins_decode_stp_pre(uint32_t ins, struct aarch64_ins_ldp_stp *stp_pre);

// STP, signed offset
//   STP <Wt1>, <Wt2>, [<Xn|SP>, #<imm>]
//   STP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]

#define AARCH64_STP_SI_INS_MASK 0x7fc00000
#define AARCH64_STP_SI_INS_BITS 0x29000000

bool aarch64_ins_decode_stp_si(uint32_t ins, struct aarch64_ins_ldp_stp *stp_si);

// STR immediate, post-index
//   STR <Wt>, [<Xn|SP>], #<imm>
//   STR <Xt>, [<Xn|SP>], #<imm>

#define AARCH64_STR_POST_INS_MASK 0xbfe00c00
#define AARCH64_STR_POST_INS_BITS 0xb8000400

bool aarch64_ins_decode_str_post(uint32_t ins, struct aarch64_ins_ldr_str_ix *str_post);

// STR immediate, pre-index
//   STR <Wt>, [<Xn|SP>, #<imm>]!
//   STR <Xt>, [<Xn|SP>, #<imm>]!

#define AARCH64_STR_PRE_INS_MASK 0xbfe00c00
#define AARCH64_STR_PRE_INS_BITS 0xb8000c00

bool aarch64_ins_decode_str_pre(uint32_t ins, struct aarch64_ins_ldr_str_ix *str_pre);

// STR immediate, unsigned offset
//   STR <Wt>, [<Xn|SP>, #<imm>]
//   STR <Xt>, [<Xn|SP>, #<imm>]

#define AARCH64_STR_UI_INS_MASK 0xbfc00000
#define AARCH64_STR_UI_INS_BITS 0xb9000000

bool aarch64_ins_decode_str_ui(uint32_t ins, struct aarch64_ins_ldr_str_ui *str_ui);

// STR register
//   STR <Wt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]
//   STR <Xt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]

#define AARCH64_STR_R_INS_MASK 0xbfe00c00
#define AARCH64_STR_R_INS_BITS 0xb8200800

bool aarch64_ins_decode_str_r(uint32_t ins, struct aarch64_ins_ldr_str_r *ldr_r);

// STRB immediate

// STRB register

// STRH immediate

// STRH register

// STTR

// STTRB

// STTRH

// STUR

// STXP

// STXR

// STXRB

// STXRH

// SVC

// SXTB : SBFM

// SXTH : SBFM

// SXTW : SBFM

// SYS

// SYSL

// TBNZ

// TBZ

// TLBI : SYS

// TST immediate : ANDS immediate
//   TST <Wn>, #<imm>
//   TST <Xn>, #<imm>

bool aarch64_alias_tst_im(struct aarch64_ins_and_orr_im *ands_im);

// UBFIZ : UBFM

// UBFM

// UBFX : UBFM

// UDIV

// UMADDL

// UMNEGL : UMSUBL

// UMSUBL

// UMULH

// UMULL : UMADDL

// UTXB : UBFM

// UTXH : UBFM

// WFE

// WFI

// YIELD

#endif
