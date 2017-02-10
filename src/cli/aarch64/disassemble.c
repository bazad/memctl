#include "cli/disassemble.h"

#include "aarch64/disasm.h"

#include "utility.h"

#include <assert.h>
#include <stdio.h>

/*
 * reg
 *
 * Description:
 * 	Returns a string representation of the given register.
 */
static const char *
reg(aarch64_reg r) {
	switch (r) {
		case AARCH64_W0:		return "W0";
		case AARCH64_W1:		return "W1";
		case AARCH64_W2:		return "W2";
		case AARCH64_W3:		return "W3";
		case AARCH64_W4:		return "W4";
		case AARCH64_W5:		return "W5";
		case AARCH64_W6:		return "W6";
		case AARCH64_W7:		return "W7";
		case AARCH64_W8:		return "W8";
		case AARCH64_W9:		return "W9";
		case AARCH64_W10:		return "W10";
		case AARCH64_W11:		return "W11";
		case AARCH64_W12:		return "W12";
		case AARCH64_W13:		return "W13";
		case AARCH64_W14:		return "W14";
		case AARCH64_W15:		return "W15";
		case AARCH64_W16:		return "W16";
		case AARCH64_W17:		return "W17";
		case AARCH64_W18:		return "W18";
		case AARCH64_W19:		return "W19";
		case AARCH64_W20:		return "W20";
		case AARCH64_W21:		return "W21";
		case AARCH64_W22:		return "W22";
		case AARCH64_W23:		return "W23";
		case AARCH64_W24:		return "W24";
		case AARCH64_W25:		return "W25";
		case AARCH64_W26:		return "W26";
		case AARCH64_W27:		return "W27";
		case AARCH64_W28:		return "W28";
		case AARCH64_W29:		return "W29";
		case AARCH64_W30:		return "W30";
		case AARCH64_WSP:		return "WSP";
		case AARCH64_WZR:		return "WZR";
		case AARCH64_X0:		return "X0";
		case AARCH64_X1:		return "X1";
		case AARCH64_X2:		return "X2";
		case AARCH64_X3:		return "X3";
		case AARCH64_X4:		return "X4";
		case AARCH64_X5:		return "X5";
		case AARCH64_X6:		return "X6";
		case AARCH64_X7:		return "X7";
		case AARCH64_X8:		return "X8";
		case AARCH64_X9:		return "X9";
		case AARCH64_X10:		return "X10";
		case AARCH64_X11:		return "X11";
		case AARCH64_X12:		return "X12";
		case AARCH64_X13:		return "X13";
		case AARCH64_X14:		return "X14";
		case AARCH64_X15:		return "X15";
		case AARCH64_X16:		return "X16";
		case AARCH64_X17:		return "X17";
		case AARCH64_X18:		return "X18";
		case AARCH64_X19:		return "X19";
		case AARCH64_X20:		return "X20";
		case AARCH64_X21:		return "X21";
		case AARCH64_X22:		return "X22";
		case AARCH64_X23:		return "X23";
		case AARCH64_X24:		return "X24";
		case AARCH64_X25:		return "X25";
		case AARCH64_X26:		return "X26";
		case AARCH64_X27:		return "X27";
		case AARCH64_X28:		return "X28";
		case AARCH64_X29:		return "X29";
		case AARCH64_X30:		return "X30";
		case AARCH64_SP:		return "SP";
		case AARCH64_XZR:		return "XZR";
	}
	return "???";
}

/*
 * shift
 *
 * Description:
 * 	Returns a string representation of the given shift type.
 */
static const char *
shift(aarch64_shift s) {
	switch (s) {
		case AARCH64_SHIFT_LSL:		return "LSL";
		case AARCH64_SHIFT_LSR:		return "LSR";
		case AARCH64_SHIFT_ASR:		return "ASR";
		case AARCH64_SHIFT_ROR:		return "ROR";
	}
	return "???";
}

/*
 * extend
 *
 * Description:
 * 	Returns a string representation of the given extension type.
 */
static const char *
extend(aarch64_extend e) {
	switch (e) {
		case AARCH64_EXTEND_UXTB:	return "UXTB";
		case AARCH64_EXTEND_UXTH:	return "UXTH";
		case AARCH64_EXTEND_UXTW:	return "UXTW";
		case AARCH64_EXTEND_UXTX:	return "UXTX";
		case AARCH64_EXTEND_SXTB:	return "SXTB";
		case AARCH64_EXTEND_SXTH:	return "SXTH";
		case AARCH64_EXTEND_SXTW:	return "SXTW";
		case AARCH64_EXTEND_SXTX:	return "SXTX";
		default:			return shift(AARCH64_SHIFT_LSL);
	};
}

static bool
disassemble_arith(uint32_t ins, char *buf) {
	struct aarch64_adc_sbc    asc;
	struct aarch64_add_sub_xr xr;
	struct aarch64_add_sub_im im;
	struct aarch64_add_sub_sr sr;
	const char *name;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (aarch64_decode_adc(ins, &asc)) {
		name = "ADC";
		goto asc;
	} else if (aarch64_decode_adcs(ins, &asc)) {
		name = "ADCS";
		goto asc;
	} else if (aarch64_decode_add_xr(ins, &xr)) {
		name = "ADD";
		goto xr;
	} else if (aarch64_decode_add_im(ins, &im)) {
		if (aarch64_alias_mov_sp(&im)) {
			W("%-7s %s, %s", "MOV", reg(im.Rd), reg(im.Rn));
			return true;
		}
		name = "ADD";
		goto im;
	} else if (aarch64_decode_add_sr(ins, &sr)) {
		name = "ADD";
		goto sr;
	} else if (aarch64_decode_adds_xr(ins, &xr)) {
		if (aarch64_alias_cmn_xr(&xr)) {
			name = "CMN";
			goto c_xr;
		}
		name = "ADDS";
		goto xr;
	} else if (aarch64_decode_adds_im(ins, &im)) {
		if (aarch64_alias_cmn_im(&im)) {
			name = "CMN";
			goto c_im;
		}
		name = "ADDS";
		goto im;
	} else if (aarch64_decode_adds_sr(ins, &sr)) {
		if (aarch64_alias_cmn_sr(&sr)) {
			name = "CMN";
			goto c_sr;
		}
		name = "ADDS";
		goto sr;
	} else if (aarch64_decode_sbc(ins, &asc)) {
		if (aarch64_alias_ngc(&asc)) {
			name = "NGC";
			goto ngc;
		}
		name = "SBC";
		goto asc;
	} else if (aarch64_decode_sbcs(ins, &asc)) {
		if (aarch64_alias_ngcs(&asc)) {
			name = "NGCS";
			goto ngc;
		}
		name = "SBCS";
		goto asc;
	} else if (aarch64_decode_sub_xr(ins, &xr)) {
		name = "SUB";
		goto xr;
	} else if (aarch64_decode_sub_im(ins, &im)) {
		name = "SUB";
		goto im;
	} else if (aarch64_decode_sub_sr(ins, &sr)) {
		if (aarch64_alias_neg(&sr)) {
			name = "NEG";
			goto neg;
		}
		name = "SUB";
		goto sr;
	} else if (aarch64_decode_subs_xr(ins, &xr)) {
		if (aarch64_alias_cmp_xr(&xr)) {
			name = "CMP";
			goto c_xr;
		}
		name = "SUBS";
		goto xr;
	} else if (aarch64_decode_subs_im(ins, &im)) {
		if (aarch64_alias_cmp_im(&im)) {
			name = "CMP";
			goto c_im;
		}
		name = "SUBS";
		goto im;
	} else if (aarch64_decode_subs_sr(ins, &sr)) {
		if (aarch64_alias_cmp_sr(&sr)) {
			name = "CMP";
			goto c_sr;
		} else if (aarch64_alias_negs(&sr)) {
			name = "NEGS";
			goto neg;
		}
		name = "SUBS";
		goto sr;
	}
	return false;
ngc:
	W("%-7s %s, %s", name, reg(asc.Rd), reg(asc.Rm));
	return true;
asc:
	W("%-7s %s, %s, %s", name, reg(asc.Rd), reg(asc.Rn), reg(asc.Rm));
	return true;
c_xr:
	W("%-7s %s, %s", name, reg(xr.Rn), reg(xr.Rm));
	goto xr_suffix;
xr:
	W("%-7s %s, %s, %s", name, reg(xr.Rd), reg(xr.Rn), reg(xr.Rm));
xr_suffix:
	if (xr.amount > 0 || !AARCH64_EXTEND_IS_LSL(xr.extend)) {
		W(", %s", extend(xr.extend));
	}
	if (xr.amount > 0) {
		W(" #%u", xr.amount);
	}
	return true;
c_im:
	W("%-7s %s, #0x%x", name, reg(im.Rn), im.imm);
	goto im_suffix;
im:
	W("%-7s %s, %s, #0x%x", name, reg(im.Rd), reg(im.Rn), im.imm);
im_suffix:
	if (im.shift > 0) {
		W(", %s #%u", shift(AARCH64_SHIFT_LSL), im.shift);
	}
	return true;
c_sr:
	W("%-7s %s, %s", name, reg(sr.Rn), reg(sr.Rm));
	goto sr_suffix;
sr:
	W("%-7s %s, %s, %s", name, reg(sr.Rd), reg(sr.Rn), reg(sr.Rm));
sr_suffix:
	if (sr.amount > 0) {
		W(", %s #%u", shift(sr.shift), sr.amount);
	}
	return true;
neg:
	W("%-7s %s, %s", name, reg(sr.Rd), reg(sr.Rm));
	if (sr.amount > 0) {
		W(", %s #%u", shift(sr.shift), sr.amount);
	}
	return true;
#undef W
}

static bool
disassemble_logic(uint32_t ins, char *buf) {
	struct aarch64_and_orr_im im;
	struct aarch64_and_orr_sr sr;
	const char *name;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (aarch64_decode_and_im(ins, &im)) {
		name = "AND";
		goto im;
	} else if (aarch64_decode_and_sr(ins, &sr)) {
		name = "AND";
		goto sr;
	} else if (aarch64_decode_ands_im(ins, &im)) {
		if (aarch64_alias_tst_im(&im)) {
			name = "TST";
			goto c_im;
		}
		name = "ANDS";
		goto im;
	} else if (aarch64_decode_ands_sr(ins, &sr)) {
		if (aarch64_alias_tst_im(&im)) {
			name = "TST";
			goto c_sr;
		}
		name = "ANDS";
		goto sr;
	} else if (aarch64_decode_orr_im(ins, &im)) {
		if (aarch64_alias_mov_bi(&im)) {
			W("%-7s %s, %llx", "MOV", reg(im.Rd), im.imm);
			return true;
		}
		name = "ORR";
		goto im;
	} else if (aarch64_decode_orr_sr(ins, &sr)) {
		if (aarch64_alias_mov_r(&sr)) {
			W("%-7s %s, %s", "MOV", reg(sr.Rd), reg(sr.Rm));
			return true;
		}
		name = "ORR";
		goto sr;
	}
	return false;
c_im:
	W("%-7s %s, #%llx", name, reg(im.Rn), im.imm);
	return true;
im:
	W("%-7s %s, %s, #%llx", name, reg(im.Rd), reg(im.Rn), im.imm);
	return true;
c_sr:
	W("%-7s %s, %s", name, reg(sr.Rn), reg(sr.Rm));
	goto sr_suffix;
sr:
	W("%-7s %s, %s, %s", name, reg(sr.Rd), reg(sr.Rn), reg(sr.Rm));
sr_suffix:
	if (sr.amount > 0) {
		W(", %s #%d", shift(sr.shift), sr.amount);
	}
	return true;
#undef W
}

static bool
disassemble_memory(uint32_t ins, uint64_t pc, char *buf) {
	struct aarch64_ldp_stp      p;
	struct aarch64_ldr_str_ix   ix;
	struct aarch64_ldr_str_ui   ui;
	struct aarch64_ldr_str_r    r;
	struct aarch64_ldr_lit      lit;
	const char *name = NULL;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (aarch64_decode_ldp_post(ins, &p)) {
		name = "LDP";
		goto p_post;
	} else if (aarch64_decode_ldp_pre(ins, &p)) {
		name = "LDP";
		goto p_pre;
	} else if (aarch64_decode_ldp_si(ins, &p)) {
		name = "LDP";
		goto p_si;
	} else if (aarch64_decode_ldr_post(ins, &ix)) {
		name = "LDR";
		goto r_post;
	} else if (aarch64_decode_ldr_pre(ins, &ix)) {
		name = "LDR";
		goto r_pre;
	} else if (aarch64_decode_ldr_ui(ins, &ui)) {
		name = "LDR";
		goto r_ui;
	} else if (aarch64_decode_ldr_r(ins, &r)) {
		name = "LDR";
		goto r_r;
	} else if (aarch64_decode_ldr_lit(ins, pc, &lit)) {
		W("%-7s %s, #0x%llx", "LDR", reg(lit.Rt), lit.label);
		return true;
	} else if (aarch64_decode_stp_post(ins, &p)) {
		name = "STP";
		goto p_post;
	} else if (aarch64_decode_stp_pre(ins, &p)) {
		name = "STP";
		goto p_pre;
	} else if (aarch64_decode_stp_si(ins, &p)) {
		name = "STP";
		goto p_si;
	} else if (aarch64_decode_str_post(ins, &ix)) {
		name = "STR";
		goto r_post;
	} else if (aarch64_decode_str_pre(ins, &ix)) {
		name = "STR";
		goto r_pre;
	} else if (aarch64_decode_str_ui(ins, &ui)) {
		name = "STR";
		goto r_ui;
	} else if (aarch64_decode_str_r(ins, &r)) {
		name = "STR";
		goto r_r;
	}
	return false;
p_post:
	W("%-7s %s, %s, [%s], #%d", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn), p.imm);
	return true;
p_pre:
	W("%-7s %s, %s, [%s, #%d]!", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn), p.imm);
	return true;
p_si:
	if (p.imm == 0) {
		W("%-7s %s, %s, [%s]", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn));
	} else {
		W("%-7s %s, %s, [%s, #%d]", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn), p.imm);
	}
	return true;
r_post:
	W("%-7s %s, [%s], #%d", name, reg(ix.Rt), reg(ix.Xn), ix.imm);
	return true;
r_pre:
	W("%-7s %s, [%s, #%d]!", name, reg(ix.Rt), reg(ix.Xn), ix.imm);
	return true;
r_ui:
	if (ui.imm == 0) {
		W("%-7s %s, [%s]", name, reg(ui.Rt), reg(ui.Xn));
	} else {
		W("%-7s %s, [%s, #%u]", name, reg(ui.Rt), reg(ui.Xn), ui.imm);
	}
	return true;
r_r:
	if (r.amount == 0) {
		if (AARCH64_EXTEND_IS_LSL(r.extend)) {
			W("%-7s %s, [%s, %s]", name, reg(r.Rt), reg(r.Xn), reg(r.Rm));
		} else {
			W("%-7s %s, [%s, %s, %s]", name, reg(r.Rt), reg(r.Xn), reg(r.Rm),
					extend(r.extend));
		}
	} else {
		W("%-7s %s, [%s, %s, %s #%u]", name, reg(r.Rt), reg(r.Xn), reg(r.Rm),
				extend(r.extend), r.amount);
	}
	return true;
#undef W
}

static bool
disassemble_movknz(uint32_t ins, char *buf) {
	struct aarch64_movknz movknz;
	const char *name;
	uint64_t mov_imm;
	if (aarch64_decode_movk(ins, &movknz)) {
		name = "MOVK";
	} else if (aarch64_decode_movn(ins, &movknz)) {
		if (aarch64_alias_mov_nwi(&movknz)) {
			mov_imm = ~((uint64_t)movknz.imm << movknz.shift);
			goto mov;
		}
		name = "MOVN";
	} else if (aarch64_decode_movz(ins, &movknz)) {
		if (aarch64_alias_mov_wi(&movknz)) {
			mov_imm = (uint64_t)movknz.imm << movknz.shift;
			goto mov;
		}
		name = "MOVZ";
	} else {
		return false;
	}
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (movknz.shift == 0) {
		W("%-7s %s, #0x%x", name, reg(movknz.Rd), movknz.imm);
	} else {
		W("%-7s %s, #0x%x, %s #%d", name, reg(movknz.Rd), movknz.imm,
				shift(AARCH64_SHIFT_LSL), movknz.shift);
	}
	return true;
mov:
	W("%-7s %s, #0x%llx", "MOV", reg(movknz.Rd), mov_imm);
	return true;
#undef W
}

static bool
disassemble1(uint32_t ins, uint64_t pc, char buf[64]) {
	struct aarch64_adr_adrp     adr;
	struct aarch64_b_bl         b;
	struct aarch64_br_blr       br;
	struct aarch64_ret          ret;

	int idx = sprintf(buf, "%llx  ", pc);
	if (idx <= 0) {
		*buf = 0;
		return false;
	}
	buf += idx;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (disassemble_arith(ins, buf)) {
	} else if (disassemble_logic(ins, buf)) {
	} else if (disassemble_memory(ins, pc, buf)) {
	} else if (disassemble_movknz(ins, buf)) {
	} else if (aarch64_decode_adr(ins, pc, &adr)) {
		W("%-7s %s, #0x%llx", "ADR", reg(adr.Xd), adr.label);
	} else if (aarch64_decode_adrp(ins, pc, &adr)) {
		W("%-7s %s, #0x%llx", "ADRP", reg(adr.Xd), adr.label);
	} else if (aarch64_decode_b(ins, pc, &b)) {
		W("%-7s #0x%llx", "B", b.label);
	} else if (aarch64_decode_bl(ins, pc, &b)) {
		W("%-7s #0x%llx", "BL", b.label);
	} else if (aarch64_decode_blr(ins, &br)) {
		W("%-7s %s", "BLR", reg(br.Xn));
	} else if (aarch64_decode_br(ins, &br)) {
		W("%-7s %s", "BR", reg(br.Xn));
	} else if (aarch64_decode_nop(ins)) {
		W("NOP");
	} else if (aarch64_decode_ret(ins, &ret)) {
		if (ret.Xn == AARCH64_X30) {
			W("RET");
		} else {
			W("%-7s %s", "RET", reg(ret.Xn));
		}
	} else {
		W("%-7s %08x", "???", ins);
		return false;
	}
#undef W
	return true;
}

void
disassemble(const void *ins0, size_t *size0, size_t *count0, uintptr_t pc0) {
	assert((pc0 & 0x3) == 0);
	const uint32_t *ins   = ins0;
	size_t size           = *size0;
	size_t count          = *count0;
	uint64_t pc           = pc0;
	const uint64_t pc_end = pc0 + min(round2_down(size, sizeof(*ins)), count * sizeof(*ins));
	while (pc < pc_end) {
		char line[65];
		disassemble1(*ins, pc, line);
		printf("%s\n", line);
		ins++;
		pc += sizeof(*ins);
	}
	size_t processed = pc - pc0;
	*size0 = size - processed;
	*count0 = count - processed / sizeof(*ins);
}
