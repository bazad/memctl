#include "disassemble.h"

#include "memctl/aarch64/disasm.h"
#include "memctl/utility.h"

#include <assert.h>
#include <stdio.h>

/*
 * reg
 *
 * Description:
 * 	Returns a string representation of the given register.
 */
static const char *
reg(aarch64_gpreg r) {
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
	struct aarch64_ins_adc    adc;
	struct aarch64_ins_add_xr xr;
	struct aarch64_ins_add_im im;
	struct aarch64_ins_add_sr sr;
	const char *name;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (aarch64_decode_adc(ins, &adc)) {
		if (AARCH64_INS_TYPE(ins, ADC_INS)) {
			name = "ADC";
		} else if (AARCH64_INS_TYPE(ins, ADCS_INS)) {
			name = "ADCS";
		} else if (AARCH64_INS_TYPE(ins, SBC_INS)) {
			if (aarch64_alias_ngc(&adc)) {
				name = "NGC";
				goto ngc;
			}
			name = "SBC";
		} else { // AARCH64_INS_TYPE(ins, SBCS_INS)
			if (aarch64_alias_ngcs(&adc)) {
				name = "NGCS";
				goto ngc;
			}
			name = "SBCS";
		}
		W("%-7s %s, %s, %s", name, reg(adc.Rd), reg(adc.Rn), reg(adc.Rm));
		return true;
ngc:
		W("%-7s %s, %s", name, reg(adc.Rd), reg(adc.Rm));
		return true;
	} else if (aarch64_decode_add_xr(ins, &xr)) {
		if (AARCH64_INS_TYPE(ins, ADD_XR_INS)) {
			name = "ADD";
		} else if (AARCH64_INS_TYPE(ins, ADDS_XR_INS)) {
			if (aarch64_alias_cmn_xr(&xr)) {
				name = "CMN";
				goto c_xr;
			}
			name = "ADDS";
		} else if (AARCH64_INS_TYPE(ins, SUB_XR_INS)) {
			name = "SUB";
		} else { // AARCH64_INS_TYPE(ins, SUBS_XR_INS)
			if (aarch64_alias_cmp_xr(&xr)) {
				name = "CMP";
				goto c_xr;
			}
			name = "SUBS";
		}
		W("%-7s %s, %s, %s", name, reg(xr.Rd), reg(xr.Rn), reg(xr.Rm));
xr_suffix:
		if (xr.amount > 0 || !AARCH64_EXTEND_IS_LSL(xr.extend)) {
			W(", %s", extend(xr.extend));
		}
		if (xr.amount > 0) {
			W(" #%u", xr.amount);
		}
		return true;
c_xr:
		W("%-7s %s, %s", name, reg(xr.Rn), reg(xr.Rm));
		goto xr_suffix;
	} else if (aarch64_decode_add_im(ins, &im)) {
		if (AARCH64_INS_TYPE(ins, ADD_IM_INS)) {
			if (aarch64_alias_mov_sp(&im)) {
				W("%-7s %s, %s", "MOV", reg(im.Rd), reg(im.Rn));
				return true;
			}
			name = "ADD";
		} else if (AARCH64_INS_TYPE(ins, ADDS_IM_INS)) {
			if (aarch64_alias_cmn_im(&im)) {
				name = "CMN";
				goto c_im;
			}
			name = "ADDS";
		} else if (AARCH64_INS_TYPE(ins, SUB_IM_INS)) {
			name = "SUB";
		} else { // AARCH64_INS_TYPE(ins, SUBS_IM_INS)
			if (aarch64_alias_cmp_im(&im)) {
				name = "CMP";
				goto c_im;
			}
			name = "SUBS";
		}
		W("%-7s %s, %s, #0x%x", name, reg(im.Rd), reg(im.Rn), im.imm);
im_suffix:
		if (im.shift > 0) {
			W(", %s #%u", shift(AARCH64_SHIFT_LSL), im.shift);
		}
		return true;
c_im:
		W("%-7s %s, #0x%x", name, reg(im.Rn), im.imm);
		goto im_suffix;
	} else if (aarch64_decode_add_sr(ins, &sr)) {
		if (AARCH64_INS_TYPE(ins, ADD_SR_INS)) {
			name = "ADD";
		} else if (AARCH64_INS_TYPE(ins, ADDS_SR_INS)) {
			if (aarch64_alias_cmn_sr(&sr)) {
				name = "CMN";
				goto c_sr;
			}
			name = "ADDS";
		} else if (AARCH64_INS_TYPE(ins, SUB_SR_INS)) {
			if (aarch64_alias_neg(&sr)) {
				name = "NEG";
				goto neg;
			}
			name = "SUB";
		} else { // AARCH64_INS_TYPE(ins, SUBS_SR_INS)
			if (aarch64_alias_cmp_sr(&sr)) {
				name = "CMP";
				goto c_sr;
			} else if (aarch64_alias_negs(&sr)) {
				name = "NEGS";
				goto neg;
			}
			name = "SUBS";
		}
		W("%-7s %s, %s, %s", name, reg(sr.Rd), reg(sr.Rn), reg(sr.Rm));
sr_suffix:
		if (sr.amount > 0) {
			W(", %s #%u", shift(sr.shift), sr.amount);
		}
		return true;
c_sr:
		W("%-7s %s, %s", name, reg(sr.Rn), reg(sr.Rm));
		goto sr_suffix;
neg:
		W("%-7s %s, %s", name, reg(sr.Rd), reg(sr.Rm));
		if (sr.amount > 0) {
			W(", %s #%u", shift(sr.shift), sr.amount);
		}
		return true;
	}
	return false;
#undef W
}

static bool
disassemble_logic(uint32_t ins, char *buf) {
	struct aarch64_ins_and_im im;
	struct aarch64_ins_and_sr sr;
	const char *name;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (aarch64_decode_and_im(ins, &im)) {
		if (im.or) {
			if (aarch64_alias_mov_bi(&im)) {
				W("%-7s %s, #0x%llx", "MOV", reg(im.Rd), im.imm);
				return true;
			}
			name = "ORR";
		} else if (im.xor) {
			name = "EOR";
		} else if (im.setflags) {
			if (aarch64_alias_tst_im(&im)) {
				W("%-7s %s, #0x%llx", "TST", reg(im.Rn), im.imm);
				return true;
			}
			name = "ANDS";
		} else {
			name = "AND";
		}
		W("%-7s %s, %s, #0x%llx", name, reg(im.Rd), reg(im.Rn), im.imm);
		return true;
	} else if (aarch64_decode_and_sr(ins, &sr)) {
		if (sr.or) {
			if (sr.not) {
				if (aarch64_alias_mvn(&sr)) {
					name = "MVN";
					goto sr_mov;
				}
				name = "ORN";
			} else {
				if (aarch64_alias_mov_r(&sr)) {
					name = "MOV";
					goto sr_mov;
				}
				name = "ORR";
			}
		} else if (sr.xor) {
			if (sr.not) {
				name = "EON";
			} else {
				name = "EOR";
			}
		} else if (sr.setflags) {
			if (sr.not) {
				name = "BICS";
			} else {
				if (aarch64_alias_tst_sr(&sr)) {
					W("%-7s %s, %s", "TST", reg(sr.Rn), reg(sr.Rm));
					goto sr_suffix;
				}
				name = "ANDS";
			}
		} else {
			if (sr.not) {
				name = "BIC";
			} else {
				name = "AND";
			}
		}
		W("%-7s %s, %s, %s", name, reg(sr.Rd), reg(sr.Rn), reg(sr.Rm));
sr_suffix:
		if (sr.amount > 0) {
			W(", %s #%d", shift(sr.shift), sr.amount);
		}
		return true;
sr_mov:
		W("%-7s %s, %s", name, reg(sr.Rd), reg(sr.Rm));
		return true;
	}
	return false;
#undef W
}

static bool
disassemble_memory(uint32_t ins, uint64_t pc, char *buf) {
	struct aarch64_ins_ldp     p;
	struct aarch64_ins_ldr_im  im;
	struct aarch64_ins_ldr_r   r;
	struct aarch64_ins_ldr_lit lit;
	const char *name = NULL;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (aarch64_decode_ldp(ins, &p)) {
		if (p.load) {
			if (p.nt) {
				name = "LDNP";
			} else if (p.sign) {
				name = "LDPSW";
			} else {
				name = "LDP";
			}
		} else {
			if (p.nt) {
				name = "STNP";
			} else {
				name = "STP";
			}
		}
		if (!p.wb) {
			if (p.imm == 0) {
				W("%-7s %s, %s, [%s]", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn));
			} else {
				W("%-7s %s, %s, [%s, #%d]", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn), p.imm);
			}
		} else if (p.post) {
			W("%-7s %s, %s, [%s], #%d", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn), p.imm);
		} else {
			W("%-7s %s, %s, [%s, #%d]!", name, reg(p.Rt1), reg(p.Rt2), reg(p.Xn), p.imm);
		}
	} else if (aarch64_decode_ldr_ix(ins, &im)
			|| aarch64_decode_ldr_ui(ins, &im)) {
		if (im.load) {
			if (im.sign) {
				if (im.size == 0) {
					name = "LDRSB";
				} else if (im.size == 1) {
					name = "LDRSH";
				} else {
					name = "LDRSW";
				}
			} else {
				if (im.size == 0) {
					name = "LDRB";
				} else if (im.size == 1) {
					name = "LDRH";
				} else {
					name = "LDR";
				}
			}
		} else {
			if (im.size == 0) {
				name = "STRB";
			} else if (im.size == 1) {
				name = "STRH";
			} else {
				name = "STR";
			}
		}
		if (im.post) {
			W("%-7s %s, [%s], #%d", name, reg(im.Rt), reg(im.Xn), im.imm);
		} else if (im.wb) {
			W("%-7s %s, [%s, #%d]!", name, reg(im.Rt), reg(im.Xn), im.imm);
		} else if (im.imm == 0) {
			W("%-7s %s, [%s]", name, reg(im.Rt), reg(im.Xn));
		} else {
			W("%-7s %s, [%s, #%u]", name, reg(im.Rt), reg(im.Xn), im.imm);
		}
	} else if (aarch64_decode_ldr_r(ins, &r)) {
		if (r.load) {
			if (r.sign) {
				if (r.size == 0) {
					name = "LDRSB";
				} else if (r.size == 1) {
					name = "LDRSH";
				} else {
					name = "LDRSW";
				}
			} else {
				if (r.size == 0) {
					name = "LDRB";
				} else if (r.size == 1) {
					name = "LDRH";
				} else {
					name = "LDR";
				}
			}
		} else {
			if (r.size == 0) {
				name = "STRB";
			} else if (r.size == 1) {
				name = "STRH";
			} else {
				name = "STR";
			}
		}
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
	} else if (aarch64_decode_ldr_lit(ins, pc, &lit)) {
		if (lit.sign) {
			name = "LDRSW";
		} else {
			name = "LDR";
		}
		W("%-7s %s, #0x%llx", name, reg(lit.Rt), lit.label);
	} else {
		return false;
	}
	return true;
#undef W
}

static bool
disassemble_movknz(uint32_t ins, char *buf) {
	struct aarch64_ins_mov mov;
	const char *name;
	uint64_t mov_imm;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (!aarch64_decode_mov(ins, &mov)) {
		return false;
	}
	if (mov.k) {
		name = "MOVK";
	} else if (mov.n) {
		if (aarch64_alias_mov_nwi(&mov)) {
			mov_imm = ~((uint64_t)mov.imm << mov.shift);
			goto mov_imm;
		}
		name = "MOVN";
	} else {
		if (aarch64_alias_mov_wi(&mov)) {
			mov_imm = (uint64_t)mov.imm << mov.shift;
			goto mov_imm;
		}
		name = "MOVZ";
	}
	if (mov.shift == 0) {
		W("%-7s %s, #0x%x", name, reg(mov.Rd), mov.imm);
	} else {
		W("%-7s %s, #0x%x, %s #%d", name, reg(mov.Rd), mov.imm,
				shift(AARCH64_SHIFT_LSL), mov.shift);
	}
	return true;
mov_imm:
	W("%-7s %s, #0x%llx", "MOV", reg(mov.Rd), mov_imm);
	return true;
#undef W
}

static bool
disassemble_adr_b(uint32_t ins, uint64_t pc, char *buf) {
	struct aarch64_ins_adr adr;
	struct aarch64_ins_b   b;
	struct aarch64_ins_br  br;
	struct aarch64_ins_cbz cbz;
#define W(fmt, ...) \
	buf += sprintf(buf, fmt, ##__VA_ARGS__)
	if (aarch64_decode_adr(ins, pc, &adr)) {
		W("%-7s %s, #0x%llx", (adr.adrp ? "ADRP" : "ADR"), reg(adr.Xd), adr.label);
	} else if (aarch64_decode_b(ins, pc, &b)) {
		W("%-7s #0x%llx", (b.link ? "BL" : "B"), b.label);
	} else if (aarch64_decode_br(ins, &br)) {
		if (!br.ret) {
			W("%-7s %s", (br.link ? "BLR" : "BR"), reg(br.Xn));
		} else if (br.Xn == AARCH64_X30) {
			W("RET");
		} else {
			W("%-7s %s", "RET", reg(br.Xn));
		}
	} else if (aarch64_decode_cbz(ins, pc, &cbz)) {
		W("%-7s %s, #0x%llx", (cbz.n ? "CBNZ" : "CBZ"), reg(cbz.Rt), cbz.label);
	} else {
		return false;
	}
	return true;
#undef W
}

static bool
disassemble1(uint32_t ins, uint64_t pc, char buf[64]) {
	int idx = sprintf(buf, "%016llx  ", pc);
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
	} else if (disassemble_adr_b(ins, pc, buf)) {
	} else if (aarch64_decode_nop(ins)) {
		W("NOP");
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
