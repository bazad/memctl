#include "vtable.h"

#include "kernel.h"
#include "memctl_error.h"
#include "utility.h"

#if __arm64__
#include "aarch64/disasm.h"
#include "kernel_slide.h"
#include "kernelcache.h"
#endif

#include <stdio.h>
#include <string.h>

#if KERNELCACHE && __arm64__
#define VTABLE_FOR_CLASS_DISASSEMBLE 1
#endif

/*
 * vtable_symbol
 *
 * Description:
 * 	Generate the symbol name for the vtable of the specified class.
 */
static char *
vtable_symbol(const char *class_name) {
	char *symbol;
	asprintf(&symbol, "__ZTV%zu%s", strlen(class_name), class_name);
	if (symbol == NULL) {
		error_out_of_memory();
	}
	return symbol;
}

/*
 * adjust_vtable_from_symbol
 *
 * Description:
 * 	Adjust the vtable address and size found using a symbol lookup.
 */
static void
adjust_vtable_from_symbol(kaddr_t *vtable, size_t *size) {
	*vtable += 2 * sizeof(kword_t);
	if (size != NULL) {
		*size -= 2 * sizeof(kword_t);
	}
}

#if VTABLE_FOR_CLASS_DISASSEMBLE

/*
 * How we find vtables in the kernelcache
 * --------------------------------------
 *
 * Each kernel extension that declares OSObject subclasses has a number of initialization functions
 * in the __DATA_CONST.__mod_init_func section of the Mach-O. These initialization functions
 * construct the OSMetaClass object associated with each class. The OSMetaClass constructor is:
 *
 *   OSMetaClass::OSMetaClass(char const*, OSMetaClass const*, unsigned int)
 *
 * The first argument to the constructor (excluding the implicit this pointer) is the name of the
 * class represented by this OSMetaClass object. By disassembling every initialization function
 * declared by a kernel extension and looking at this argument, we can detect which OSMetaClass
 * instance is associated with the desired class name.
 *
 * The reason this OSMetaClass object is useful is that all OSObject subclasses override the
 * getMetaClass method:
 *
 *   virtual const OSMetaClass * getMetaClass() const
 *
 * The override is defined in the OSDefineMetaClassWithInit macro, which must be called for every
 * OSObject subclass. Thus, every virtual method table corresponding to a subclass of OSObject
 * contains a method that returns the associated OSMetaClass instance.
 *
 * Thus, if we know which OSMetaClass instance is associated with the given class name, we can
 * disassemble the getMetaClass method in every virtual method table to find which one returns the
 * correct OSMetaClass instance. This will be the virtual method table we are looking for.
 */

// AArch64 simulator

#define NREGS          32
#define TEMPREGS_START 0
#define TEMPREGS_END   17

/*
 * struct sim
 *
 * Description:
 * 	A simplistic AArch64 simulator to run an initialization function and detect construction of
 * 	OSMetaClass objects.
 */
struct sim {
	uint64_t pc;
	const uint32_t *ins;
	size_t count;
	struct {
		uint64_t value;
		enum { VALUE, UNKNOWN } state;
	} X[NREGS];
	uint64_t bl;
	uint64_t ret;
};

/*
 * sim_clear
 *
 * Description:
 * 	Clear the register state in the simulator.
 */
static void
sim_clear(struct sim *sim) {
	for (size_t i = 0; i < NREGS; i++) {
		sim->X[i].state = UNKNOWN;
	}
}

/*
 * sim_clear_temporary
 *
 * Description:
 * 	Clear the temporary registers.
 */
static void
sim_clear_temporary(struct sim *sim) {
	for (size_t i = TEMPREGS_START; i <= TEMPREGS_END; i++) {
		sim->X[i].state = UNKNOWN;
	}
}

/*
 * regmask
 *
 * Description:
 * 	Get the register mask for the given register.
 */
static uint64_t
regmask(aarch64_reg r) {
	return ((2 << (AARCH64_REGSIZE(r) - 1)) - 1);
}

/*
 * getreg
 *
 * Description:
 * 	Get the contents of the given register, and set the register state flags in state.
 */
static uint64_t
getreg(struct sim *sim, aarch64_reg r, int *state) {
	if (AARCH64_REGZR(r)) {
		*state |= VALUE;
		return 0;
	}
	unsigned n = AARCH64_REGNAME(r);
	*state |= sim->X[n].state;
	return sim->X[n].value & regmask(r);
}

/*
 * shiftreg
 *
 * Description:
 * 	Get the shifted register value.
 */
static uint64_t
shiftreg(struct sim *sim, aarch64_reg r, aarch64_shift shift, unsigned amount, int *state) {
	uint64_t value = getreg(sim, r, state);
	size_t size = AARCH64_REGSIZE(r);
	switch (shift) {
		case AARCH64_SHIFT_LSL: return lsl(value, amount, size);
		case AARCH64_SHIFT_LSR: return lsr(value, amount);
		case AARCH64_SHIFT_ASR: return asr(value, amount, size);
		case AARCH64_SHIFT_ROR: return ror(value, amount, size);
		default:                assert(false);
	}
}

/*
 * setreg
 *
 * Description:
 * 	Set the contents and state of the given register.
 */
static void
setreg(struct sim *sim, aarch64_reg d, uint64_t value, int state) {
	if (!AARCH64_REGZR(d)) {
		unsigned d0 = AARCH64_REGNAME(d);
		sim->X[d0].value = value & regmask(d);
		sim->X[d0].state = state;
	}
}

/*
 * exec_one
 *
 * Description:
 * 	Execute one instruction in the simulator.
 */
static bool
exec_one(struct sim *sim) {
	if (sim->count == 0) {
		return false;
	}
	if (sim->bl != 0) {
		sim_clear_temporary(sim);
		sim->bl = 0;
	}
	sim->ret = 0;
	uint32_t ins = *sim->ins;
	uint64_t pc = sim->pc;
	// Process the instruction, updating the state.
	int state = 0;
	struct aarch64_ins_adr adr;
	struct aarch64_ins_add_im add_im;
	struct aarch64_ins_movknz movz;
	struct aarch64_ins_and_orr_im orr_im;
	struct aarch64_ins_and_sr orr_sr;
	struct aarch64_ins_b b;
	struct aarch64_ins_ldr_str_ui ldr_ui;
	struct aarch64_ins_ldr_str_ui str_ui;
	struct aarch64_ins_ldp_stp ldp;
	struct aarch64_ins_ldp_stp stp;
	struct aarch64_ins_ret ret;
	if (aarch64_decode_adr(ins, pc, &adr)) {
		setreg(sim, adr.Xd, adr.label, VALUE);
	} else if (aarch64_decode_add_im(ins, &add_im)) {
		uint64_t value = getreg(sim, add_im.Rn, &state);
		uint64_t imm = (uint64_t)add_im.imm << add_im.shift;
		if (add_im.op == AARCH64_INS_ADD_IM_OP_ADD) {
			value += imm;
		} else {
			value -= imm;
		}
		setreg(sim, add_im.Rd, value, state);
	} else if (aarch64_ins_decode_movz(ins, &movz)) {
		uint64_t value = (uint64_t)movz.imm << movz.shift;
		setreg(sim, movz.Rd, value, VALUE);
	} else if (aarch64_ins_decode_orr_im(ins, &orr_im)) {
		uint64_t value = getreg(sim, orr_im.Rn, &state) | orr_im.imm;
		setreg(sim, orr_im.Rd, value, state);
	} else if (aarch64_decode_and_sr(ins, &orr_sr) && orr_sr.op == AARCH64_AND_SR_OP_ORR) {
		uint64_t value = getreg(sim, orr_sr.Rn, &state);
		value |= shiftreg(sim, orr_sr.Rm, orr_sr.shift, orr_sr.amount, &state);
		setreg(sim, orr_sr.Rd, value, state);
	} else if (aarch64_decode_b(ins, pc, &b) && b.link) {
		sim->bl = b.label;
	} else if (aarch64_ins_decode_ldr_ui(ins, &ldr_ui)) {
		setreg(sim, ldr_ui.Rt, 0, UNKNOWN);
	} else if (aarch64_ins_decode_ldp_post(ins, &ldp)
			|| aarch64_ins_decode_ldp_pre(ins, &ldp)
			|| aarch64_ins_decode_ldp_si(ins, &ldp)) {
		setreg(sim, ldp.Rt1, 0, UNKNOWN);
		setreg(sim, ldp.Rt2, 0, UNKNOWN);
	} else if (aarch64_ins_decode_str_ui(ins, &str_ui)
			|| aarch64_ins_decode_stp_si(ins, &stp)
			|| aarch64_ins_decode_stp_pre(ins, &stp)
			|| aarch64_ins_decode_stp_post(ins, &stp)) {
		// Ignore.
	} else if (aarch64_ins_decode_ret(ins, &ret)) {
		uint64_t retaddr = getreg(sim, ret.Xn, &state);
		sim->ret = (state == VALUE ? retaddr : 1);
	} else if (aarch64_ins_decode_nop(ins)) {
		// Nothing to do.
	} else {
		memctl_warning("unknown instruction: %x", ins); // TODO
		sim_clear(sim);
	}
	sim->ins++;
	sim->count--;
	sim->pc += sizeof(*sim->ins);
	return true;
}

/*
 * exec_until_function_call
 *
 * Description:
 * 	Execute the simulator until a function call.
 */
static bool
exec_until_function_call(struct sim *sim) {
	do {
		if (!exec_one(sim)) {
			return false;
		}
		if (sim->ret != 0) {
			return false;
		}
	} while (sim->bl == 0);
	return true;
}

/*
 * exec_until_return
 *
 * Description:
 * 	Execute the simulator until the function returns.
 */
static bool
exec_until_return(struct sim *sim) {
	do {
		if (!exec_one(sim)) {
			return false;
		}
	} while (sim->ret == 0);
	return true;
}

// Vtable discovery

/*
 * struct region
 *
 * Description:
 * 	Records information about a region of memory mapped by the Mach-O file..
 */
struct region {
	const void *data;
	kaddr_t     addr;
	size_t      size;
};

/*
 * region_contains
 *
 * Description:
 * 	Returns true if the region contains the given address.
 */
static bool
region_contains(const struct region *region, kaddr_t addr) {
	return region->addr <= addr && addr < region->addr + region->size;
}

/*
 * region_get
 *
 * Description:
 * 	Retrieves the contents of the region at the given address.
 */
static void
region_get(const struct region *region, kaddr_t addr, const void **data, size_t *size) {
	assert(region_contains(region, addr));
	kaddr_t offset = addr - region->addr;
	*data = (const void *)((uintptr_t)region->data + offset);
	*size = region->size - offset;
}

/*
 * region_address
 *
 * Description:
 * 	Get the address of the given data pointer in the memory region.
 */
static kaddr_t
region_address(const struct region *region, const void *data) {
	assert(region->data <= data);
	uintptr_t offset = (uintptr_t)data - (uintptr_t)region->data;
	assert(offset <= region->size);
	return region->addr + offset;
}

/*
 * struct context
 *
 * Description:
 * 	Context while processing a kernel extension to look for vtables.
 */
struct context {
	// Parameters and results.
	const char *bundle_id;
	const char *class_name;
	kaddr_t *vtable;
	size_t *vtable_size;
	kext_result kr;
	// Intermediate values.
	kaddr_t metaclass;
	// __DATA_CONST
	struct region data_const;
	// __DATA_CONST.__const
	struct region data_const_const;
	// __TEXT_EXEC
	struct region text_exec;
	// __TEXT
	struct region text;
	// __DATA
	struct region data;
	// __DATA_CONST.__mod_init_func
	const kaddr_t *mod_init_func;
	size_t         mod_init_func_count;
};

/*
 * context_set_kext
 *
 * Description:
 * 	Set up the context to analyze the kernel extension at the given base address.
 */
static bool
context_set_kext(struct context *context, const struct macho *macho) {
	const struct load_command *segment;
	const void *section;
	const void *data;
	uint64_t addr;
	size_t size;
	// __DATA_CONST
	segment = macho_find_segment(macho, "__DATA_CONST");
	if (segment == NULL) {
		return false;
	}
	macho_segment_data(macho, segment, &context->data_const.data, &addr,
			&context->data_const.size);
	context->data_const.addr = addr;
	// __DATA_CONST.__mod_init_func
	section = macho_find_section(macho, segment, "__mod_init_func");
	if (section == NULL) {
		return false;
	}
	macho_section_data(macho, segment, section, &data, &addr, &size);
	assert(size % sizeof(kaddr_t) == 0);
	context->mod_init_func = (const kaddr_t *)data;
	context->mod_init_func_count = size / sizeof(kaddr_t);
	// __DATA_CONST.__const
	section = macho_find_section(macho, segment, "__const");
	if (section == NULL) {
		return false;
	}
	macho_section_data(macho, segment, section, &context->data_const_const.data, &addr,
			&context->data_const_const.size);
	context->data_const_const.addr = addr;
	// __TEXT_EXEC
	segment = macho_find_segment(macho, "__TEXT_EXEC");
	if (segment == NULL) {
		return false;
	}
	macho_segment_data(macho, segment, &context->text_exec.data, &addr,
			&context->text_exec.size);
	context->text_exec.addr = addr;
	// __TEXT
	segment = macho_find_segment(macho, "__TEXT");
	if (segment == NULL) {
		return false;
	}
	macho_segment_data(macho, segment, &context->text.data, &addr, &context->text.size);
	context->text.addr = addr;
	// __DATA
	segment = macho_find_segment(macho, "__DATA");
	if (segment == NULL) {
		return false;
	}
	macho_segment_data(macho, segment, &context->data.data, &addr, &context->data.size);
	context->data.addr = addr;
	return true;
}

/*
 * get_metaclass_from_call
 *
 * Description:
 * 	Check if the function call at which the simulator stopped is a call to construct the
 * 	metaclass we are searching for.
 */
static bool
get_metaclass_from_call(struct context *c, struct sim *sim) {
	kword_t arg[4];
	for (size_t i = 0; i < 4; i++) {
		if (i != 2 && sim->X[i].state != VALUE) {
			return false;
		}
		arg[i] = sim->X[i].value;
	}
	if (!region_contains(&c->data, arg[0])) {
		return false;
	}
	if (!region_contains(&c->text, arg[1])) {
		return false;
	}
	if (arg[3] > UINT32_MAX) {
		return false;
	}
	const void *str;
	size_t size;
	region_get(&c->text, arg[1], &str, &size);
	if (strncmp(str, c->class_name, size) != 0) {
		return false;
	}
	c->metaclass = arg[0];
	return true;
}

/*
 * find_metaclass_from_initializer
 *
 * Description:
 * 	Simulate the given initialization function to search for the metaclass.
 */
static bool
find_metaclass_from_initializer(struct context *c, kaddr_t mod_init_func) {
	struct sim sim;
	sim_clear(&sim);
	sim.pc = mod_init_func;
	region_get(&c->text_exec, mod_init_func, (const void **)&sim.ins, &sim.count);
	sim.count /= sizeof(*sim.ins);
	for (;;) {
		if (!exec_until_function_call(&sim)) {
			return false;
		}
		if (get_metaclass_from_call(c, &sim)) {
			return true;
		}
	}
}

/*
 * check_metaclass_at_return
 *
 * Description:
 * 	Check whether the method seems to be returning the metaclass pointer.
 */
static bool
check_metaclass_at_return(struct context *c, struct sim *sim) {
	return (sim->X[0].state == VALUE && sim->X[0].value == c->metaclass);
}

#define MAX_GETMETACLASS_INSTRUCTION_COUNT 8

/*
 * method_is_getmetaclass
 *
 * Description:
 * 	Simulate the given method to see if it seems to be returning the metaclass pointer.
 */
static bool
method_is_getmetaclass(struct context *c, kaddr_t method) {
	if (!region_contains(&c->text_exec, method)) {
		return false;
	}
	struct sim sim;
	sim_clear(&sim);
	sim.pc = method;
	region_get(&c->text_exec, method, (const void **)&sim.ins, &sim.count);
	sim.count /= sizeof(*sim.ins);
	if (sim.count > MAX_GETMETACLASS_INSTRUCTION_COUNT) {
		sim.count = MAX_GETMETACLASS_INSTRUCTION_COUNT;
	}
	if (!exec_until_return(&sim)) {
		return false;
	}
	if (check_metaclass_at_return(c, &sim)) {
		return true;
	}
	return false;
}

#define NMETHODS           12
#define GETMETACLASS_INDEX 7

/*
 * find_vtable_for_metaclass
 *
 * Description:
 * 	Search through the __DATA_CONST.__const section for the vtable.
 */
static bool
find_vtable_for_metaclass(struct context *c) {
	// Look for a vtable whose 7th method returns the metaclass pointer.
	const kaddr_t *v = c->data_const_const.data;
	const kaddr_t *end = v + c->data_const_const.size / sizeof(*v);
	for (; v + 2 + NMETHODS <= end; v++) {
		if (v[2] == 0 || v[0] != 0 || v[1] != 0) {
			continue;
		}
		for (size_t i = 2 + 1; i < 2 + NMETHODS; i++) {
			if (v[i] == 0) {
				goto next;
			}
		}
		if (method_is_getmetaclass(c, v[2 + GETMETACLASS_INDEX])) {
			goto found;
		}
next:;
	}
	return false;
found:
	// Skip the first 2 (empty) slots in the vtable.
	v += 2;
	*c->vtable = region_address(&c->data_const_const, v) + kernel_slide;
	if (c->vtable_size != NULL) {
		// The vtable runs until the end of the section or the first NULL pointer.
		size_t i = NMETHODS;
		while (v + i < end && v[i] != 0) {
			i++;
		}
		*c->vtable_size = i * sizeof(kaddr_t);
	}
	return true;
}

/*
 * find_vtable_by_static_analysis
 *
 * Description:
 * 	Find the vtable for the given class in the given kext by:
 * 	  1. parsing the kext's Mach-O file to find the initialization functions;
 * 	  2. disassembling the initialization functions to find the class's OSMetaClass instance;
 * 	  3. searching through the __DATA_CONST.__const section for possible virtual method tables;
 * 	     and
 * 	  4. disassembling the virtual method table's getMetaClass method to see if it returns the
 * 	     OSMetaClass instance found earlier.
 */
static bool
find_vtable_by_static_analysis(struct context *c, const struct macho *macho) {
	if (!context_set_kext(c, macho)) {
		return false;
	}
	for (size_t i = 0; i < c->mod_init_func_count; i++) {
		if (find_metaclass_from_initializer(c, c->mod_init_func[i])) {
			return find_vtable_for_metaclass(c);
		}
	}
	return false;
}

/*
 * find_vtable_by_symbol
 *
 * Description:
 * 	Find the vtable for the given class in the given kext by looking up the vtable's symbol.
 *
 * Returns:
 * 	KEXT_SUCCESS, KEXT_ERROR, or KEXT_NOT_FOUND.
 */
static kext_result
find_vtable_by_symbol(struct context *c, const struct macho *macho) {
	const struct symtab_command *symtab = (const struct symtab_command *)
		macho_find_load_command(macho, NULL, LC_SYMTAB);
	if (symtab == NULL) {
		return KEXT_NOT_FOUND;
	}
	char *symbol = vtable_symbol(c->class_name);
	if (symbol == NULL) {
		return KEXT_ERROR;
	}
	uint64_t vtable_addr;
	macho_result mr = macho_resolve_symbol(macho, symtab, symbol, &vtable_addr,
			c->vtable_size);
	free(symbol);
	if (mr == MACHO_SUCCESS) {
		*c->vtable = vtable_addr + kernel_slide;
		adjust_vtable_from_symbol(c->vtable, c->vtable_size);
		return KEXT_SUCCESS;
	} else if (mr == MACHO_ERROR) {
		return KEXT_ERROR;
	} else {
		assert(mr == MACHO_NOT_FOUND);
		return KEXT_NOT_FOUND;
	}
}

/*
 * check_kext_for_vtable
 *
 * Description:
 * 	A kernelcache_for_each callback to check the given kernel extension for the desired vtable.
 */
static bool
check_kext_for_vtable(void *context, CFDictionaryRef info, const char *bundle_id, kaddr_t base,
		size_t size) {
	struct context *c = context;
	// If the caller is looking for a particular kernel extension and this one isn't it, skip
	// it.
	if (c->bundle_id != NULL && strcmp(c->bundle_id, bundle_id) != 0) {
		return false;
	}
	// At least one kext matched, so change the return value to KEXT_NOT_FOUND rather than
	// KEXT_NO_KEXT.
	c->kr = KEXT_NOT_FOUND;
	// Initialize the Mach-O for the kernel extension at the given base address.
	if (base == 0) {
		goto not_found;
	}
	struct macho macho;
	kext_result kr = kernelcache_kext_init_macho_at_address(&kernelcache, &macho, base);
	if (kr != KEXT_SUCCESS) {
		assert(kr == KEXT_NO_KEXT);
		goto not_found;
	}
	// See if we can find the vtable symbol.
	kr = find_vtable_by_symbol(c, &macho);
	if (kr != KEXT_NOT_FOUND) {
		c->kr = kr;
		return true;
	}
	// Symbol resolution failed, so try to find the vtable by static analysis.
	if (!find_vtable_by_static_analysis(c, &macho)) {
		goto not_found;
	}
	c->kr = KEXT_SUCCESS;
	return true;
not_found:
	// If we were looking for a particular bundle ID, stop searching through the kernelcache.
	return c->bundle_id != NULL;
}

/*
 * vtable_for_class_disassemble
 *
 * Description:
 * 	Scan the kernelcache looking for the vtable for the given class.
 *
 * Returns:
 * 	KEXT_SUCCESS, KEXT_ERROR, KEXT_NO_KEXT, or KEXT_NOT_FOUND.
 */
static kext_result
vtable_for_class_disassemble(const char *class_name, const char *bundle_id, kaddr_t *vtable,
		size_t *size) {
	struct context context = { bundle_id, class_name, vtable, size, KEXT_NO_KEXT };
	kernelcache_for_each(&kernelcache, check_kext_for_vtable, &context);
	return context.kr;
}

#else

/*
 * vtable_for_class_symbol
 *
 * Description:
 * 	Look up the vtable for the given class in the macOS kernel by its symbol.
 *
 * Returns:
 * 	KEXT_SUCCESS, KEXT_ERROR, KEXT_NO_KEXT, or KEXT_NOT_FOUND
 */
static kext_result
vtable_for_class_symbol(const char *class_name, const char *bundle_id, kaddr_t *vtable,
		size_t *size) {
	// Generate the vtable symbol.
	char *symbol = vtable_symbol(class_name);
	if (symbol == NULL) {
		return KEXT_ERROR;
	}
	kext_result kr = resolve_symbol(bundle_id, symbol, vtable, size);
	free(symbol);
	if (kr == KEXT_SUCCESS) {
		adjust_vtable_from_symbol(vtable, size);
	} else if (kr == KEXT_NO_SYMBOLS) {
		kr = KEXT_NOT_FOUND;
	}
	return kr;
}

#endif

kext_result
vtable_for_class(const char *class_name, const char *bundle_id, kaddr_t *vtable, size_t *size) {
#if VTABLE_FOR_CLASS_DISASSEMBLE
	return vtable_for_class_disassemble(class_name, bundle_id, vtable, size);
#else
	return vtable_for_class_symbol(class_name, bundle_id, vtable, size);
#endif
}
