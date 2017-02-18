#include "disasm.h"

#include <assert.h>
#include <stdlib.h>
#include <strings.h>

// Bit manipulations

/*
 * ones
 *
 * Description:
 * 	Returns a mask of the given number of bits.
 */
static inline uint64_t
ones(unsigned n) {
	return (n == 64 ? (uint64_t)(-1) : ((uint64_t)1 << n) - 1);
}

/*
 * lobits
 *
 * Description:
 * 	Returns the least significant n bits of x.
 */
static inline uint64_t
lobits(uint64_t x, unsigned n) {
	return (x & ones(n));
}

/*
 * test
 *
 * Description:
 * 	Returns 1 if (zero-indexed) bit n is set in x, 0 otherwise.
 */
static inline unsigned
test(uint64_t x, unsigned n) {
	return (x & (1 << n)) != 0;
}

/*
 * ror
 *
 * Description:
 * 	Rotate x, an n-bit value, right by shift bits.
 */
static inline uint64_t
ror(uint64_t x, unsigned n, unsigned shift) {
	assert(x == lobits(x, n));
	unsigned m = shift % n;
	return lobits(x << (n - m), n) | (x >> m);
}

/*
 * replicate
 *
 * Description:
 * 	Replicate x, an m-bit value, so that it is n bits.
 */
static uint64_t
replicate(uint64_t x, unsigned m, unsigned n) {
	assert(n >= m && n % m == 0);
	assert(x == lobits(x, m));
	uint64_t r = 0;
	for (unsigned c = n / m; c > 0; c--) {
		r = (r << m) | x;
	}
	return r;
}

/*
 * extract
 *
 * Description:
 * 	Extract bits lo to hi of x, inclusive, sign extending the result if sign is 1. Return the
 * 	extracted value shifted left by shift bits.
 */
static inline uint64_t
extract(uint64_t x, unsigned sign, unsigned hi, unsigned lo, unsigned shift) {
	unsigned d = 64 - (hi - lo + 1);
	if (sign) {
		return ((((int64_t) x) >> lo) << d) >> (d - shift);
	} else  {
		return ((((uint64_t) x) >> lo) << d) >> (d - shift);
	}
}

// Instruction routines

#define USE_ZR	AARCH64_ZR_INS
#define USE_SP	0

/*
 * get_reg
 *
 * Description:
 * 	Returns the register number at the given index in the instruction.
 */
static inline aarch64_reg
get_reg(uint32_t ins, unsigned sf, unsigned zrsp, unsigned lo) {
	assert(sf == 0 || sf == 1);
	assert(zrsp == USE_ZR || zrsp == USE_SP);
	aarch64_reg reg     = ((ins >> lo) & 0x1f);
	aarch64_reg size    = (32 * (sf + 1));
	aarch64_reg zr_hint = zrsp * (reg == AARCH64_RSP);
	return zr_hint | size | reg;
}

/*
 * reg_is_zrsp
 *
 * Description:
 * 	Returns true if the given register has the same name as the ZR or SP register.
 */
static inline bool
reg_is_zrsp(aarch64_reg reg) {
	return AARCH64_REGNAME(reg) == AARCH64_RSP;
}

/*
 * get_shift
 *
 * Description:
 * 	Returns the shift type at the given index in the instruction.
 */
static inline aarch64_shift
get_shift(uint32_t ins, unsigned lo) {
	return (ins >> lo) & 0x3;
}

/*
 * get_extend
 *
 * Description:
 * 	Returns the extend type at the given index in the instruction.
 */
static inline aarch64_extend
get_extend(uint32_t ins, unsigned lo) {
	return (ins >> lo) & 0x7;
}

/*
 * decode_bit_masks
 *
 * Description:
 * 	Decode the bit masks in some logical immediate instructions.
 */
static bool
decode_bit_masks(unsigned sf, uint8_t N, uint8_t imms, uint8_t immr, uint8_t immediate,
                 uint64_t *wmask, uint64_t *tmask) {
	uint8_t len = fls((N << 6) | lobits(~imms, 6));
	if (len <= 1) {
		return false;
	}
	len--;
	uint8_t levels = ones(len);
	if (immediate != 0 && (imms & levels) == levels) {
		return false;
	}
	uint8_t S = imms & levels;
	uint8_t R = immr & levels;
	int8_t diff = S - R;
	uint8_t esize = 1 << len;
	uint8_t d = lobits(diff, len - 1);
	uint64_t welem = ones(S + 1);
	uint64_t telem = ones(d + 1);
	*wmask = replicate(ror(welem, esize, R), esize, (sf ? 64 : 32));
	*tmask = replicate(telem, esize, (sf ? 64 : 32));
	return true;
}

// Generalized decoders

static bool
decode_adc_sbc(uint32_t ins, struct aarch64_adc_sbc *adc_sbc) {
	//  31   30  29  28             21 20       16 15         10 9         5 4         0
	// +----+---+---+-----------------+-----------+-------------+-----------+-----------+
	// | sf | 0 | 0 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | ADC
	// | sf | 0 | 1 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | ADCS
	// | sf | 1 | 0 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | SBC
	// | sf | 1 | 1 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | SBCS
	// +----+---+---+-----------------+-----------+-------------+-----------+-----------+
	//       op   S
	unsigned sf = test(ins, 31);
	adc_sbc->Rd = get_reg(ins, sf, USE_ZR, 0);
	adc_sbc->Rn = get_reg(ins, sf, USE_ZR, 5);
	adc_sbc->Rm = get_reg(ins, sf, USE_ZR, 16);
	return true;
}

static bool
decode_add_sub_xr(uint32_t ins, struct aarch64_add_sub_xr *add_sub_xr) {
	//  31   30  29  28       24 23 22 21  20       16 15    13 12   10 9         5 4         0
	// +----+---+---+-----------+-----+---+-----------+--------+-------+-----------+-----------+
	// | sf | 0 | 0 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | ADD extended register
	// | sf | 0 | 1 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | ADDS extended register
	// | sf | 1 | 0 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | SUB extended register
	// | sf | 1 | 1 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | SUBS extended register
	// +----+---+---+-----------+-----+---+-----------+--------+-------+-----------+-----------+
	//       op   S
	unsigned shift = extract(ins, 0, 12, 10, 0);
	if (shift > 4) {
		return false;
	}
	unsigned sf           = test(ins, 31);
	unsigned S            = test(ins, 29);
	aarch64_extend extend = get_extend(ins, 13);
	unsigned Xm           = (extend & 0x3) == 0x3;
	aarch64_reg Rd        = get_reg(ins, sf, (S ? USE_ZR : USE_SP), 0);
	aarch64_reg Rn        = get_reg(ins, sf, USE_SP, 5);
	if (((!S && reg_is_zrsp(Rd)) || reg_is_zrsp(Rn)) && extend == (AARCH64_EXTEND_UXTW + sf)) {
		extend |= AARCH64_EXTEND_LSL;
	}
	add_sub_xr->Rd     = Rd;
	add_sub_xr->Rn     = Rn;
	add_sub_xr->Rm     = get_reg(ins, Xm, USE_ZR, 16);
	add_sub_xr->extend = extend;
	add_sub_xr->amount = shift;
	return true;
}

static bool
decode_add_sub_im(uint32_t ins, struct aarch64_add_sub_im *add_sub_im) {
	//  31   30  29  28       24 23   22 21                     10 9         5 4         0
	// +----+---+---+-----------+-------+-------------------------+-----------+-----------+
	// | sf | 0 | 0 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | ADD immediate
	// | sf | 0 | 1 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | ADDS immediate
	// | sf | 1 | 0 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | SUB immediate
	// | sf | 1 | 1 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | SUBS immediate
	// +----+---+---+-----------+-------+-------------------------+-----------+-----------+
	//       op   S
	unsigned sf    = test(ins, 31);
	unsigned S     = test(ins, 29);
	unsigned shift = extract(ins, 0, 23, 22, 0);
	uint16_t imm   = extract(ins, 0, 21, 10, 0);
	if (test(shift, 1)) {
		return false;
	}
	add_sub_im->Rd    = get_reg(ins, sf, (S ? USE_ZR : USE_SP), 0);
	add_sub_im->Rn    = get_reg(ins, sf, USE_SP, 5);
	add_sub_im->imm   = imm;
	add_sub_im->shift = 12 * shift;
	return true;
}

static bool
decode_and_orr_im(uint32_t ins, struct aarch64_and_orr_im *and_orr_im) {
	//  31   30 29 28         23 22  21         16 15         10 9         5 4         0
	// +----+-----+-------------+---+-------------+-------------+-----------+-----------+
	// | sf | 0 0 | 1 0 0 1 0 0 | N |    immr     |    imms     |    Rn     |    Rd     | AND immediate
	// | sf | 1 1 | 1 0 0 1 0 0 | N |    immr     |    imms     |    Rn     |    Rd     | ANDS immediate
	// | sf | 0 1 | 1 0 0 1 0 0 | N |    immr     |    imms     |    Rn     |    Rd     | ORR immediate
	// +----+-----+-------------+---+-------------+-------------+-----------+-----------+
	//        opc
	unsigned sf = test(ins, 31);
	unsigned S  = test(ins, 30);
	unsigned N  = test(ins, 22);
	if (sf == 0 && N != 0) {
		return false;
	}
	uint8_t immr = extract(ins, 0, 21, 16, 0);
	uint8_t imms = extract(ins, 0, 15, 10, 0);
	uint64_t wmask, tmask;
	if (!decode_bit_masks(sf, N, imms, immr, 1, &wmask, &tmask)) {
		return false;
	}
	and_orr_im->Rd  = get_reg(ins, sf, (S ? USE_ZR : USE_SP), 0);
	and_orr_im->Rn  = get_reg(ins, sf, USE_ZR, 5);
	and_orr_im->imm = lobits(wmask, (sf ? 64 : 32));
	return true;
}

static bool
decode_add_sub_sr(uint32_t ins, struct aarch64_add_sub_sr *add_sub_sr) {
	//  31   30  29  28       24 23   22 21  20       16 15         10 9         5 4         0
	// +----+---+---+-----------+-------+---+-----------+-------------+-----------+-----------+
	// | sf | 0 | 0 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ADD shifted register
	// | sf | 0 | 1 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ADDS shifted register
	// | sf | 1 | 0 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | SUB shifted register
	// | sf | 1 | 1 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | SUBS shifted register
	// +----+---+---+-----------+-------+---+-----------+-------------+-----------+-----------+
	//       op   S
	unsigned sf         = test(ins, 31);
	aarch64_shift shift = get_shift(ins, 22);
	unsigned amount     = extract(ins, 0, 15, 10, 0);
	if (shift == AARCH64_SHIFT_ROR) {
		return false;
	}
	if (sf == 0 && test(amount, 5)) {
		return false;
	}
	add_sub_sr->Rd     = get_reg(ins, sf, USE_ZR, 0);
	add_sub_sr->Rn     = get_reg(ins, sf, USE_ZR, 5);
	add_sub_sr->Rm     = get_reg(ins, sf, USE_ZR, 16);
	add_sub_sr->shift  = shift;
	add_sub_sr->amount = amount;
	return true;
}

static bool
decode_and_orr_sr(uint32_t ins, struct aarch64_and_orr_sr *and_orr_sr) {
	//  31   30 29 28       24 23   22 21  20       16 15         10 9         5 4         0
	// +----+-----+-----------+-------+---+-----------+-------------+-----------+-----------+
	// | sf | 0 0 | 0 1 0 1 0 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | AND shifted register
	// | sf | 1 1 | 0 1 0 1 0 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ANDS shifted register
	// | sf | 0 1 | 0 1 0 1 0 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ORR shifted register
	// +----+-----+-----------+-------+---+-----------+-------------+-----------+-----------+
	//        opc                       N
	unsigned sf     = test(ins, 31);
	unsigned amount = extract(ins, 0, 15, 10, 0);
	if (sf == 0 && test(amount, 5)) {
		return false;
	}
	and_orr_sr->Rd     = get_reg(ins, sf, USE_ZR, 0);
	and_orr_sr->Rn     = get_reg(ins, sf, USE_ZR, 5);
	and_orr_sr->Rm     = get_reg(ins, sf, USE_ZR, 16);
	and_orr_sr->shift  = get_shift(ins, 22);
	and_orr_sr->amount = amount;
	return true;
}

static bool
decode_adr_adrp(uint32_t ins, uint64_t pc, struct aarch64_adr_adrp *adr_adrp) {
	//  31  30   29 28       24 23                                    5 4         0
	// +---+-------+-----------+---------------------------------------+-----------+
	// | 0 | immlo | 1 0 0 0 0 |                 immhi                 |    Rd     | ADR
	// | 1 | immlo | 1 0 0 0 0 |                 immhi                 |    Rd     | ADRP
	// +---+-------+-----------+---------------------------------------+-----------+
	//  op
	unsigned shift  = 12 * test(ins, 31);
	int64_t imm     = extract(ins, 1, 23, 5, shift + 2) | extract(ins, 0, 30, 29, shift);
	adr_adrp->Xd    = get_reg(ins, 1, USE_ZR, 0);
	adr_adrp->label = (pc & ~ones(shift)) + imm;
	return true;
}

static bool
decode_b_bl(uint32_t ins, uint64_t pc, struct aarch64_b_bl *b_bl) {
	//  31  30       26 25                                                  0
	// +---+-----------+-----------------------------------------------------+
	// | 0 | 0 0 1 0 1 |                        imm26                        | B
	// | 1 | 0 0 1 0 1 |                        imm26                        | BL
	// +---+-----------+-----------------------------------------------------+
	//  op
	b_bl->label = pc + extract(ins, 1, 25, 0, 2);
	return true;
}

static bool
decode_br_blr(uint32_t ins, struct aarch64_br_blr *br_blr) {
	//  31           25 24 23 22 21 20       16 15         10 9         5 4         0
	// +---------------+-----+-----+-----------+-------------+-----------+-----------+
	// | 1 1 0 1 0 1 1 | 0 0 | 0 1 | 1 1 1 1 1 | 0 0 0 0 0 0 |    Rn     | 0 0 0 0 0 | BLR
	// | 1 1 0 1 0 1 1 | 0 0 | 0 0 | 1 1 1 1 1 | 0 0 0 0 0 0 |    Rn     | 0 0 0 0 0 | BR
	// +---------------+-----+-----+-----------+-------------+-----------+-----------+
	//                         op
	br_blr->Xn = get_reg(ins, 1, USE_ZR, 5);
	return true;
}

static bool
decode_ldp_stp(uint32_t ins, struct aarch64_ldp_stp *ldp_stp) {
	//  31 30 29   27 26  25   23 22  21           15 14       10 9         5 4         0
	// +-----+-------+---+-------+---+---------------+-----------+-----------+-----------+
	// | x 0 | 1 0 1 | 0 | 0 0 1 | 1 |     imm7      |    Rt2    |    Rn     |    Rt1    | LDP, post-index
	// | x 0 | 1 0 1 | 0 | 0 1 1 | 1 |     imm7      |    Rt2    |    Rn     |    Rt1    | LDP, pre-index
	// | x 0 | 1 0 1 | 0 | 0 1 0 | 1 |     imm7      |    Rt2    |    Rn     |    Rt1    | LDP, signed offset
	// | x 0 | 1 0 1 | 0 | 0 0 1 | 0 |     imm7      |    Rt2    |    Rn     |    Rt1    | STP, post-index
	// | x 0 | 1 0 1 | 0 | 0 1 1 | 0 |     imm7      |    Rt2    |    Rn     |    Rt1    | STP, pre-index
	// | x 0 | 1 0 1 | 0 | 0 0 1 | 0 |     imm7      |    Rt2    |    Rn     |    Rt1    | STP, signed offset
	// +-----+-------+---+-------+---+---------------+-----------+-----------+-----------+
	//   opc                       L
	unsigned sf  = test(ins, 31);
	ldp_stp->Rt1 = get_reg(ins, sf, USE_ZR, 0);
	ldp_stp->Xn  = get_reg(ins, 1, USE_SP, 5);
	ldp_stp->Rt2 = get_reg(ins, sf, USE_ZR, 10);
	ldp_stp->imm = extract(ins, 1, 21, 15, 2 + sf);
	return true;
}

static bool
decode_movknz(uint32_t ins, struct aarch64_movknz *movknz) {
	//  31   30 29 28         23 22 21 20                              5 4         0
	// +----+-----+-------------+-----+---------------------------------+-----------+
	// | sf | 1 1 | 1 0 0 1 0 1 | hw  |              imm16              |    Rd     | MOVK
	// | sf | 0 0 | 1 0 0 1 0 1 | hw  |              imm16              |    Rd     | MOVN
	// | sf | 1 0 | 1 0 0 1 0 1 | hw  |              imm16              |    Rd     | MOVZ
	// +----+-----+-------------+-----+---------------------------------+-----------+
	//        opc
	unsigned sf = test(ins, 31);
	unsigned hw = extract(ins, 0, 22, 21, 0);
	if (sf == 0 && test(hw, 1)) {
		return false;
	}
	movknz->Rd    = get_reg(ins, sf, USE_ZR, 0);
	movknz->imm   = extract(ins, 0, 20, 5, 0);
	movknz->shift = 16 * hw;
	return true;
}

static bool
decode_ldr_str_ix(uint32_t ins, struct aarch64_ldr_str_ix *ldr_str_ix) {
	//  31 30 29   27 26  25 24 23 22 21  20               12 11 10 9         5 4         0
	// +-----+-------+---+-----+-----+---+-------------------+-----+-----------+-----------+
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 1 | 0 |       imm9        | 0 1 |    Rn     |    Rt     | LDR immediate, post-index
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 1 | 0 |       imm9        | 1 1 |    Rn     |    Rt     | LDR immedate, pre-index
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 0 | 0 |       imm9        | 0 1 |    Rn     |    Rt     | STR immediate, post-index
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 0 | 0 |       imm9        | 1 1 |    Rn     |    Rt     | STR immedate, pre-index
	// +-----+-------+---+-----+-----+---+-------------------+-----+-----------+-----------+
	//  size                     opc
	unsigned sf     = test(ins, 30);
	ldr_str_ix->Rt  = get_reg(ins, sf, USE_ZR, 0);
	ldr_str_ix->Xn  = get_reg(ins, 1, USE_SP, 5);
	ldr_str_ix->imm = extract(ins, 1, 20, 12, 0);
	return true;
}

static bool
decode_ldr_str_ui(uint32_t ins, struct aarch64_ldr_str_ui *ldr_str_ui) {
	//  31 30 29   27 26  25 24 23 22 21                     10 9         5 4         0
	// +-----+-------+---+-----+-----+-------------------------+-----------+-----------+
	// | 1 x | 1 1 1 | 0 | 0 1 | 0 1 |          imm12          |    Rn     |    Rt     | LDR unsigned offset
	// | 1 x | 1 1 1 | 0 | 0 1 | 0 0 |          imm12          |    Rn     |    Rt     | STR unsigned offset
	// +-----+-------+---+-----+-----+-------------------------+-----------+-----------+
	//  size                     opc
	unsigned size     = extract(ins, 0, 31, 30, 0);
	ldr_str_ui->Rt    = get_reg(ins, size & 1, USE_ZR, 0);
	ldr_str_ui->Xn    = get_reg(ins, 1, USE_SP, 5);
	ldr_str_ui->imm   = extract(ins, 0, 21, 10, size);
	return true;
}

static bool
decode_ldr_str_r(uint32_t ins, struct aarch64_ldr_str_r *ldr_str_r) {
	//  31 30 29   27 26  25 24 23 22 21  20       16 15    13 12  11 10 9         5 4         0
	// +-----+-------+---+-----+-----+---+-----------+--------+---+-----+-----------+-----------+
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 1 | 1 |    Rm     | option | S | 1 0 |    Rn     |    Rt     | LDR register
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 0 | 1 |    Rm     | option | S | 1 0 |    Rn     |    Rt     | STR register
	// +-----+-------+---+-----+-----+---+-----------+--------+---+-----+-----------+-----------+
	//  size                     opc
	aarch64_extend extend = get_extend(ins, 13);
	if (!test(extend, 1)) {
		return false;
	}
	if (extend == AARCH64_EXTEND_UXTX) {
		extend |= AARCH64_EXTEND_LSL;
	}
	unsigned size = extract(ins, 0, 31, 30, 0);
	ldr_str_r->Rt     = get_reg(ins, size & 1, USE_ZR, 0);
	ldr_str_r->Xn     = get_reg(ins, 1, USE_SP, 5);
	ldr_str_r->Rm     = get_reg(ins, extend & 1, USE_ZR, 16);
	ldr_str_r->extend = extend;
	ldr_str_r->amount = test(ins, 12) * size;
	return true;
}

// Disassembly

bool
aarch64_decode_adc(uint32_t ins, struct aarch64_adc_sbc *adc) {
	if (!AARCH64_INS_TYPE(ins, ADC)) {
		return false;
	}
	return decode_adc_sbc(ins, adc);
}

bool
aarch64_decode_adcs(uint32_t ins, struct aarch64_adc_sbc *adcs) {
	if (!AARCH64_INS_TYPE(ins, ADCS)) {
		return false;
	}
	return decode_adc_sbc(ins, adcs);
}

bool
aarch64_decode_add_xr(uint32_t ins, struct aarch64_add_sub_xr *add_xr){
	if (!AARCH64_INS_TYPE(ins, ADD_XR)) {
		return false;
	}
	return decode_add_sub_xr(ins, add_xr);
}

bool
aarch64_decode_add_im(uint32_t ins, struct aarch64_add_sub_im *add_im) {
	if (!AARCH64_INS_TYPE(ins, ADD_IM)) {
		return false;
	}
	return decode_add_sub_im(ins, add_im);
}

bool aarch64_decode_add_sr(uint32_t ins, struct aarch64_add_sub_sr *add_sr) {
	if (!AARCH64_INS_TYPE(ins, ADD_SR)) {
		return false;
	}
	return decode_add_sub_sr(ins, add_sr);
}

bool
aarch64_decode_adds_xr(uint32_t ins, struct aarch64_add_sub_xr *adds_xr) {
	if (!AARCH64_INS_TYPE(ins, ADDS_XR)) {
		return false;
	}
	return decode_add_sub_xr(ins, adds_xr);
}

bool
aarch64_decode_adds_im(uint32_t ins, struct aarch64_add_sub_im *adds_im) {
	if (!AARCH64_INS_TYPE(ins, ADDS_IM)) {
		return false;
	}
	return decode_add_sub_im(ins, adds_im);
}

bool
aarch64_decode_adds_sr(uint32_t ins, struct aarch64_add_sub_sr *adds_sr) {
	if (!AARCH64_INS_TYPE(ins, ADDS_SR)) {
		return false;
	}
	return decode_add_sub_sr(ins, adds_sr);
}

bool
aarch64_decode_adr(uint32_t ins, uint64_t pc, struct aarch64_adr_adrp *adr) {
	if (!AARCH64_INS_TYPE(ins, ADR)) {
		return false;
	}
	return decode_adr_adrp(ins, pc, adr);
}

bool
aarch64_decode_adrp(uint32_t ins, uint64_t pc, struct aarch64_adr_adrp *adrp) {
	if (!AARCH64_INS_TYPE(ins, ADRP)) {
		return false;
	}
	return decode_adr_adrp(ins, pc, adrp);
}

bool
aarch64_decode_and_im(uint32_t ins, struct aarch64_and_orr_im *and_im) {
	if (!AARCH64_INS_TYPE(ins, AND_IM)) {
		return false;
	}
	return decode_and_orr_im(ins, and_im);
}

bool
aarch64_decode_and_sr(uint32_t ins, struct aarch64_and_orr_sr *and_sr) {
	if (!AARCH64_INS_TYPE(ins, AND_SR)) {
		return false;
	}
	return decode_and_orr_sr(ins, and_sr);
}

bool
aarch64_decode_ands_im(uint32_t ins, struct aarch64_and_orr_im *ands_im) {
	if (!AARCH64_INS_TYPE(ins, ANDS_IM)) {
		return false;
	}
	return decode_and_orr_im(ins, ands_im);
}

bool
aarch64_decode_ands_sr(uint32_t ins, struct aarch64_and_orr_sr *ands_sr) {
	if (!AARCH64_INS_TYPE(ins, ANDS_SR)) {
		return false;
	}
	return decode_and_orr_sr(ins, ands_sr);
}

bool
aarch64_decode_b(uint32_t ins, uint64_t pc, struct aarch64_b_bl *b) {
	if (!AARCH64_INS_TYPE(ins, B)) {
		return false;
	}
	return decode_b_bl(ins, pc, b);
}

bool
aarch64_decode_bl(uint32_t ins, uint64_t pc, struct aarch64_b_bl *bl) {
	if (!AARCH64_INS_TYPE(ins, BL)) {
		return false;
	}
	return decode_b_bl(ins, pc, bl);
}

bool
aarch64_decode_blr(uint32_t ins, struct aarch64_br_blr *blr) {
	if (!AARCH64_INS_TYPE(ins, BLR)) {
		return false;
	}
	return decode_br_blr(ins, blr);
}

bool
aarch64_decode_br(uint32_t ins, struct aarch64_br_blr *br) {
	if (!AARCH64_INS_TYPE(ins, BR)) {
		return false;
	}
	return decode_br_blr(ins, br);
}

bool
aarch64_alias_cmn_xr(struct aarch64_add_sub_xr *adds_xr) {
	// CMN extended register : ADDS extended register
	// Preferred when Rd == '11111'
	return reg_is_zrsp(adds_xr->Rd);
}

bool
aarch64_alias_cmn_im(struct aarch64_add_sub_im *adds_im) {
	// CMN immediate : ADDS immediate
	// Preferred when Rd == '11111'
	return reg_is_zrsp(adds_im->Rd);
}

bool
aarch64_alias_cmn_sr(struct aarch64_add_sub_sr *adds_sr) {
	// CMN shifted register : ADDS shifted register
	// Preferred when Rd == '11111'
	return reg_is_zrsp(adds_sr->Rd);
}

bool
aarch64_alias_cmp_xr(struct aarch64_add_sub_xr *subs_xr) {
	// CMP extended register : SUBS extended register
	// Preferred when Rd == '11111'
	return reg_is_zrsp(subs_xr->Rd);
}

bool
aarch64_alias_cmp_im(struct aarch64_add_sub_im *subs_im) {
	// CMP immediate : SUBS immediate
	// Preferred when Rd == '11111'
	return reg_is_zrsp(subs_im->Rd);
}

bool
aarch64_alias_cmp_sr(struct aarch64_add_sub_sr *subs_sr) {
	// CMP shifted register : SUBS shifted register
	// Preferred when Rd == '11111'
	return reg_is_zrsp(subs_sr->Rd);
}

bool
aarch64_decode_ldp_post(uint32_t ins, struct aarch64_ldp_stp *ldp_post) {
	if (!AARCH64_INS_TYPE(ins, LDP_POST)) {
		return false;
	}
	return decode_ldp_stp(ins, ldp_post);
}

bool
aarch64_decode_ldp_pre(uint32_t ins, struct aarch64_ldp_stp *ldp_pre) {
	if (!AARCH64_INS_TYPE(ins, LDP_PRE)) {
		return false;
	}
	return decode_ldp_stp(ins, ldp_pre);
}

bool
aarch64_decode_ldp_si(uint32_t ins, struct aarch64_ldp_stp *ldp_si) {
	if (!AARCH64_INS_TYPE(ins, LDP_SI)) {
		return false;
	}
	return decode_ldp_stp(ins, ldp_si);
}

bool
aarch64_decode_ldr_post(uint32_t ins, struct aarch64_ldr_str_ix *ldr_post) {
	if (!AARCH64_INS_TYPE(ins, LDR_POST)) {
		return false;
	}
	return decode_ldr_str_ix(ins, ldr_post);
}

bool
aarch64_decode_ldr_pre(uint32_t ins, struct aarch64_ldr_str_ix *ldr_pre) {
	if (!AARCH64_INS_TYPE(ins, LDR_PRE)) {
		return false;
	}
	return decode_ldr_str_ix(ins, ldr_pre);
}

bool
aarch64_decode_ldr_ui(uint32_t ins, struct aarch64_ldr_str_ui *ldr_ui) {
	if (!AARCH64_INS_TYPE(ins, LDR_UI)) {
		return false;
	}
	return decode_ldr_str_ui(ins, ldr_ui);
}

bool
aarch64_decode_ldr_lit(uint32_t ins, uint64_t pc, struct aarch64_ldr_lit *ldr_lit) {
	//  31 30 29   27 26  25 24 23                                    5 4         0
	// +-----+-------+---+-----+---------------------------------------+-----------+
	// | 0 x | 0 1 1 | 0 | 0 0 |                 imm19                 |    Rt     |
	// +-----+-------+---+-----+---------------------------------------+-----------+
	//   opc
	if (!AARCH64_INS_TYPE(ins, LDR_LIT)) {
		return false;
	}
	unsigned sf    = test(ins, 30);
	ldr_lit->Rt    = get_reg(ins, sf, USE_ZR, 0);
	ldr_lit->label = pc + extract(ins, 1, 23, 5, 2);
	return true;
}

bool
aarch64_decode_ldr_r(uint32_t ins, struct aarch64_ldr_str_r *ldr_r) {
	if (!AARCH64_INS_TYPE(ins, LDR_R)) {
		return false;
	}
	return decode_ldr_str_r(ins, ldr_r);
}

bool
aarch64_alias_mov_sp(struct aarch64_add_sub_im *add_im) {
	// MOV to/from SP : ADD immediate
	// Preferred when shift == '00' && imm12 == '00' && (Rd == '11111' || Rn == '11111')
	return ((reg_is_zrsp(add_im->Rd) || reg_is_zrsp(add_im->Rn))
	        && add_im->imm == 0
	        && add_im->shift == 0);
}

bool
aarch64_alias_mov_nwi(struct aarch64_movknz *movn) {
	// MOV inverted wide immediate : MOVN
	// Preferred when:
	//   32: !(IsZero(imm16) && hw != '00')
	//   64: !(IsZero(imm16) && hw != '00') && IsOnes(imm16)
	return !(movn->imm == 0 && movn->shift == 0)
	       && (AARCH64_REGSIZE(movn->Rd) == 32 || movn->imm == (uint16_t)-1);
}

bool
aarch64_alias_mov_wi(struct aarch64_movknz *movz) {
	// MOV wide immediate : MOVZ
	// Preferred when !(IsZero(imm16) && hw != '00')
	return (movz->imm != 0 || movz->shift == 0);
}

bool
aarch64_alias_mov_bi(struct aarch64_and_orr_im *orr_im) {
	// MOV bitmask immediate : ORR immediate
	// Preferred when Rn == '11111' && !MoveWidePreferred(sf, N, imms, immr)
	// TODO: Use MoveWidePreferred. It should be possible to use the conditions there to
	// establish conditions on orr_im->imm.
	return reg_is_zrsp(orr_im->Rn);
}

bool
aarch64_alias_mov_r(struct aarch64_and_orr_sr *orr_sr) {
	// MOV register : ORR shifted register
	// Preferred when shift == '00' && imm6 == '000000' && Rn == '11111'
	return (reg_is_zrsp(orr_sr->Rn)
	        && orr_sr->amount == 0
	        && orr_sr->shift == AARCH64_SHIFT_LSL);
}

bool
aarch64_decode_movk(uint32_t ins, struct aarch64_movknz *movk) {
	if (!AARCH64_INS_TYPE(ins, MOVK)) {
		return false;
	}
	return decode_movknz(ins, movk);
}

bool
aarch64_decode_movn(uint32_t ins, struct aarch64_movknz *movn) {
	if (!AARCH64_INS_TYPE(ins, MOVN)) {
		return false;
	}
	return decode_movknz(ins, movn);
}

bool
aarch64_decode_movz(uint32_t ins, struct aarch64_movknz *movz) {
	if (!AARCH64_INS_TYPE(ins, MOVZ)) {
		return false;
	}
	return decode_movknz(ins, movz);
}

bool
aarch64_alias_neg(struct aarch64_add_sub_sr *sub_sr) {
	// NEG shifted register : SUB shifted register
	// Preferred when Rn == '11111'
	return reg_is_zrsp(sub_sr->Rn);
}

bool
aarch64_alias_negs(struct aarch64_add_sub_sr *subs_sr) {
	// NEGS shifted register : SUBS shifted register
	// Preferred when Rn == '11111'
	return reg_is_zrsp(subs_sr->Rn);
}

bool
aarch64_alias_ngc(struct aarch64_adc_sbc *sbc) {
	// NGC : SBC
	// Preferred when Rn == '11111'
	return reg_is_zrsp(sbc->Rn);
}

bool
aarch64_alias_ngcs(struct aarch64_adc_sbc *sbcs) {
	// NGCS : SBCS
	// Preferred when Rn == '11111'
	return reg_is_zrsp(sbcs->Rn);
}

bool
aarch64_decode_nop(uint32_t ins) {
	return AARCH64_INS_TYPE(ins, NOP);
}

bool
aarch64_decode_orr_im(uint32_t ins, struct aarch64_and_orr_im *orr_im) {
	if (!AARCH64_INS_TYPE(ins, ORR_IM)) {
		return false;
	}
	return decode_and_orr_im(ins, orr_im);
}

bool
aarch64_decode_orr_sr(uint32_t ins, struct aarch64_and_orr_sr *orr_sr) {
	if (!AARCH64_INS_TYPE(ins, ORR_SR)) {
		return false;
	}
	return decode_and_orr_sr(ins, orr_sr);
}

bool
aarch64_decode_ret(uint32_t ins, struct aarch64_ret *ret) {
	//  31           25 24 23 22 21 20       16 15         10 9         5 4         0
	// +---------------+-----+-----+-----------+-------------+-----------+-----------+
	// | 1 1 0 1 0 1 1 | 0 0 | 1 0 | 1 1 1 1 1 | 0 0 0 0 0 0 |    Rn     | 0 0 0 0 0 |
	// +---------------+-----+-----+-----------+-------------+-----------+-----------+
	//                         op
	if (!AARCH64_INS_TYPE(ins, RET)) {
		return false;
	}
	ret->Xn = get_reg(ins, 1, USE_ZR, 5);
	return true;
}

bool
aarch64_decode_sbc(uint32_t ins, struct aarch64_adc_sbc *sbc) {
	if (!AARCH64_INS_TYPE(ins, SBC)) {
		return false;
	}
	return decode_adc_sbc(ins, sbc);
}

bool
aarch64_decode_sbcs(uint32_t ins, struct aarch64_adc_sbc *sbcs) {
	if (!AARCH64_INS_TYPE(ins, SBCS)) {
		return false;
	}
	return decode_adc_sbc(ins, sbcs);
}

bool
aarch64_decode_stp_post(uint32_t ins, struct aarch64_ldp_stp *stp_post) {
	if (!AARCH64_INS_TYPE(ins, STP_POST)) {
		return false;
	}
	return decode_ldp_stp(ins, stp_post);
}

bool
aarch64_decode_stp_pre(uint32_t ins, struct aarch64_ldp_stp *stp_pre) {
	if (!AARCH64_INS_TYPE(ins, STP_PRE)) {
		return false;
	}
	return decode_ldp_stp(ins, stp_pre);
}

bool
aarch64_decode_stp_si(uint32_t ins, struct aarch64_ldp_stp *stp_si) {
	if (!AARCH64_INS_TYPE(ins, STP_SI)) {
		return false;
	}
	return decode_ldp_stp(ins, stp_si);
}

bool
aarch64_decode_str_post(uint32_t ins, struct aarch64_ldr_str_ix *str_post) {
	if (!AARCH64_INS_TYPE(ins, STR_POST)) {
		return false;
	}
	return decode_ldr_str_ix(ins, str_post);
}

bool
aarch64_decode_str_pre(uint32_t ins, struct aarch64_ldr_str_ix *str_pre) {
	if (!AARCH64_INS_TYPE(ins, STR_PRE)) {
		return false;
	}
	return decode_ldr_str_ix(ins, str_pre);
}

bool
aarch64_decode_str_ui(uint32_t ins, struct aarch64_ldr_str_ui *str_ui) {
	if (!AARCH64_INS_TYPE(ins, STR_UI)) {
		return false;
	}
	return decode_ldr_str_ui(ins, str_ui);
}

bool
aarch64_decode_str_r(uint32_t ins, struct aarch64_ldr_str_r *str_r) {
	if (!AARCH64_INS_TYPE(ins, STR_R)) {
		return false;
	}
	return decode_ldr_str_r(ins, str_r);
}

bool
aarch64_decode_sub_xr(uint32_t ins, struct aarch64_add_sub_xr *sub_xr) {
	if (!AARCH64_INS_TYPE(ins, SUB_XR)) {
		return false;
	}
	return decode_add_sub_xr(ins, sub_xr);
}

bool
aarch64_decode_sub_im(uint32_t ins, struct aarch64_add_sub_im *sub_im) {
	if (!AARCH64_INS_TYPE(ins, SUB_IM)) {
		return false;
	}
	return decode_add_sub_im(ins, sub_im);
}

bool
aarch64_decode_sub_sr(uint32_t ins, struct aarch64_add_sub_sr *sub_sr) {
	if (!AARCH64_INS_TYPE(ins, SUB_SR)) {
		return false;
	}
	return decode_add_sub_sr(ins, sub_sr);
}

bool
aarch64_decode_subs_xr(uint32_t ins, struct aarch64_add_sub_xr *subs_xr) {
	if (!AARCH64_INS_TYPE(ins, SUBS_XR)) {
		return false;
	}
	return decode_add_sub_xr(ins, subs_xr);
}

bool
aarch64_decode_subs_im(uint32_t ins, struct aarch64_add_sub_im *subs_im) {
	if (!AARCH64_INS_TYPE(ins, SUBS_IM)) {
		return false;
	}
	return decode_add_sub_im(ins, subs_im);
}

bool
aarch64_decode_subs_sr(uint32_t ins, struct aarch64_add_sub_sr *subs_sr) {
	if (!AARCH64_INS_TYPE(ins, SUBS_SR)) {
		return false;
	}
	return decode_add_sub_sr(ins, subs_sr);
}

bool
aarch64_alias_tst_im(struct aarch64_and_orr_im *ands_im) {
	// TST immediate : ANDS immediate
	// Preferred when Rd == '11111'
	return reg_is_zrsp(ands_im->Rd);
}

bool
aarch64_alias_tst_sr(struct aarch64_and_orr_sr *ands_sr) {
	// TST shifted register : ANDS shifted register
	// Preferred when Rd == '11111'
	return reg_is_zrsp(ands_sr->Rd);
}
