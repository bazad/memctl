#include "memctl/aarch64/disasm.h"

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
 * gpreg
 *
 * Description:
 * 	Returns the register number at the given index in the instruction.
 */
static inline aarch64_gpreg
gpreg(uint32_t ins, unsigned sf, unsigned zrsp, unsigned lo) {
	assert(sf == 0 || sf == 1);
	assert(zrsp == USE_ZR || zrsp == USE_SP);
	aarch64_gpreg reg     = ((ins >> lo) & 0x1f);
	aarch64_gpreg size    = (sf ? 0 : 32);
	aarch64_gpreg zr_hint = zrsp * (reg == AARCH64_SP);
	return zr_hint | size | reg;
}

/*
 * gpreg_is_zrsp
 *
 * Description:
 * 	Returns true if the given register has the same name as the ZR or SP register.
 */
static inline bool
gpreg_is_zrsp(aarch64_gpreg reg) {
	return AARCH64_GPREGID(reg) == AARCH64_SP;
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

bool
aarch64_decode_adc(uint32_t ins, struct aarch64_ins_adc *adc) {
	//  31   30  29  28             21 20       16 15         10 9         5 4         0
	// +----+---+---+-----------------+-----------+-------------+-----------+-----------+
	// | sf | 0 | 0 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | ADC
	// | sf | 0 | 1 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | ADCS
	// | sf | 1 | 0 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | SBC
	// | sf | 1 | 1 | 1 1 0 1 0 0 0 0 |    Rm     | 0 0 0 0 0 0 |    Rn     |    Rd     | SBCS
	// +----+---+---+-----------------+-----------+-------------+-----------+-----------+
	//       op   S
	if (!AARCH64_INS_TYPE(ins, ADC_CLASS)) {
		return false;
	}
	unsigned sf   = test(ins, 31);
	adc->op       = test(ins, 30);
	adc->setflags = test(ins, 29);
	adc->Rd       = gpreg(ins, sf, USE_ZR, 0);
	adc->Rn       = gpreg(ins, sf, USE_ZR, 5);
	adc->Rm       = gpreg(ins, sf, USE_ZR, 16);
	return true;
}

bool
aarch64_decode_add_xr(uint32_t ins, struct aarch64_ins_add_xr *add_xr) {
	//  31   30  29  28       24 23 22 21  20       16 15    13 12   10 9         5 4         0
	// +----+---+---+-----------+-----+---+-----------+--------+-------+-----------+-----------+
	// | sf | 0 | 0 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | ADD extended register
	// | sf | 0 | 1 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | ADDS extended register
	// | sf | 1 | 0 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | SUB extended register
	// | sf | 1 | 1 | 0 1 0 1 1 | 0 0 | 1 |    Rm     | option | imm3  |    Rn     |    Rd     | SUBS extended register
	// +----+---+---+-----------+-----+---+-----------+--------+-------+-----------+-----------+
	//       op   S
	if (!AARCH64_INS_TYPE(ins, ADD_XR_CLASS)) {
		return false;
	}
	unsigned shift = extract(ins, 0, 12, 10, 0);
	if (shift > 4) {
		return false;
	}
	unsigned sf           = test(ins, 31);
	unsigned S            = test(ins, 29);
	aarch64_extend extend = get_extend(ins, 13);
	unsigned Xm           = (extend & 0x3) == 0x3;
	aarch64_gpreg Rd      = gpreg(ins, sf, (S ? USE_ZR : USE_SP), 0);
	aarch64_gpreg Rn      = gpreg(ins, sf, USE_SP, 5);
	if (((!S && gpreg_is_zrsp(Rd)) || gpreg_is_zrsp(Rn)) && extend == (AARCH64_EXTEND_UXTW + sf)) {
		extend |= AARCH64_EXTEND_LSL;
	}
	add_xr->op       = test(ins, 30);
	add_xr->setflags = S;
	add_xr->Rd       = Rd;
	add_xr->Rn       = Rn;
	add_xr->Rm       = gpreg(ins, Xm, USE_ZR, 16);
	add_xr->extend   = extend;
	add_xr->amount   = shift;
	return true;
}

bool
aarch64_decode_add_im(uint32_t ins, struct aarch64_ins_add_im *add_im) {
	//  31   30  29  28       24 23   22 21                     10 9         5 4         0
	// +----+---+---+-----------+-------+-------------------------+-----------+-----------+
	// | sf | 0 | 0 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | ADD immediate
	// | sf | 0 | 1 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | ADDS immediate
	// | sf | 1 | 0 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | SUB immediate
	// | sf | 1 | 1 | 1 0 0 0 1 | shift |          imm12          |    Rn     |    Rd     | SUBS immediate
	// +----+---+---+-----------+-------+-------------------------+-----------+-----------+
	//       op   S
	if (!AARCH64_INS_TYPE(ins, ADD_IM_CLASS)) {
		return false;
	}
	unsigned sf    = test(ins, 31);
	unsigned S     = test(ins, 29);
	unsigned shift = extract(ins, 0, 23, 22, 0);
	uint16_t imm   = extract(ins, 0, 21, 10, 0);
	if (test(shift, 1)) {
		return false;
	}
	add_im->op       = test(ins, 30);
	add_im->setflags = test(ins, 29);
	add_im->Rd       = gpreg(ins, sf, (S ? USE_ZR : USE_SP), 0);
	add_im->Rn       = gpreg(ins, sf, USE_SP, 5);
	add_im->imm      = imm;
	add_im->shift    = 12 * shift;
	return true;
}

bool
aarch64_decode_and_im(uint32_t ins, struct aarch64_ins_and_im *and_im) {
	//  31   30 29 28         23 22  21         16 15         10 9         5 4         0
	// +----+-----+-------------+---+-------------+-------------+-----------+-----------+
	// | sf | 0 0 | 1 0 0 1 0 0 | N |    immr     |    imms     |    Rn     |    Rd     | AND immediate
	// | sf | 1 1 | 1 0 0 1 0 0 | N |    immr     |    imms     |    Rn     |    Rd     | ANDS immediate
	// | sf | 0 1 | 1 0 0 1 0 0 | N |    immr     |    imms     |    Rn     |    Rd     | ORR immediate
	// +----+-----+-------------+---+-------------+-------------+-----------+-----------+
	//        opc
	if (!AARCH64_INS_TYPE(ins, AND_IM_CLASS)) {
		return false;
	}
	unsigned sf  = test(ins, 31);
	unsigned S   = test(ins, 30);
	unsigned opc = extract(ins, 0, 30, 29, 0);
	unsigned N   = test(ins, 22);
	if (opc == 2 || (sf == 0 && N != 0)) {
		return false;
	}
	uint8_t immr = extract(ins, 0, 21, 16, 0);
	uint8_t imms = extract(ins, 0, 15, 10, 0);
	uint64_t wmask, tmask;
	if (!decode_bit_masks(sf, N, imms, immr, 1, &wmask, &tmask)) {
		return false;
	}
	and_im->op       = (opc == 1);
	and_im->setflags = (opc == 3);
	and_im->Rd       = gpreg(ins, sf, (S ? USE_ZR : USE_SP), 0);
	and_im->Rn       = gpreg(ins, sf, USE_ZR, 5);
	and_im->imm      = lobits(wmask, (sf ? 64 : 32));
	return true;
}

bool
aarch64_decode_add_sr(uint32_t ins, struct aarch64_ins_add_sr *add_sr) {
	//  31   30  29  28       24 23   22 21  20       16 15         10 9         5 4         0
	// +----+---+---+-----------+-------+---+-----------+-------------+-----------+-----------+
	// | sf | 0 | 0 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ADD shifted register
	// | sf | 0 | 1 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ADDS shifted register
	// | sf | 1 | 0 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | SUB shifted register
	// | sf | 1 | 1 | 0 1 0 1 1 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | SUBS shifted register
	// +----+---+---+-----------+-------+---+-----------+-------------+-----------+-----------+
	//       op   S
	if (!AARCH64_INS_TYPE(ins, ADD_SR_CLASS)) {
		return false;
	}
	unsigned sf         = test(ins, 31);
	aarch64_shift shift = get_shift(ins, 22);
	unsigned amount     = extract(ins, 0, 15, 10, 0);
	if (shift == AARCH64_SHIFT_ROR) {
		return false;
	}
	if (sf == 0 && test(amount, 5)) {
		return false;
	}
	add_sr->op       = test(ins, 30);
	add_sr->setflags = test(ins, 29);
	add_sr->Rd       = gpreg(ins, sf, USE_ZR, 0);
	add_sr->Rn       = gpreg(ins, sf, USE_ZR, 5);
	add_sr->Rm       = gpreg(ins, sf, USE_ZR, 16);
	add_sr->shift    = shift;
	add_sr->amount   = amount;
	return true;
}

bool
aarch64_decode_and_sr(uint32_t ins, struct aarch64_ins_and_sr *and_sr) {
	//  31   30 29 28       24 23   22 21  20       16 15         10 9         5 4         0
	// +----+-----+-----------+-------+---+-----------+-------------+-----------+-----------+
	// | sf | 0 0 | 0 1 0 1 0 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | AND shifted register
	// | sf | 1 1 | 0 1 0 1 0 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ANDS shifted register
	// | sf | 0 1 | 0 1 0 1 0 | shift | 0 |    Rm     |    imm6     |    Rn     |    Rd     | ORR shifted register
	// +----+-----+-----------+-------+---+-----------+-------------+-----------+-----------+
	//        opc                       N
	if (!AARCH64_INS_TYPE(ins, AND_SR_CLASS)) {
		return false;
	}
	uint8_t opc = extract(ins, 0, 30, 29, 0);
	if (opc == 2) {
		return false;
	}
	unsigned sf     = test(ins, 31);
	unsigned amount = extract(ins, 0, 15, 10, 0);
	if (sf == 0 && test(amount, 5)) {
		return false;
	}
	and_sr->op       = (opc == 1);
	and_sr->setflags = (opc == 3);
	and_sr->Rd       = gpreg(ins, sf, USE_ZR, 0);
	and_sr->Rn       = gpreg(ins, sf, USE_ZR, 5);
	and_sr->Rm       = gpreg(ins, sf, USE_ZR, 16);
	and_sr->shift    = get_shift(ins, 22);
	and_sr->amount   = amount;
	return true;
}

bool
aarch64_decode_adr(uint32_t ins, uint64_t pc, struct aarch64_ins_adr *adr) {
	//  31  30   29 28       24 23                                    5 4         0
	// +---+-------+-----------+---------------------------------------+-----------+
	// | 0 | immlo | 1 0 0 0 0 |                 immhi                 |    Rd     | ADR
	// | 1 | immlo | 1 0 0 0 0 |                 immhi                 |    Rd     | ADRP
	// +---+-------+-----------+---------------------------------------+-----------+
	//  op
	if (!AARCH64_INS_TYPE(ins, ADR_CLASS)) {
		return false;
	}
	unsigned shift  = 12 * test(ins, 31);
	int64_t imm     = extract(ins, 1, 23, 5, shift + 2) | extract(ins, 0, 30, 29, shift);
	adr->op    = test(ins, 31);
	adr->Xd    = gpreg(ins, 1, USE_ZR, 0);
	adr->label = (pc & ~ones(shift)) + imm;
	return true;
}

bool
aarch64_decode_b(uint32_t ins, uint64_t pc, struct aarch64_ins_b *b) {
	//  31  30       26 25                                                  0
	// +---+-----------+-----------------------------------------------------+
	// | 0 | 0 0 1 0 1 |                        imm26                        | B
	// | 1 | 0 0 1 0 1 |                        imm26                        | BL
	// +---+-----------+-----------------------------------------------------+
	//  op
	if (!AARCH64_INS_TYPE(ins, B_CLASS)) {
		return false;
	}
	b->link  = test(ins, 31);
	b->label = pc + extract(ins, 1, 25, 0, 2);
	return true;
}

bool
aarch64_decode_br(uint32_t ins, struct aarch64_ins_br *br) {
	//  31           25 24 23 22 21 20       16 15         10 9         5 4         0
	// +---------------+-----+-----+-----------+-------------+-----------+-----------+
	// | 1 1 0 1 0 1 1 | 0 0 | 0 0 | 1 1 1 1 1 | 0 0 0 0 0 0 |    Rn     | 0 0 0 0 0 | BR
	// | 1 1 0 1 0 1 1 | 0 0 | 0 1 | 1 1 1 1 1 | 0 0 0 0 0 0 |    Rn     | 0 0 0 0 0 | BLR
	// | 1 1 0 1 0 1 1 | 0 0 | 1 0 | 1 1 1 1 1 | 0 0 0 0 0 0 |    Rn     | 0 0 0 0 0 | RET
	// +---------------+-----+-----+-----------+-------------+-----------+-----------+
	//                         op
	if (!AARCH64_INS_TYPE(ins, BR_CLASS)) {
		return false;
	}
	if (extract(ins, 0, 22, 21, 0) == 0x3) {
		return false;
	}
	br->op   = test(ins, 22);
	br->link = test(ins, 21);
	br->Xn   = gpreg(ins, 1, USE_ZR, 5);
	return true;
}

static bool
decode_ldp_stp(uint32_t ins, struct aarch64_ins_ldp_stp *ldp_stp) {
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
	ldp_stp->Rt1 = gpreg(ins, sf, USE_ZR, 0);
	ldp_stp->Xn  = gpreg(ins, 1, USE_SP, 5);
	ldp_stp->Rt2 = gpreg(ins, sf, USE_ZR, 10);
	ldp_stp->imm = extract(ins, 1, 21, 15, 2 + sf);
	return true;
}

static bool
decode_movknz(uint32_t ins, struct aarch64_ins_movknz *movknz) {
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
	movknz->Rd    = gpreg(ins, sf, USE_ZR, 0);
	movknz->imm   = extract(ins, 0, 20, 5, 0);
	movknz->shift = 16 * hw;
	return true;
}

static bool
decode_ldr_str_ix(uint32_t ins, struct aarch64_ins_ldr_str_ix *ldr_str_ix) {
	//  31 30 29   27 26  25 24 23 22 21  20               12 11 10 9         5 4         0
	// +-----+-------+---+-----+-----+---+-------------------+-----+-----------+-----------+
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 1 | 0 |       imm9        | 0 1 |    Rn     |    Rt     | LDR immediate, post-index
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 1 | 0 |       imm9        | 1 1 |    Rn     |    Rt     | LDR immedate, pre-index
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 0 | 0 |       imm9        | 0 1 |    Rn     |    Rt     | STR immediate, post-index
	// | 1 x | 1 1 1 | 0 | 0 0 | 0 0 | 0 |       imm9        | 1 1 |    Rn     |    Rt     | STR immedate, pre-index
	// +-----+-------+---+-----+-----+---+-------------------+-----+-----------+-----------+
	//  size                     opc
	unsigned sf     = test(ins, 30);
	ldr_str_ix->Rt  = gpreg(ins, sf, USE_ZR, 0);
	ldr_str_ix->Xn  = gpreg(ins, 1, USE_SP, 5);
	ldr_str_ix->imm = extract(ins, 1, 20, 12, 0);
	return true;
}

static bool
decode_ldr_str_ui(uint32_t ins, struct aarch64_ins_ldr_str_ui *ldr_str_ui) {
	//  31 30 29   27 26  25 24 23 22 21                     10 9         5 4         0
	// +-----+-------+---+-----+-----+-------------------------+-----------+-----------+
	// | 1 x | 1 1 1 | 0 | 0 1 | 0 1 |          imm12          |    Rn     |    Rt     | LDR unsigned offset
	// | 1 x | 1 1 1 | 0 | 0 1 | 0 0 |          imm12          |    Rn     |    Rt     | STR unsigned offset
	// +-----+-------+---+-----+-----+-------------------------+-----------+-----------+
	//  size                     opc
	unsigned size     = extract(ins, 0, 31, 30, 0);
	ldr_str_ui->Rt    = gpreg(ins, size & 1, USE_ZR, 0);
	ldr_str_ui->Xn    = gpreg(ins, 1, USE_SP, 5);
	ldr_str_ui->imm   = extract(ins, 0, 21, 10, size);
	return true;
}

static bool
decode_ldr_str_r(uint32_t ins, struct aarch64_ins_ldr_str_r *ldr_str_r) {
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
	ldr_str_r->Rt     = gpreg(ins, size & 1, USE_ZR, 0);
	ldr_str_r->Xn     = gpreg(ins, 1, USE_SP, 5);
	ldr_str_r->Rm     = gpreg(ins, extend & 1, USE_ZR, 16);
	ldr_str_r->extend = extend;
	ldr_str_r->amount = test(ins, 12) * size;
	return true;
}

// Disassembly

bool
aarch64_alias_cmn_xr(struct aarch64_ins_add_xr *adds_xr) {
	// CMN extended register : ADDS extended register
	// Preferred when Rd == '11111'
	return (adds_xr->op == AARCH64_INS_ADD_XR_OP_ADD
	        && adds_xr->setflags
	        && gpreg_is_zrsp(adds_xr->Rd));
}

bool
aarch64_alias_cmn_im(struct aarch64_ins_add_im *adds_im) {
	// CMN immediate : ADDS immediate
	// Preferred when Rd == '11111'
	return (adds_im->op == AARCH64_INS_ADD_IM_OP_ADD
	        && adds_im->setflags
	        && gpreg_is_zrsp(adds_im->Rd));
}

bool
aarch64_alias_cmn_sr(struct aarch64_ins_add_sr *adds_sr) {
	// CMN shifted register : ADDS shifted register
	// Preferred when Rd == '11111'
	return gpreg_is_zrsp(adds_sr->Rd);
}

bool
aarch64_alias_cmp_xr(struct aarch64_ins_add_xr *subs_xr) {
	// CMP extended register : SUBS extended register
	// Preferred when Rd == '11111'
	return (subs_xr->op == AARCH64_INS_ADD_XR_OP_SUB
	        && subs_xr->setflags
	        && gpreg_is_zrsp(subs_xr->Rd));
}

bool
aarch64_alias_cmp_im(struct aarch64_ins_add_im *subs_im) {
	// CMP immediate : SUBS immediate
	// Preferred when Rd == '11111'
	return (subs_im->op == AARCH64_INS_ADD_IM_OP_SUB
	        && subs_im->setflags
	        && gpreg_is_zrsp(subs_im->Rd));
}

bool
aarch64_alias_cmp_sr(struct aarch64_ins_add_sr *subs_sr) {
	// CMP shifted register : SUBS shifted register
	// Preferred when Rd == '11111'
	return gpreg_is_zrsp(subs_sr->Rd);
}

bool
aarch64_ins_decode_ldp_post(uint32_t ins, struct aarch64_ins_ldp_stp *ldp_post) {
	if (!AARCH64_INS_TYPE(ins, LDP_POST_INS)) {
		return false;
	}
	return decode_ldp_stp(ins, ldp_post);
}

bool
aarch64_ins_decode_ldp_pre(uint32_t ins, struct aarch64_ins_ldp_stp *ldp_pre) {
	if (!AARCH64_INS_TYPE(ins, LDP_PRE_INS)) {
		return false;
	}
	return decode_ldp_stp(ins, ldp_pre);
}

bool
aarch64_ins_decode_ldp_si(uint32_t ins, struct aarch64_ins_ldp_stp *ldp_si) {
	if (!AARCH64_INS_TYPE(ins, LDP_SI_INS)) {
		return false;
	}
	return decode_ldp_stp(ins, ldp_si);
}

bool
aarch64_ins_decode_ldr_post(uint32_t ins, struct aarch64_ins_ldr_str_ix *ldr_post) {
	if (!AARCH64_INS_TYPE(ins, LDR_POST_INS)) {
		return false;
	}
	return decode_ldr_str_ix(ins, ldr_post);
}

bool
aarch64_ins_decode_ldr_pre(uint32_t ins, struct aarch64_ins_ldr_str_ix *ldr_pre) {
	if (!AARCH64_INS_TYPE(ins, LDR_PRE_INS)) {
		return false;
	}
	return decode_ldr_str_ix(ins, ldr_pre);
}

bool
aarch64_ins_decode_ldr_ui(uint32_t ins, struct aarch64_ins_ldr_str_ui *ldr_ui) {
	if (!AARCH64_INS_TYPE(ins, LDR_UI_INS)) {
		return false;
	}
	return decode_ldr_str_ui(ins, ldr_ui);
}

bool
aarch64_ins_decode_ldr_lit(uint32_t ins, uint64_t pc, struct aarch64_ins_ldr_lit *ldr_lit) {
	//  31 30 29   27 26  25 24 23                                    5 4         0
	// +-----+-------+---+-----+---------------------------------------+-----------+
	// | 0 x | 0 1 1 | 0 | 0 0 |                 imm19                 |    Rt     |
	// +-----+-------+---+-----+---------------------------------------+-----------+
	//   opc
	if (!AARCH64_INS_TYPE(ins, LDR_LIT_INS)) {
		return false;
	}
	unsigned sf    = test(ins, 30);
	ldr_lit->Rt    = gpreg(ins, sf, USE_ZR, 0);
	ldr_lit->label = pc + extract(ins, 1, 23, 5, 2);
	return true;
}

bool
aarch64_ins_decode_ldr_r(uint32_t ins, struct aarch64_ins_ldr_str_r *ldr_r) {
	if (!AARCH64_INS_TYPE(ins, LDR_R_INS)) {
		return false;
	}
	return decode_ldr_str_r(ins, ldr_r);
}

bool
aarch64_alias_mov_sp(struct aarch64_ins_add_im *add_im) {
	// MOV to/from SP : ADD immediate
	// Preferred when shift == '00' && imm12 == '00' && (Rd == '11111' || Rn == '11111')
	return (add_im->op == AARCH64_INS_ADD_IM_OP_ADD
	        && !add_im->setflags
	        && (gpreg_is_zrsp(add_im->Rd) || gpreg_is_zrsp(add_im->Rn))
	        && add_im->imm == 0
	        && add_im->shift == 0);
}

bool
aarch64_alias_mov_nwi(struct aarch64_ins_movknz *movn) {
	// MOV inverted wide immediate : MOVN
	// Preferred when:
	//   32: !(IsZero(imm16) && hw != '00')
	//   64: !(IsZero(imm16) && hw != '00') && IsOnes(imm16)
	return !(movn->imm == 0 && movn->shift == 0)
	       && (AARCH64_GPREGSIZE(movn->Rd) == 32 || movn->imm == (uint16_t)-1);
}

bool
aarch64_alias_mov_wi(struct aarch64_ins_movknz *movz) {
	// MOV wide immediate : MOVZ
	// Preferred when !(IsZero(imm16) && hw != '00')
	return (movz->imm != 0 || movz->shift == 0);
}

bool
aarch64_alias_mov_bi(struct aarch64_ins_and_im *orr_im) {
	// MOV bitmask immediate : ORR immediate
	// Preferred when Rn == '11111' && !MoveWidePreferred(sf, N, imms, immr)
	// TODO: Use MoveWidePreferred. It should be possible to use the conditions there to
	// establish conditions on orr_im->imm.
	return (orr_im->op == AARCH64_INS_AND_IM_OP_ORR
	        && gpreg_is_zrsp(orr_im->Rn));
}

bool
aarch64_alias_mov_r(struct aarch64_ins_and_sr *orr_sr) {
	// MOV register : ORR shifted register
	// Preferred when shift == '00' && imm6 == '000000' && Rn == '11111'
	return (orr_sr->op == AARCH64_INS_AND_SR_OP_ORR
	        && gpreg_is_zrsp(orr_sr->Rn)
	        && orr_sr->amount == 0
	        && orr_sr->shift == AARCH64_SHIFT_LSL);
}

bool
aarch64_ins_decode_movk(uint32_t ins, struct aarch64_ins_movknz *movk) {
	if (!AARCH64_INS_TYPE(ins, MOVK_INS)) {
		return false;
	}
	return decode_movknz(ins, movk);
}

bool
aarch64_ins_decode_movn(uint32_t ins, struct aarch64_ins_movknz *movn) {
	if (!AARCH64_INS_TYPE(ins, MOVN_INS)) {
		return false;
	}
	return decode_movknz(ins, movn);
}

bool
aarch64_ins_decode_movz(uint32_t ins, struct aarch64_ins_movknz *movz) {
	if (!AARCH64_INS_TYPE(ins, MOVZ_INS)) {
		return false;
	}
	return decode_movknz(ins, movz);
}

bool
aarch64_alias_neg(struct aarch64_ins_add_sr *sub_sr) {
	// NEG shifted register : SUB shifted register
	// Preferred when Rn == '11111'
	return gpreg_is_zrsp(sub_sr->Rn);
}

bool
aarch64_alias_negs(struct aarch64_ins_add_sr *subs_sr) {
	// NEGS shifted register : SUBS shifted register
	// Preferred when Rn == '11111'
	return gpreg_is_zrsp(subs_sr->Rn);
}

bool
aarch64_alias_ngc(struct aarch64_ins_adc *sbc) {
	// NGC : SBC
	// Preferred when Rn == '11111'
	return (sbc->op == AARCH64_INS_ADC_OP_SBC
	        && !sbc->setflags
	        && gpreg_is_zrsp(sbc->Rn));
}

bool
aarch64_alias_ngcs(struct aarch64_ins_adc *sbcs) {
	// NGCS : SBCS
	// Preferred when Rn == '11111'
	return (sbcs->op == AARCH64_INS_ADC_OP_SBC
	        && sbcs->setflags
	        && gpreg_is_zrsp(sbcs->Rn));
}

bool
aarch64_decode_nop(uint32_t ins) {
	return AARCH64_INS_TYPE(ins, NOP_INS);
}

bool
aarch64_ins_decode_stp_post(uint32_t ins, struct aarch64_ins_ldp_stp *stp_post) {
	if (!AARCH64_INS_TYPE(ins, STP_POST_INS)) {
		return false;
	}
	return decode_ldp_stp(ins, stp_post);
}

bool
aarch64_ins_decode_stp_pre(uint32_t ins, struct aarch64_ins_ldp_stp *stp_pre) {
	if (!AARCH64_INS_TYPE(ins, STP_PRE_INS)) {
		return false;
	}
	return decode_ldp_stp(ins, stp_pre);
}

bool
aarch64_ins_decode_stp_si(uint32_t ins, struct aarch64_ins_ldp_stp *stp_si) {
	if (!AARCH64_INS_TYPE(ins, STP_SI_INS)) {
		return false;
	}
	return decode_ldp_stp(ins, stp_si);
}

bool
aarch64_ins_decode_str_post(uint32_t ins, struct aarch64_ins_ldr_str_ix *str_post) {
	if (!AARCH64_INS_TYPE(ins, STR_POST_INS)) {
		return false;
	}
	return decode_ldr_str_ix(ins, str_post);
}

bool
aarch64_ins_decode_str_pre(uint32_t ins, struct aarch64_ins_ldr_str_ix *str_pre) {
	if (!AARCH64_INS_TYPE(ins, STR_PRE_INS)) {
		return false;
	}
	return decode_ldr_str_ix(ins, str_pre);
}

bool
aarch64_ins_decode_str_ui(uint32_t ins, struct aarch64_ins_ldr_str_ui *str_ui) {
	if (!AARCH64_INS_TYPE(ins, STR_UI_INS)) {
		return false;
	}
	return decode_ldr_str_ui(ins, str_ui);
}

bool
aarch64_ins_decode_str_r(uint32_t ins, struct aarch64_ins_ldr_str_r *str_r) {
	if (!AARCH64_INS_TYPE(ins, STR_R_INS)) {
		return false;
	}
	return decode_ldr_str_r(ins, str_r);
}

bool
aarch64_alias_tst_im(struct aarch64_ins_and_im *ands_im) {
	// TST immediate : ANDS immediate
	// Preferred when Rd == '11111'
	return (ands_im->op == AARCH64_INS_AND_IM_OP_AND
	        && ands_im->setflags
	        && gpreg_is_zrsp(ands_im->Rd));
}

bool
aarch64_alias_tst_sr(struct aarch64_ins_and_sr *ands_sr) {
	// TST shifted register : ANDS shifted register
	// Preferred when Rd == '11111'
	return (ands_sr->op == AARCH64_INS_AND_SR_OP_AND
	        && ands_sr->setflags
	        && gpreg_is_zrsp(ands_sr->Rd));
}
