#include "memctl/vtable.h"

#include "memctl/kernel.h"
#include "memctl/memctl_error.h"
#include "memctl/utility.h"

#if __arm64__
#include "memctl/aarch64/disasm.h"
#include "memctl/aarch64/ksim.h"
#include "memctl/kernel_slide.h"
#include "memctl/kernelcache.h"
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

// Kernel extension memory regions

/*
 * struct region
 *
 * Description:
 * 	Records information about a region of memory mapped by the Mach-O file.
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

// Vtable discovery

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
 * break_at_branch_and_link
 *
 * Description:
 * 	A ksim_run_until_fn callback to break execution once a branch instruction is hit.
 */
static bool
break_at_branch_and_link(struct ksim *ksim, uint32_t ins) {
	return AARCH64_INS_TYPE(ins, BL_INS);
}

/*
 * break_at_return
 *
 * Description:
 * 	A ksim_run_until_fn callback to break execution once a return instruction is hit.
 */
static bool
break_at_return(struct ksim *ksim, uint32_t ins) {
	return AARCH64_INS_TYPE(ins, RET_INS);
}

/*
 * get_metaclass_from_call
 *
 * Description:
 * 	Check if the function call at which the simulator stopped is a call to construct the
 * 	metaclass we are searching for.
 */
static bool
get_metaclass_from_call(struct context *c, struct ksim *ksim) {
	uint64_t arg0, arg1, arg3;
	if (!ksim_reg(ksim, AARCH64_X0, &arg0)
			|| !ksim_reg(ksim, AARCH64_X1,  &arg1)
			|| !ksim_reg(ksim, AARCH64_X3, &arg3)) {
		return false;
	}
	if (!region_contains(&c->data, arg0)
			|| !region_contains(&c->text, arg1)
			|| arg3 >= UINT32_MAX) {
		return false;
	}
	const void *str;
	size_t size;
	region_get(&c->text, arg1, &str, &size);
	if (strncmp(str, c->class_name, size) != 0) {
		return false;
	}
	c->metaclass = arg0;
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
	struct ksim ksim;
	ksim_init(&ksim, c->text_exec.data, c->text_exec.size, c->text_exec.addr, mod_init_func);
	ksim.run_until = break_at_branch_and_link;
	for (;;) {
		if (!ksim_run(&ksim)) {
			return false;
		}
		if (get_metaclass_from_call(c, &ksim)) {
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
check_metaclass_at_return(struct context *c, struct ksim *ksim) {
	uint64_t ret;
	return (ksim_reg(ksim, AARCH64_X0, &ret)
			&& ret == c->metaclass);
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
	struct ksim ksim;
	ksim_init(&ksim, c->text_exec.data, c->text_exec.size, c->text_exec.addr, method);
	ksim.max_instruction_count = MAX_GETMETACLASS_INSTRUCTION_COUNT;
	ksim.run_until = break_at_return;
	if (!ksim_run(&ksim)) {
		return false;
	}
	return check_metaclass_at_return(c, &ksim);
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
	// Try to find the vtable by static analysis.
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

#endif

/*
 * vtable_for_class_symbol
 *
 * Description:
 * 	Look up the vtable for the given class by its symbol.
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
	// Search all the kexts.
	kext_result kr = resolve_symbol(bundle_id, symbol, vtable, size);
	free(symbol);
	if (kr == KEXT_SUCCESS) {
		adjust_vtable_from_symbol(vtable, size);
	} else if (kr == KEXT_NO_SYMBOLS) {
		kr = KEXT_NOT_FOUND;
	}
	return kr;
}

kext_result
vtable_for_class(const char *class_name, const char *bundle_id, kaddr_t *vtable, size_t *size) {
	kext_result kr = vtable_for_class_symbol(class_name, bundle_id, vtable, size);
#if VTABLE_FOR_CLASS_DISASSEMBLE
	if (kr == KEXT_NOT_FOUND) {
		kr = vtable_for_class_disassemble(class_name, bundle_id, vtable, size);
	}
#endif
	return kr;
}
