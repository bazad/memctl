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
 * 	When this flag is set in an aarch64_gpreg, it indicates that the register should be
 * 	interpreted as ZR rather than SP in the context of the decoded instruction.
 */
#define AARCH64_ZR_INS 0x40

/*
 * aarch64_gpreg
 *
 * Description:
 * 	An AArch64 general purpose register identifier.
 */
typedef uint8_t aarch64_gpreg;

enum {
	AARCH64_X0  =                        0,
	AARCH64_X1  =                        1,
	AARCH64_X2  =                        2,
	AARCH64_X3  =                        3,
	AARCH64_X4  =                        4,
	AARCH64_X5  =                        5,
	AARCH64_X6  =                        6,
	AARCH64_X7  =                        7,
	AARCH64_X8  =                        8,
	AARCH64_X9  =                        9,
	AARCH64_X10 =                       10,
	AARCH64_X11 =                       11,
	AARCH64_X12 =                       12,
	AARCH64_X13 =                       13,
	AARCH64_X14 =                       14,
	AARCH64_X15 =                       15,
	AARCH64_X16 =                       16,
	AARCH64_X17 =                       17,
	AARCH64_X18 =                       18,
	AARCH64_X19 =                       19,
	AARCH64_X20 =                       20,
	AARCH64_X21 =                       21,
	AARCH64_X22 =                       22,
	AARCH64_X23 =                       23,
	AARCH64_X24 =                       24,
	AARCH64_X25 =                       25,
	AARCH64_X26 =                       26,
	AARCH64_X27 =                       27,
	AARCH64_X28 =                       28,
	AARCH64_X29 =                       29,
	AARCH64_X30 =                       30,
	AARCH64_SP  =                       31,
	AARCH64_XZR = AARCH64_ZR_INS |      31,
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
	AARCH64_WSP =                  32 | 31,
	AARCH64_WZR = AARCH64_ZR_INS | 32 | 31,
};

/*
 * macro AARCH64_GPREGSIZE
 *
 * Description:
 * 	Return the width of the given register.
 */
#define AARCH64_GPREGSIZE(reg)	((reg) & 32 ? 32 : 64)

/*
 * macro AARCH64_GPREGID
 *
 * Description:
 * 	Return the numeric ID of the given register.
 */
#define AARCH64_GPREGID(reg)	((reg) & 0x1f)

/*
 * macro AARCH64_GPREGZR
 *
 * Description:
 * 	Return nonozero if the given register should be interpreted as ZR in the context of the
 * 	decoded instruction.
 */
#define AARCH64_GPREGZR(reg)	((reg) & AARCH64_ZR_INS)

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

#define AARCH64_ADC_CLASS_MASK 0x1fe0fc00
#define AARCH64_ADC_CLASS_BITS 0x1a000000

struct aarch64_ins_adc {
	uint8_t       adc:1;
	uint8_t       setflags:1;
	uint8_t       _fill:6;
	aarch64_gpreg Rd;
	aarch64_gpreg Rn;
	aarch64_gpreg Rm;
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

#define AARCH64_ADD_XR_CLASS_MASK 0x1fe00000
#define AARCH64_ADD_XR_CLASS_BITS 0x0b200000

struct aarch64_ins_add_xr {
	uint8_t        add:1;
	uint8_t        setflags:1;
	uint8_t        _fill:6;
	aarch64_gpreg  Rd;
	aarch64_gpreg  Rn;
	aarch64_gpreg  Rm;
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

#define AARCH64_ADD_IM_CLASS_MASK 0x1f000000
#define AARCH64_ADD_IM_CLASS_BITS 0x11000000

struct aarch64_ins_add_im {
	uint8_t       add:1;
	uint8_t       setflags:1;
	uint8_t       _fill:6;
	aarch64_gpreg Rd;
	aarch64_gpreg Rn;
	uint16_t      imm;
	uint8_t       shift;
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

#define AARCH64_ADD_SR_CLASS_MASK 0x1f200000
#define AARCH64_ADD_SR_CLASS_BITS 0x0b000000

struct aarch64_ins_add_sr {
	uint8_t       add:1;
	uint8_t       setflags:1;
	uint8_t       _fill:6;
	aarch64_gpreg Rd;
	aarch64_gpreg Rn;
	aarch64_gpreg Rm;
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


// ---- ADR, ADRP ----

#define AARCH64_ADR_CLASS_MASK 0x1f000000
#define AARCH64_ADR_CLASS_BITS 0x10000000

struct aarch64_ins_adr {
	uint8_t       adrp:1;
	uint8_t       _fill:7;
	aarch64_gpreg Xd;
	uint64_t      label;
};

bool aarch64_decode_adr(uint32_t ins, uint64_t pc, struct aarch64_ins_adr *adr);

// ADR
//   ADR <Xd>, <label>
#define AARCH64_ADR_INS_MASK 0x9f000000
#define AARCH64_ADR_INS_BITS 0x10000000

// ADRP
//   ADRP <Xd>, <label>
#define AARCH64_ADRP_INS_MASK 0x9f000000
#define AARCH64_ADRP_INS_BITS 0x90000000


// ---- AND immediate, ANDS immediate, ORR immediate ----
// ---- MOV bitmask immediate, TST immediate ----

#define AARCH64_AND_IM_CLASS_MASK 0x5f800000
#define AARCH64_AND_IM_CLASS_BITS 0x12000000

struct aarch64_ins_and_im {
	uint8_t       and:1;
	uint8_t       setflags:1;
	uint8_t       _fill:6;
	aarch64_gpreg Rd;
	aarch64_gpreg Rn;
	uint64_t      imm;
};

bool aarch64_decode_and_im(uint32_t ins, struct aarch64_ins_and_im *and_im);

// AND immediate
//   AND <Wd|WSP>, <Wn>, #<imm>
//   AND <Xd|XSP>, <Xn>, #<imm>
#define AARCH64_AND_IM_INS_MASK 0x7f800000
#define AARCH64_AND_IM_INS_BITS 0x12000000

// ANDS immediate
//   ANDS <Wd>, <Wn>, #<imm>
//   ANDS <Xd>, <Xn>, #<imm>
#define AARCH64_ANDS_IM_INS_MASK 0x7f800000
#define AARCH64_ANDS_IM_INS_BITS 0x72000000

// MOV bitmask immediate : ORR immediate
//   MOV <Wd|WSP>, #<imm>
//   MOV <Xd|SP>, #<imm>
bool aarch64_alias_mov_bi(struct aarch64_ins_and_im *orr_im);

// ORR immediate
//   ORR <Wd|WSP>, <Wn>, #<imm>
//   ORR <Xd|XSP>, <Xn>, #<imm>
#define AARCH64_ORR_IM_INS_MASK 0x7f800000
#define AARCH64_ORR_IM_INS_BITS 0x32000000

// TST immediate : ANDS immediate
//   TST <Wn>, #<imm>
//   TST <Xn>, #<imm>
bool aarch64_alias_tst_im(struct aarch64_ins_and_im *ands_im);


// ---- AND shifted register, ANDS shifted register, ORR shifted register ----
// ---- MOV register, TST shifted register ----

#define AARCH64_AND_SR_CLASS_MASK 0x1f200000
#define AARCH64_AND_SR_CLASS_BITS 0x0a000000

struct aarch64_ins_and_sr {
	uint8_t       and:1;
	uint8_t       setflags:1;
	uint8_t       _fill:6;
	aarch64_gpreg Rd;
	aarch64_gpreg Rn;
	aarch64_gpreg Rm;
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


// ASR register : ASRV

// ASR immediate : SBFM

// ASRV

// AT : SYS

// B.cond

// ---- B, BL ----

#define AARCH64_B_CLASS_MASK 0x7c000000
#define AARCH64_B_CLASS_BITS 0x14000000

struct aarch64_ins_b {
	uint8_t  link:1;
	uint8_t  _fill:7;
	uint64_t label;
};

bool aarch64_decode_b(uint32_t ins, uint64_t pc, struct aarch64_ins_b *b);

// B
//   B <label>
#define AARCH64_B_INS_MASK 0xfc000000
#define AARCH64_B_INS_BITS 0x14000000

// BL
//   BL <label>
#define AARCH64_BL_INS_MASK 0xfc000000
#define AARCH64_BL_INS_BITS 0x94000000


// BFI : BFM

// BFM

// BFXIL : BFM

// BIC shifted register

// BICS shifted register


// ---- BLR, BR, RET ----

#define AARCH64_BR_CLASS_MASK 0xff9ffc1f
#define AARCH64_BR_CLASS_BITS 0xd61f0000

struct aarch64_ins_br {
	uint8_t       ret:1;
	uint8_t       link:1;
	uint8_t       _fill:6;
	aarch64_gpreg Xn;
};

bool aarch64_decode_br(uint32_t ins, struct aarch64_ins_br *br);

// BLR
//   BLR <Xn>
#define AARCH64_BLR_INS_MASK 0xfffffc1f
#define AARCH64_BLR_INS_BITS 0xd63f0000

// BR
//   BR <Xn>
#define AARCH64_BR_INS_MASK 0xfffffc1f
#define AARCH64_BR_INS_BITS 0xd61f0000

// RET
//   RET {<Xn>}
#define AARCH64_RET_INS_MASK 0xfffffc1f
#define AARCH64_RET_INS_BITS 0xd65f0000


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


// ---- LDNP, LDP post-index, LDP pre-index, LDP signed offset,
//      LDPSW post-index, LDPSW pre-index, LDPSW signed offset,
//      STNP, STP post-index, STP pre-index, STP signed offset  ----

#define AARCH64_LDP_CLASS_MASK 0x3e000000
#define AARCH64_LDP_CLASS_BITS 0x28000000

struct aarch64_ins_ldp {
	uint8_t       load:1;
	uint8_t       size:2;
	uint8_t       wb:1;
	uint8_t       post:1;
	uint8_t       sign:1;
	uint8_t       nt:1;
	uint8_t       _fill:1;
	aarch64_gpreg Rt1;
	aarch64_gpreg Rt2;
	aarch64_gpreg Xn;
	int16_t       imm;
};

bool aarch64_decode_ldp(uint32_t ins, struct aarch64_ins_ldp *ldp);

// LDNP
//   LDNP <Wt1>, <Wt2>, [<Xn|SP>{, #<imm>}]
//   LDNP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDNP_INS_MASK 0x7fc00000
#define AARCH64_LDNP_INS_BITS 0x28400000

// LDP, post-index
//   LDP <Wt1>, <Wt2>, [<Xn|SP>], #<imm>
//   LDP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>
#define AARCH64_LDP_POST_INS_MASK 0x7fc00000
#define AARCH64_LDP_POST_INS_BITS 0x28c00000

// LDP, pre-index
//   LDP <Wt1>, <Wt2>, [<Xn|SP>, #<imm>]!
//   LDP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDP_PRE_INS_MASK 0x7fc00000
#define AARCH64_LDP_PRE_INS_BITS 0x29c00000

// LDP, signed offset
//   LDP <Wt1>, <Wt2>, [<Xn|SP>{, #<imm>}]
//   LDP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDP_SI_INS_MASK 0x7fc00000
#define AARCH64_LDP_SI_INS_BITS 0x29400000

// LDPSW, post-index
//   LDPSW <Xt1>, <Xt2>, [<Xn|SP>], #<imm>
#define AARCH64_LDPSW_POST_INS_MASK 0xffc00000
#define AARCH64_LDPSW_POST_INS_BITS 0x68c00000

// LDPSW, pre-index
//   LDPSW <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDPSW_PRE_INS_MASK 0xffc00000
#define AARCH64_LDPSW_PRE_INS_BITS 0x69c00000

// LDPSW, signed offset
//   LDPSW <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDPSW_SOFF_INS_MASK 0xffc00000
#define AARCH64_LDPSW_SOFF_INS_BITS 0x69400000

// STNP
//   STNP <Wt1>, <Wt2>, [<Xn|SP>{, #<imm>}]
//   STNP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
#define AARCH64_STNP_INS_MASK 0x7fc00000
#define AARCH64_STNP_INS_BITS 0x28000000

// STP, post-index
//   STP <Wt1>, <Wt2>, [<Xn|SP>], #<imm>
//   STP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>
#define AARCH64_STP_POST_INS_MASK 0x7fc00000
#define AARCH64_STP_POST_INS_BITS 0x28800000

// STP, pre-index
//   STP <Wt1>, <Wt2>, [<Xn|SP>, #<imm>]!
//   STP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!
#define AARCH64_STP_PRE_INS_MASK 0x7fc00000
#define AARCH64_STP_PRE_INS_BITS 0x29800000

// STP, signed offset
//   STP <Wt1>, <Wt2>, [<Xn|SP>, #<imm>]
//   STP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]
#define AARCH64_STP_SI_INS_MASK 0x7fc00000
#define AARCH64_STP_SI_INS_BITS 0x29000000


// ---- LDR immediate post-index, LDR immediate pre-index, LDRB immediate post-index,
//      LDRB immediate pre-index, LDRH immediate post-index, LDRH immediate pre-index,
//      LDRSB immediate post-index, LDRSB immediate pre-index, LDRSH immediate post-index,
//      LDRSH immediate pre-index, LDRSW immediate post-index, LDRSW immediate pre-index,
//      STR immediate post-index, STR immediate pre-index, STRB immediate post-index,
//      STRB immediate pre-index, STRH immediate post-index, STRH immediate pre-index ----

#define AARCH64_LDR_IX_CLASS_MASK 0x3f200400
#define AARCH64_LDR_IX_CLASS_BITS 0x38000400

struct aarch64_ins_ldr_im {
	uint8_t       load:1;
	uint8_t       size:2;
	uint8_t       wb:1;
	uint8_t       post:1;
	uint8_t       sign:1;
	uint8_t       _fill:2;
	aarch64_gpreg Rt;
	aarch64_gpreg Xn;
	int32_t       imm;
};

bool aarch64_decode_ldr_ix(uint32_t ins, struct aarch64_ins_ldr_im *ldr_ix);

// LDR immediate, post-index
//   LDR <Wt>, [<Xn|SP>], #<imm>
//   LDR <Xt>, [<Xn|SP>], #<imm>
#define AARCH64_LDR_POST_INS_MASK 0xbfe00c00
#define AARCH64_LDR_POST_INS_BITS 0xb8400400

// LDR immediate, pre-index
//   LDR <Wt>, [<Xn|SP>, #<imm>]!
//   LDR <Xt>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDR_PRE_INS_MASK 0xbfe00c00
#define AARCH64_LDR_PRE_INS_BITS 0xb8400c00

// LDRB immediate, post-index
//   LDRB <Wt>, [<Xn|SP>], #<imm>
#define AARCH64_LDRB_POST_INS_MASK 0xffe00c00
#define AARCH64_LDRB_POST_INS_BITS 0x38400400

// LDRB immediate, pre-index
//   LDRB <Wt>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDRB_PRE_INS_MASK 0xffe00c00
#define AARCH64_LDRB_PRE_INS_BITS 0x38400c00

// LDRH immediate, post-index
//   LDRH <Wt>, [<Xn|SP>], #<imm>
#define AARCH64_LDRH_POST_INS_MASK 0xffe00c00
#define AARCH64_LDRH_POST_INS_BITS 0x78400400

// LDRH immediate, pre-index
//   LDRH <Wt>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDRH_PRE_INS_MASK 0xffe00c00
#define AARCH64_LDRH_PRE_INS_BITS 0x78400c00

// LDRSB immediate, post-index
//   LDRSB <Wt>, [<Xn|SP>], #<imm>
//   LDRSB <Xt>, [<Xn|SP>], #<imm>
#define AARCH64_LDRSB_POST_INS_MASK 0xffa00c00
#define AARCH64_LDRSB_POST_INS_BITS 0x38800400

// LDRSB immediate, pre-index
//   LDRSB <Wt>, [<Xn|SP>, #<imm>]!
//   LDRSB <Xt>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDRSB_PRE_INS_MASK 0xffa00c00
#define AARCH64_LDRSB_PRE_INS_BITS 0x38800c00

// LDRSH immediate, post-index
//   LDRSH <Wt>, [<Xn|SP>], #<imm>
//   LDRSH <Xt>, [<Xn|SP>], #<imm>
#define AARCH64_LDRSH_POST_INS_MASK 0xffa00c00
#define AARCH64_LDRSH_POST_INS_BITS 0x78800400

// LDRSH immediate, pre-index
//   LDRSH <Wt>, [<Xn|SP>, #<imm>]!
//   LDRSH <Xt>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDRSH_PRE_INS_MASK 0xffa00c00
#define AARCH64_LDRSH_PRE_INS_BITS 0x78800c00

// LDRSW immediate, post-index
//   LDRSW <Xt>, [<Xn|SP>], #<imm>
#define AARCH64_LDRSW_POST_INS_MASK 0xffe00c00
#define AARCH64_LDRSW_POST_INS_BITS 0xb8800400

// LDRSW immediate, pre-index
//   LDRSW <Xt>, [<Xn|SP>, #<imm>]!
#define AARCH64_LDRSW_PRE_INS_MASK 0xffe00c00
#define AARCH64_LDRSW_PRE_INS_BITS 0xb8800c00

// STR immediate, post-index
//   STR <Wt>, [<Xn|SP>], #<imm>
//   STR <Xt>, [<Xn|SP>], #<imm>
#define AARCH64_STR_POST_INS_MASK 0xbfe00c00
#define AARCH64_STR_POST_INS_BITS 0xb8000400

// STR immediate, pre-index
//   STR <Wt>, [<Xn|SP>, #<imm>]!
//   STR <Xt>, [<Xn|SP>, #<imm>]!
#define AARCH64_STR_PRE_INS_MASK 0xbfe00c00
#define AARCH64_STR_PRE_INS_BITS 0xb8000c00

// STRB immediate, post-index
//   STRB <Wt>, [<Xn|SP>], #<imm>
#define AARCH64_STRB_POST_INS_MASK 0xffe00c00
#define AARCH64_STRB_POST_INS_BITS 0x38000400

// STRB immediate, pre-index
//   STRB <Wt>, [<Xn|SP>, #<imm>]!
#define AARCH64_STRB_PRE_INS_MASK 0xffe00c00
#define AARCH64_STRB_PRE_INS_BITS 0x38000c00

// STRH immediate, post-index
//   STRH <Wt>, [<Xn|SP>], #<imm>
#define AARCH64_STRH_POST_INS_MASK 0xffe00c00
#define AARCH64_STRH_POST_INS_BITS 0x78000400

// STRH immediate, pre-index
//   STRH <Wt>, [<Xn|SP>, #<imm>]!
#define AARCH64_STRH_PRE_INS_MASK 0xffe00c00
#define AARCH64_STRH_PRE_INS_BITS 0x78000c00


// ---- LDR immediate unsigned offset, LDRB immediate unsigned offset,
//      LDRH immediate unsigned offset, LDRSB immediate unsigned offset,
//      LDRSH immediate unsigned offset, LDRSW immediate unsigned offset,
//      STR immediate unsigned offset, STRB immediate unsigned offset,
//      STRH immediate unsigned offset ----

#define AARCH64_LDR_UI_CLASS_MASK 0x3f000000
#define AARCH64_LDR_UI_CLASS_BITS 0x39000000

bool aarch64_decode_ldr_ui(uint32_t ins, struct aarch64_ins_ldr_im *ldr_ui);

// LDR immediate, unsigned offset
//   LDR <Wt>, [<Xn|SP>{, #<imm>}]
//   LDR <Xt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDR_UI_INS_MASK 0xbfc00000
#define AARCH64_LDR_UI_INS_BITS 0xb9400000

// LDRB immediate, unsigned offset
//   LDRB <Wt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDRB_UI_INS_MASK 0xffc00000
#define AARCH64_LDRB_UI_INS_BITS 0x39400000

// LDRH immediate, unsigned offset
//   LDRH <Wt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDRH_UI_INS_MASK 0xffc00000
#define AARCH64_LDRH_UI_INS_BITS 0x79400000

// LDRSB immediate, unsigned offset
//   LDRSB <Wt>, [<Xn|SP>{, #<imm>}]
//   LDRSB <Xt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDRSB_UI_INS_MASK 0xff800000
#define AARCH64_LDRSB_UI_INS_BITS 0x39800000

// LDRSH immediate, unsigned offset
//   LDRSH <Wt>, [<Xn|SP>{, #<imm>}]
//   LDRSH <Xt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDRSH_UI_INS_MASK 0xff800000
#define AARCH64_LDRSH_UI_INS_BITS 0x79800000

// LDRSW immediate, unsigned offset
//   LDRSW <Xt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_LDRSW_UI_INS_MASK 0xffc00000
#define AARCH64_LDRSW_UI_INS_BITS 0xb9800000

// STR immediate, unsigned offset
//   STR <Wt>, [<Xn|SP>{, #<imm>}]
//   STR <Xt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_STR_UI_INS_MASK 0xbfc00000
#define AARCH64_STR_UI_INS_BITS 0xb9000000

// STRB immediate, unsigned offset
//   STRB <Wt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_STRB_UI_INS_MASK 0xffc00000
#define AARCH64_STRB_UI_INS_BITS 0x39000000

// STRH immediate, unsigned offset
//   STRH <Wt>, [<Xn|SP>{, #<imm>}]
#define AARCH64_STRH_UI_INS_MASK 0xffc00000
#define AARCH64_STRH_UI_INS_BITS 0x79000000


// LDR literal
//   LDR <Wt>, <label>
//   LDR <Xt>, <label>

#define AARCH64_LDR_LIT_INS_MASK 0xbf000000
#define AARCH64_LDR_LIT_INS_BITS 0x18000000

struct aarch64_ins_ldr_lit {
	aarch64_gpreg Rt;
	uint64_t      label;
};

bool aarch64_ins_decode_ldr_lit(uint32_t ins, uint64_t pc, struct aarch64_ins_ldr_lit *ldr_lit);

// LDR register
//   LDR <Wt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]
//   LDR <Xt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]

#define AARCH64_LDR_R_INS_MASK 0xbfe00c00
#define AARCH64_LDR_R_INS_BITS 0xb8600800

struct aarch64_ins_ldr_str_r {
	aarch64_gpreg  Rt;
	aarch64_gpreg  Xn;
	aarch64_gpreg  Rm;
	aarch64_extend extend;
	uint8_t        amount;
};

bool aarch64_ins_decode_ldr_r(uint32_t ins, struct aarch64_ins_ldr_str_r *ldr_r);

// LDRB register

// LDRH register

// LDRSB register

// LDRSH register

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


// ---- MOVK, MOVN, MOVZ ----
// ---- MOV inverted wide immediate, MOV wide immediate ----

#define AARCH64_MOV_CLASS_MASK 0x1f800000
#define AARCH64_MOV_CLASS_BITS 0x12800000

struct aarch64_ins_mov {
	uint8_t       k:1;
	uint8_t       n:1;
	uint8_t       _fill:6;
	aarch64_gpreg Rd;
	uint16_t      imm;
	uint8_t       shift;
};

bool aarch64_decode_mov(uint32_t ins, struct aarch64_ins_mov *movk);

// MOV inverted wide immediate : MOVN
//   MOV <Wd>, #<imm>
//   MOV <Xd>, #<imm>
bool aarch64_alias_mov_nwi(struct aarch64_ins_mov *movn);

// MOV wide immediate : MOVZ
//   MOV <Wd>, #<imm>
//   MOV <Xd>, #<imm>
bool aarch64_alias_mov_wi(struct aarch64_ins_mov *movz);

// MOVK
//   MOVK <Wd>, #<imm>{, LSL #<shift>}
//   MOVK <Xd>, #<imm>{, LSL #<shift>}
#define AARCH64_MOVK_INS_MASK 0x7f800000
#define AARCH64_MOVK_INS_BITS 0x72800000

// MOVN
//   MOVN <Wd>, #<imm>{, LSL #<shift>}
//   MOVN <Xd>, #<imm>{, LSL #<shift>}
#define AARCH64_MOVN_INS_MASK 0x7f800000
#define AARCH64_MOVN_INS_BITS 0x12800000

// MOVZ
//   MOVZ <Wd>, #<imm>{, LSL #<shift>}
//   MOVZ <Xd>, #<imm>{, LSL #<shift>}
#define AARCH64_MOVZ_INS_MASK 0x7f800000
#define AARCH64_MOVZ_INS_BITS 0x52800000


// MRS

// MSR immediate

// MSR register

// MSUB

// MUL : MADD

// MVN : ORN shifted register


// ---- NOP ----

// NOP
//   NOP
#define AARCH64_NOP_INS_MASK 0xffffffff
#define AARCH64_NOP_INS_BITS 0xd503201f

bool aarch64_decode_nop(uint32_t ins);


// ORN shifted register

// PRFM immediate

// PRFM literal

// PRFM register

// PRFM unscaled offset

// RBIT

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

// STR register
//   STR <Wt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]
//   STR <Xt>, [<Xn|SP>, (<Wm>|<Xm>){, <extend> {<amount>}}]

#define AARCH64_STR_R_INS_MASK 0xbfe00c00
#define AARCH64_STR_R_INS_BITS 0xb8200800

bool aarch64_ins_decode_str_r(uint32_t ins, struct aarch64_ins_ldr_str_r *ldr_r);

// STRB register

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
