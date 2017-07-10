#include "memctl/aarch64/aarch64_sim.h"

#include "memctl/utility.h"

// Bit manipulations

/*
 * sign_extend
 *
 * Description:
 * 	Sign-extend x from bit len (0-indexed).
 */
static inline uint64_t
sign_extend(uint64_t x, unsigned len) {
	return bext(x, 1, len, 0, 0);
}

// Simulation

/*
 * add_with_carry
 *
 * Description:
 * 	Add x and y together with a carry bit c. Store the result in result and optionally store
 * 	the new status flags in nzcv.
 *
 * Notes:
 * 	The taint for both result and nzcv is the same: the meet of the taints for all three
 * 	sources.
 */
static void
add_with_carry(uint64_t *result, aarch64_pstate *nzcv, uint64_t x, uint64_t y, uint8_t c) {
	assert((c & ~1) == 0);
	uint64_t t = x + y;
	uint64_t r = t + c;
	*result = r;
	if (nzcv != NULL) {
		uint32_t n_ = (int64_t)r < 0;
		uint32_t z_ = r == 0;
		uint32_t c_ = t < x || r < c;
		uint64_t r_hi = ((int64_t)x < 0 ? -1 : 0) + ((int64_t) y < 0 ? -1 : 0)
			+ (t < x ? 1 : 0) + (r < c ? 1 : 0);
		uint32_t v_ = r_hi != (n_ ? -1 : 0);
		*nzcv = n_ << AARCH64_PSTATE_SHIFT_N
			| z_ << AARCH64_PSTATE_SHIFT_Z
			| c_ << AARCH64_PSTATE_SHIFT_C
			| v_ << AARCH64_PSTATE_SHIFT_V;
	}
}

/*
 * make_nzcv
 *
 * Description:
 * 	Create an NZCV for the given result. The C and V bits are set to 0.
 */
static aarch64_pstate
make_nzcv(uint64_t result) {
	return ((int64_t)result < 0) << AARCH64_PSTATE_SHIFT_N
		| (result == 0) << AARCH64_PSTATE_SHIFT_Z;
}

/*
 * gpreg_word_
 *
 * Description:
 * 	Return the aarch64_sim_word for the given general-purpose register (as long as it is not
 * 	the zero register).
 */
static struct aarch64_sim_word *
gpreg_word_(struct aarch64_sim *sim, aarch64_gpreg reg) {
	assert(!AARCH64_GPREGZR(reg));
	unsigned n = AARCH64_GPREGID(reg);
	if (n == AARCH64_SIM_GPREGS) {
		return &sim->SP;
	}
	return &sim->X[n];
}

/*
 * gpreg_mask_
 *
 * Description:
 * 	Get the size mask for the given general-purpose register.
 */
static uint64_t
gpreg_mask_(aarch64_gpreg reg) {
	return ((2 << (AARCH64_GPREGSIZE(reg) - 1)) - 1);
}

/*
 * gpreg_get_
 *
 * Description:
 * 	Read the value from the specified general-purpose register, meeting the given taint with
 * 	the register's taint.
 */
static uint64_t
gpreg_get_(struct aarch64_sim *sim, aarch64_gpreg reg, aarch64_sim_taint *taint) {
	if (AARCH64_GPREGZR(reg)) {
		aarch64_sim_taint_meet_with(taint, sim->taint_default[AARCH64_SIM_TAINT_CONSTANT]);
		return 0;
	}
	struct aarch64_sim_word *word = gpreg_word_(sim, reg);
	aarch64_sim_taint_meet_with(taint, word->taint);
	return word->value & gpreg_mask_(reg);
}

/*
 * gpreg_set_
 *
 * Description:
 * 	Write the value to the specified general-purpose register, replacing the register's taint
 * 	with the given taint.
 */
static void
gpreg_set_(struct aarch64_sim *sim, aarch64_gpreg reg, uint64_t value, aarch64_sim_taint taint) {
	if (!AARCH64_GPREGZR(reg)) {
		struct aarch64_sim_word *word = gpreg_word_(sim, reg);
		word->taint = taint;
		word->value = value & gpreg_mask_(reg);
	}
}

/*
 * gpreg_get_extend_
 *
 * Description:
 * 	Read the value from the specified general-purpose register, meeting the given taint with
 * 	the register's taint. Return the extended and shifted value.
 */
static uint64_t
gpreg_get_extend_(struct aarch64_sim *sim, aarch64_gpreg reg, aarch64_extend extend,
		unsigned shift, aarch64_sim_taint *taint) {
	assert(shift <= 4);
	uint64_t value = gpreg_get_(sim, reg, taint);
	uint8_t length = 1 << AARCH64_EXTEND_LEN(extend);
	bool sign = AARCH64_EXTEND_SIGN(extend);
	return bext(value, sign, length - 1, 0, shift) & gpreg_mask_(reg);
}

static uint64_t
gpreg_get_shift_(struct aarch64_sim *sim, aarch64_gpreg reg, aarch64_shift shift, unsigned amount,
		aarch64_sim_taint *taint) {
	uint64_t value = gpreg_get_(sim, reg, taint);
	size_t size = AARCH64_GPREGSIZE(reg);
	switch (shift) {
		case AARCH64_SHIFT_LSL: return lsl(value, amount, size);
		case AARCH64_SHIFT_LSR: return lsr(value, amount);
		case AARCH64_SHIFT_ASR: return asr(value, amount, size);
		case AARCH64_SHIFT_ROR: return ror(value, amount, size);
		default:                assert(false);
	}
}

/*
 * MACRO pstate_get_
 *
 * Description:
 * 	Get the specified flag from the PSTATE register, meeting the corresponding taint field with
 * 	the specified taint.
 */
#define pstate_get_(sim, pstate_flag, pstate_taint, taint)				\
	meet_and_return_(taint,								\
			sim->PSTATE.taint_##pstate_taint,				\
			(sim->PSTATE.pstate & AARCH64_PSTATE_##pstate_flag)		\
				>> AARCH64_PSTATE_SHIFT_##pstate_flag)

#define pstate_set_(sim, pstate_flag, value, pstate_taint, taint)			\
	do {										\
		assert((value & ~AARCH64_PSTATE_##pstate_flag) == 0);			\
		aarch64_sim_taint_meet_with(&sim->PSTATE.taint_##pstate_taint, taint);	\
		sim->PSTATE.pstate &= ~AARCH64_PSTATE_##pstate_flag;			\
		sim->PSTATE.pstate |= value;						\
	} while (0)

/*
 * meet_and_return_
 *
 * Description:
 * 	A helper function for the pstate_get_ macro.
 */
static uint64_t
meet_and_return_(aarch64_sim_taint *taint, aarch64_sim_taint source, uint64_t value) {
	aarch64_sim_taint_meet_with(taint, source);
	return value;
}

// Public API

void
aarch64_sim_taint_meet_with(aarch64_sim_taint *a, aarch64_sim_taint b) {
	a->t_and &= b.t_and;
	a->t_or |= b.t_or;
}

void
aarch64_sim_word_clear(struct aarch64_sim *sim, struct aarch64_sim_word *word) {
	word->value = 0;
	word->taint = sim->taint_default[AARCH64_SIM_TAINT_UNKNOWN];
}

void
aarch64_sim_pstate_clear(struct aarch64_sim *sim, struct aarch64_sim_pstate *pstate) {
	pstate->pstate = 0;
	pstate->taint_nzcv = sim->taint_default[AARCH64_SIM_TAINT_UNKNOWN];
}

void
aarch64_sim_clear(struct aarch64_sim *sim) {
	aarch64_sim_word_clear(sim, &sim->instruction);
	aarch64_sim_word_clear(sim, &sim->PC);
	for (size_t i = 0; i < AARCH64_SIM_GPREGS; i++) {
		aarch64_sim_word_clear(sim, &sim->X[i]);
	}
	aarch64_sim_word_clear(sim, &sim->SP);
	aarch64_sim_pstate_clear(sim, &sim->PSTATE);
}

void
aarch64_sim_pc_advance(struct aarch64_sim *sim) {
	sim->PC.value += AARCH64_SIM_INSTRUCTION_SIZE;
	assert((sim->PC.value & 3) == 0);
	aarch64_sim_taint_meet_with(&sim->PC.taint, sim->taint_default[AARCH64_SIM_TAINT_CONSTANT]);
}

bool
aarch64_sim_step(struct aarch64_sim *sim) {
	bool keep_running = true;
	// Fetch the next instruction.
	bool run = sim->instruction_fetch(sim);
	if (!run) {
		return false;
	}

	// The instruction and program counter.
	uint32_t ins = (uint32_t) sim->instruction.value;
	uint64_t pc = sim->PC.value;
	// The taint for all sources.
	aarch64_sim_taint taint = sim->instruction.taint;
	// Branching state.
	bool do_branch = false;
	bool take_branch = true;
	enum aarch64_sim_branch_type branch_type = AARCH64_SIM_BRANCH_TYPE_BRANCH;
	struct aarch64_sim_word branch_address = { 0, taint };
	struct aarch64_sim_word branch_condition = { 1, taint };
	// Common values.
	uint64_t op1, op2, result;
	uint8_t carry = 0;
	aarch64_pstate nzcv;

	struct aarch64_ins_adc     adc;
	struct aarch64_ins_add_xr  add_xr;
	struct aarch64_ins_add_im  add_im;
	struct aarch64_ins_add_sr  add_sr;
	struct aarch64_ins_adr     adr;
	struct aarch64_ins_and_im  and_im;
	struct aarch64_ins_and_sr  and_sr;
	struct aarch64_ins_b       b;
	struct aarch64_ins_br      br;
	struct aarch64_ins_cbz     cbz;
	struct aarch64_ins_ldp     ldp;
	struct aarch64_ins_ldr_im  ldr_im;
	struct aarch64_ins_ldr_lit ldr_lit;
	struct aarch64_ins_mov     mov;

	if (aarch64_decode_adc(ins, &adc)) {
		op1 = gpreg_get_(sim, adc.Rn, &taint);
		op2 = gpreg_get_(sim, adc.Rm, &taint);
		carry = pstate_get_(sim, C, nzcv, &taint);
		if (!adc.adc) {
			op2 = ~op2;
		}
		add_with_carry(&result, &nzcv, op1, op2, carry);
		if (adc.setflags) {
			pstate_set_(sim, NZCV, nzcv, nzcv, taint);
		}
		gpreg_set_(sim, adc.Rd, result, taint);
	} else if (aarch64_decode_add_xr(ins, &add_xr)) {
		op1 = gpreg_get_(sim, add_xr.Rn, &taint);
		op2 = gpreg_get_extend_(sim, add_xr.Rn, add_xr.extend, add_xr.amount,
				&taint);
		if (!add_xr.add) {
			op2 = ~op2;
			carry = 1;
		}
		add_with_carry(&result, &nzcv, op1, op2, carry);
		if (add_xr.setflags) {
			pstate_set_(sim, NZCV, nzcv, nzcv, taint);
		}
		gpreg_set_(sim, add_xr.Rd, result, taint);
	} else if (aarch64_decode_add_im(ins, &add_im)) {
		op1 = gpreg_get_(sim, add_im.Rn, &taint);
		op2 = (uint64_t)add_im.imm << add_im.shift;
		if (!add_im.add) {
			op2 = ~op2;
			carry = 1;
		}
		add_with_carry(&result, &nzcv, op1, op2, carry);
		if (add_im.setflags) {
			pstate_set_(sim, NZCV, nzcv, nzcv, taint);
		}
		gpreg_set_(sim, add_im.Rd, result, taint);
	} else if (aarch64_decode_add_sr(ins, &add_sr)) {
		op1 = gpreg_get_(sim, add_sr.Rn, &taint);
		op2 = gpreg_get_shift_(sim, add_sr.Rm, add_sr.shift, add_sr.amount, &taint);
		if (!add_sr.add) {
			op2 = ~op2;
			carry = 1;
		}
		add_with_carry(&result, &nzcv, op1, op2, carry);
		if (add_sr.setflags) {
			pstate_set_(sim, NZCV, nzcv, nzcv, taint);
		}
		gpreg_set_(sim, add_sr.Rd, result, taint);
	} else if (aarch64_decode_adr(ins, pc, &adr)) {
		aarch64_sim_taint_meet_with(&taint, sim->PC.taint);
		gpreg_set_(sim, adr.Xd, adr.label, taint);
	} else if (aarch64_decode_and_im(ins, &and_im)) {
		op1 = gpreg_get_(sim, and_im.Rn, &taint);
		op2 = and_im.imm;
		if (and_im.and) {
			result = op1 & op2;
		} else if (and_im.or) {
			result = op1 | op2;
		} else {
			assert(and_im.xor);
			result = op1 ^ op2;
		}
		if (and_im.setflags) {
			pstate_set_(sim, NZCV, make_nzcv(result), nzcv, taint);
		}
		gpreg_set_(sim, and_im.Rd, result, taint);
	} else if (aarch64_decode_and_sr(ins, &and_sr)) {
		op1 = gpreg_get_(sim, and_sr.Rn, &taint);
		op2 = gpreg_get_shift_(sim, and_sr.Rm, and_sr.shift, and_sr.amount, &taint);
		if (and_sr.not) {
			op2 = ~op2;
		}
		if (and_sr.and) {
			result = op1 & op2;
		} else if (and_sr.or) {
			result = op1 | op2;
		} else {
			assert(and_sr.xor);
			result = op1 ^ op2;
		}
		if (and_sr.setflags) {
			pstate_set_(sim, NZCV, make_nzcv(result), nzcv, taint);
		}
		gpreg_set_(sim, and_sr.Rd, result, taint);
	} else if (aarch64_decode_b(ins, pc, &b)) {
		do_branch = true;
		if (b.link) {
			branch_type = AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK;
		}
		branch_address.value = b.label;
		aarch64_sim_taint_meet_with(&branch_address.taint, sim->PC.taint);
	} else if (aarch64_decode_br(ins, &br)) {
		do_branch = true;
		if (br.ret) {
			branch_type = AARCH64_SIM_BRANCH_TYPE_RETURN;
		} else if (br.link) {
			branch_type = AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK;
		}
		branch_address.value = gpreg_get_(sim, br.Xn, &taint);
		branch_address.taint = taint;
	} else if (aarch64_decode_cbz(ins, pc, &cbz)) {
		do_branch = true;
		branch_type = AARCH64_SIM_BRANCH_TYPE_CONDITIONAL;
		op1 = gpreg_get_(sim, cbz.Rt, &taint);
		branch_condition.value = ((cbz.n && op1 != 0) || (!cbz.n && op1 == 0));
		branch_condition.taint = taint;
		branch_address.value = pc + cbz.label;
		aarch64_sim_taint_meet_with(&branch_address.taint, sim->PC.taint);
	} else if (aarch64_decode_ldp(ins, &ldp)) {
		uint64_t address = gpreg_get_(sim, ldp.Xn, &taint);
		if (!ldp.post) {
			address += ldp.imm;
		}
		struct aarch64_sim_word address_taint = { address, taint };
		size_t size = 1 << ldp.size;
		if (ldp.load) {
			struct aarch64_sim_word mem1 = { 0, taint }, mem2 = { 0, taint };
			run = sim->memory_load(sim, &mem1, &address_taint, size);
			if (!run) {
				keep_running = false;
			}
			address_taint.value += size;
			run = sim->memory_load(sim, &mem2, &address_taint, size);
			if (!run) {
				keep_running = false;
			}
			if (ldp.sign) {
				mem1.value = sign_extend(mem1.value, 8 * size - 1);
				mem2.value = sign_extend(mem2.value, 8 * size - 1);
			}
			gpreg_set_(sim, ldp.Rt1, mem1.value, mem1.taint);
			gpreg_set_(sim, ldp.Rt2, mem2.value, mem2.taint);
		} else {
			struct aarch64_sim_word reg1 = { 0, sim->instruction.taint };
			struct aarch64_sim_word reg2 = { 0, sim->instruction.taint };
			reg1.value = gpreg_get_(sim, ldp.Rt1, &reg1.taint);
			reg2.value = gpreg_get_(sim, ldp.Rt2, &reg2.taint);
			run = sim->memory_store(sim, &reg1, &address_taint, size);
			if (!run) {
				keep_running = false;
			}
			address_taint.value += size;
			run = sim->memory_store(sim, &reg2, &address_taint, size);
			if (!run) {
				keep_running = false;
			}
		}
		if (ldp.wb) {
			if (ldp.post) {
				address += ldp.imm;
			}
			gpreg_set_(sim, ldp.Xn, address, taint);
		}
	} else if (aarch64_decode_ldr_ix(ins, &ldr_im)
			|| aarch64_decode_ldr_ui(ins, &ldr_im)) {
		uint64_t address = gpreg_get_(sim, ldr_im.Xn, &taint);
		if (!ldr_im.post) {
			address += ldr_im.imm;
		}
		struct aarch64_sim_word address_taint = { address, taint };
		size_t size = 1 << ldr_im.size;
		if (ldr_im.load) {
			struct aarch64_sim_word mem = { 0, taint };
			run = sim->memory_load(sim, &mem, &address_taint, size);
			if (!run) {
				keep_running = false;
			}
			if (ldr_im.sign) {
				mem.value = sign_extend(mem.value, 8 * size - 1);
			}
			gpreg_set_(sim, ldr_im.Rt, mem.value, mem.taint);
		} else {
			struct aarch64_sim_word reg = { 0, sim->instruction.taint };
			reg.value = gpreg_get_(sim, ldr_im.Rt, &reg.taint);
			run = sim->memory_store(sim, &reg, &address_taint, size);
			if (!run) {
				keep_running = false;
			}
		}
		if (ldr_im.wb) {
			if (ldr_im.post) {
				address += ldr_im.imm;
			}
			gpreg_set_(sim, ldr_im.Xn, address, taint);
		}
	} else if (aarch64_decode_ldr_lit(ins, pc, &ldr_lit)) {
		struct aarch64_sim_word address_taint = { ldr_lit.label, taint };
		aarch64_sim_taint_meet_with(&address_taint.taint, sim->PC.taint);
		size_t size = 1 << ldr_lit.size;
		assert(ldr_lit.load);
		struct aarch64_sim_word mem = { 0, taint };
		run = sim->memory_load(sim, &mem, &address_taint, size);
		if (!run) {
			keep_running = false;
		}
		if (ldr_lit.sign) {
			mem.value = sign_extend(mem.value, 8 * size - 1);
		}
		gpreg_set_(sim, ldr_im.Rt, mem.value, mem.taint);
	} else if (aarch64_decode_mov(ins, &mov)) {
		op1 = 0;
		if (mov.k) {
			op1 = gpreg_get_(sim, mov.Rd, &taint);
			op1 &= ~(ones(16) << mov.shift);
		}
		op1 |= (uint64_t)mov.imm << mov.shift;
		if (mov.n) {
			op1 = ~op1;
		}
		gpreg_set_(sim, mov.Rd, op1, taint);
	} else if (aarch64_decode_nop(ins)) {
		// Do nothing.
	} else {
		run = sim->illegal_instruction(sim);
		if (!run) {
			keep_running = false;
		}
	}

	// Handle any branching.
	if (do_branch) {
		run = sim->branch_hit(sim, branch_type, &branch_address, &branch_condition,
				&take_branch);
		if (!run) {
			keep_running = false;
		}
	}
	// Advance PC to the next instruction.
	aarch64_sim_pc_advance(sim);
	// If we're taking the branch, perform any linking and set PC to the branch target.
	if (do_branch && take_branch) {
		// Perform the link step.
		if (branch_type == AARCH64_SIM_BRANCH_TYPE_BRANCH_AND_LINK) {
			// The sources of X30's new taint are PC and the current instruction. We
			// assume that the taint at this point is still the current instruction's
			// taint.
			assert(taint.t_and == sim->instruction.taint.t_and
					&& taint.t_or == sim->instruction.taint.t_or);
			aarch64_sim_taint_meet_with(&taint, sim->PC.taint);
			gpreg_set_(sim, AARCH64_X30, sim->PC.value, taint);
		}
		// Set PC to the branch target.
		sim->PC = branch_address;
	}

	return keep_running;
}

void
aarch64_sim_run(struct aarch64_sim *sim) {
	bool run;
	do {
		run = aarch64_sim_step(sim);
	} while (run);
}
