#include "aarch64/finder/vtables.h"

/*
 * How we find vtables in the kernelcache
 * --------------------------------------
 *
 *  Each kernel extension that declares OSObject subclasses has a number of initialization
 *  functions in the __DATA_CONST.__mod_init_func section of the Mach-O. These initialization
 *  functions construct the OSMetaClass object associated with each class. The OSMetaClass
 *  constructor is:
 *
 *  	OSMetaClass::OSMetaClass(char const*, OSMetaClass const*, unsigned int)
 *
 *  The first argument to the constructor (excluding the implicit this pointer) is the name of the
 *  class represented by this OSMetaClass object. By disassembling every initialization function
 *  declared by a kernel extension and looking at this argument, we can detect which OSMetaClass
 *  instance is associated with each class name.
 *
 *  The reason this OSMetaClass object is useful is that all OSObject subclasses override the
 *  getMetaClass method:
 *
 *  	virtual const OSMetaClass * getMetaClass() const
 *
 *  The override is defined in the OSDefineMetaClassWithInit macro, which must be called for every
 *  OSObject subclass. Thus, every virtual method table corresponding to a subclass of OSObject
 *  contains a method that returns the associated OSMetaClass instance.
 *
 *  Thus, if we know which OSMetaClass instance is associated with the given class name, we can
 *  disassemble the getMetaClass method in every virtual method table to find which one returns the
 *  correct OSMetaClass instance. This will be the virtual method table we are looking for.
 *
 *  Our strategy for finding all the vtables in a kext is:
 *  	1. First, iterate each initialization function to find each possible call to the
 *  	   OSMetaClass constructor. This gives us a mapping from OSMetaClass instances to class
 *  	   names, although this mapping may include false positives.
 *  	2. Once we have collected all possible OSMetaClass instances, scan the __DATA_CONST segment
 *  	   of the kext looking for possible vtables.
 *  	3. For each vtable candidate, simulate the entry corresponding to getMetaClass and check
 *  	   whether it returns an OSMetaClass instance discovered earlier.
 *  	4. If it does, then add the vtable and metaclass symbols. (We could also add the metaclass
 *  	   vtable as a symbol, but right now we don't.)
 */

#include "memctl/aarch64/ksim.h"
#include "memctl/vtable.h"

#include "mangle.h"

/*
 * struct state
 *
 * Description:
 * 	State while processing a kernel extension to look for vtables.
 */
struct state {
	/* Regions of the kext. */
	struct mapped_region data_const;        // __DATA_CONST
	struct mapped_region data_const_const;  // __DATA_CONST.__const
	struct mapped_region text_exec;         // __TEXT_EXEC
	struct mapped_region text;              // __TEXT
	struct mapped_region data;              // __DATA

	/* The initialization functions. */
	const kaddr_t *mod_init_func;           // __DATA_CONST.__mod_init_func
	size_t         mod_init_func_count;

	/* Mapping from possible OSMetaClass instances to class names. */
	kaddr_t *    metaclass_instances;
	const char **class_names;
	size_t       class_count;
};

/*
 * init_state_regions
 *
 * Description:
 * 	Initialize the regions in the state struct.
 */
static bool
init_state_regions(struct state *state, const struct macho *macho) {
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
	macho_segment_data(macho, segment, &state->data_const.data, &addr,
			&state->data_const.size);
	state->data_const.addr = addr;
	// __DATA_CONST.__mod_init_func
	section = macho_find_section(macho, segment, "__mod_init_func");
	if (section == NULL) {
		return false;
	}
	macho_section_data(macho, segment, section, &data, &addr, &size);
	assert(size % sizeof(kaddr_t) == 0);
	state->mod_init_func       = (const kaddr_t *)data;
	state->mod_init_func_count = size / sizeof(kaddr_t);
	// __DATA_CONST.__const
	section = macho_find_section(macho, segment, "__const");
	if (section == NULL) {
		return false;
	}
	macho_section_data(macho, segment, section, &state->data_const_const.data, &addr,
			&state->data_const_const.size);
	state->data_const_const.addr = addr;
	// __TEXT_EXEC
	segment = macho_find_segment(macho, "__TEXT_EXEC");
	if (segment == NULL) {
		return false;
	}
	macho_segment_data(macho, segment, &state->text_exec.data, &addr, &state->text_exec.size);
	state->text_exec.addr = addr;
	// __TEXT
	segment = macho_find_segment(macho, "__TEXT");
	if (segment == NULL) {
		return false;
	}
	macho_segment_data(macho, segment, &state->text.data, &addr, &state->text.size);
	state->text.addr = addr;
	// __DATA
	segment = macho_find_segment(macho, "__DATA");
	if (segment == NULL) {
		return false;
	}
	macho_segment_data(macho, segment, &state->data.data, &addr, &state->data.size);
	state->data.addr = addr;
	return true;
}

/*
 * deinit_state
 *
 * Description:
 * 	Clean up resources allocated in init_state.
 */
static void
deinit_state(struct state *state) {
	free(state->metaclass_instances);
	free(state->class_names);
}

/*
 * init_state
 *
 * Description:
 * 	Set up the state to analyze the kernel extension.
 */
static bool
init_state(struct state *state, const struct kext *kext) {
	state->metaclass_instances = NULL;
	state->class_names         = NULL;
	state->class_count         = 0;
	return init_state_regions(state, &kext->macho);
}

/*
 * add_metaclass
 *
 * Description:
 * 	Add a potential association between an OSMetaClass instance and a class name.
 */
static bool
add_metaclass(struct state *state, kaddr_t metaclass, const char *class_name) {
	assert(metaclass != 0 && class_name != NULL);
	size_t count = state->class_count + 1;
	// Grow the arrays.
	kaddr_t *metaclass_instances = realloc(state->metaclass_instances,
			count * sizeof(*state->metaclass_instances));
	if (metaclass_instances == NULL) {
		return false;
	}
	state->metaclass_instances = metaclass_instances;
	const char **class_names = realloc(state->class_names,
			count * sizeof(*state->class_names));
	if (class_names == NULL) {
		return false;
	}
	state->class_names = class_names;
	// Add the new metaclass and class_name.
	state->metaclass_instances[count - 1] = metaclass;
	state->class_names[count - 1] = class_name;
	state->class_count = count;
	return true;
}

/*
 * get_class_name_for_metaclass
 *
 * Description:
 * 	Search the associations we've gathered to find which class name, if any, is associated with
 * 	the given metaclass.
 */
static const char *
get_class_name_for_metaclass(struct state *state, kaddr_t metaclass) {
	for (size_t i = 0; i < state->class_count; i++) {
		if (state->metaclass_instances[i] == metaclass) {
			return state->class_names[i];
		}
	}
	return NULL;
}

/*
 * collect_metaclass_from_call
 *
 * Description:
 * 	Check if the function call at which the simulator stopped is a call to construct an
 * 	OSMetaClass instance. If it seems likely, add an association between the OSMetaClass
 * 	instance's address and the class name.
 */
static bool
collect_metaclass_from_call(struct state *state, struct ksim *ksim) {
	kword_t arg0_metaclass  = ksim_reg(ksim, AARCH64_X0);
	kword_t arg1_class_name = ksim_reg(ksim, AARCH64_X1);
	kword_t arg3_size       = ksim_reg(ksim, AARCH64_X3);
	if (arg0_metaclass == 0 || arg1_class_name == 0 || arg3_size == 0) {
		return true;
	}
	if (!mapped_region_contains(&state->data, arg0_metaclass, sizeof(kword_t))
	    || !mapped_region_contains(&state->text, arg1_class_name, 2)
	    || arg3_size >= UINT32_MAX) {
		return true;
	}
	const char *class_name = mapped_region_get(&state->text, arg1_class_name, NULL);
	return add_metaclass(state, arg0_metaclass, class_name);
}

/*
 * collect_metaclasses_from_initializer
 *
 * Description:
 * 	Simulate the initializer, looking for calls to the OSMetaClass constructor. For each call
 * 	site, associate the OSMetaClass instance with the name of the class it describes.
 */
static bool
collect_metaclasses_from_initializer(struct state *state, kaddr_t mod_init_func) {
	struct ksim ksim;
	ksim_init_sim(&ksim, mod_init_func);
	for (;;) {
		if (!ksim_exec_until_call(&ksim, NULL, NULL, 256)) {
			return true;
		}
		if (!collect_metaclass_from_call(state, &ksim)) {
			return false;
		}
	}
}

/*
 * collect_metaclasses
 *
 * Description:
 * 	Parse each mod_init_func, collecting mappings from metaclass instances to class names.
 */
static bool
collect_metaclasses(struct state *state) {
	bool success = true;
	for (size_t i = 0; success && i < state->mod_init_func_count; i++) {
		success = collect_metaclasses_from_initializer(state, state->mod_init_func[i]);
	}
	return success;
}

/*
 * add_symbols
 *
 * Description:
 * 	Add symbols for the OSMetaClass instance and virtual method table for the given class name.
 */
static void
add_symbols(struct symbol_table *symtab, const char *class_name, kaddr_t metaclass,
		kaddr_t vtable) {
	// Skip the symbols if the vtable address is already known.
	size_t offset;
	if (symbol_table_resolve_address(symtab, vtable, NULL, NULL, &offset) && offset == 0) {
		return;
	}
	// Add the vtable symbol.
	size_t vtable_symbol_size = mangle_class_vtable(NULL, 0, &class_name, 1) + 1;
	char vtable_symbol[vtable_symbol_size];
	mangle_class_vtable(vtable_symbol, vtable_symbol_size, &class_name, 1);
	bool success = symbol_table_add_symbol(symtab, vtable_symbol, vtable);
	if (!success) {
		return;
	}
	// Add the metaclass symbol.
	const char *metaclass_name[2] = { class_name, "gMetaClass" };
	size_t metaclass_symbol_size = mangle_class_name(NULL, 0, metaclass_name, 2) + 1;
	char metaclass_symbol[metaclass_symbol_size];
	mangle_class_name(metaclass_symbol, metaclass_symbol_size, metaclass_name, 2);
	symbol_table_add_symbol(symtab, metaclass_symbol, metaclass);
}

#define MIN_GETMETACLASS_INSTRUCTION_COUNT      2
#define MAX_GETMETACLASS_INSTRUCTION_COUNT      8
#define MIN_GETMETACLASS_SIZE   (MIN_GETMETACLASS_INSTRUCTION_COUNT * AARCH64_INSTRUCTION_SIZE)

/*
 * simulate_getMetaClass
 *
 * Description:
 * 	Simulate the given method to see if it seems to be returning an OSMetaClass pointer.
 */
static kaddr_t
simulate_getMetaClass(struct state *state, kaddr_t method) {
	if (!mapped_region_contains(&state->text_exec, method, MIN_GETMETACLASS_SIZE)) {
		return 0;
	}
	struct ksim ksim;
	ksim_init_sim(&ksim, method);
	if (!ksim_exec_until_return(&ksim, NULL, MAX_GETMETACLASS_INSTRUCTION_COUNT)) {
		return 0;
	}
	return ksim_reg(&ksim, AARCH64_X0);
}

#define NMETHODS           12
#define GETMETACLASS_INDEX 7

/*
 * search_for_vtables
 *
 * Description:
 * 	Search through the __DATA_CONST.__const section for possible virtual method tables. For
 * 	each possible vtable, disassemble the getMetaClass method to see if it returns an
 * 	OSMetaClass instance found earlier. If it does, add symbols sfor the vtable and the
 * 	OSMetaClass instance.
 */
static void
search_for_vtables(struct state *state, struct kext *kext) {
	// Look for a vtable whose 7th method returns a metaclass pointer.
	const kaddr_t *v = state->data_const_const.data;
	const kaddr_t *end = v + state->data_const_const.size / sizeof(*v);
	for (; v + VTABLE_OFFSET + NMETHODS <= end; v++) {
		// Make sure all entries prior to VTABLE_OFFSET are empty.
		for (size_t i = 0; i < VTABLE_OFFSET; i++) {
			if (v[i] != 0) {
				goto next;
			}
		}
		// Make sure that the NMETHODS entries after VTABLE_OFFSET are nonempty.
		for (size_t i = VTABLE_OFFSET; i < VTABLE_OFFSET + NMETHODS; i++) {
			if (v[i] == 0) {
				goto next;
			}
		}
		// Simulate the getMetaClass method.
		kaddr_t getMetaClass = v[VTABLE_OFFSET + GETMETACLASS_INDEX];
		kaddr_t metaclass = simulate_getMetaClass(state, getMetaClass);
		if (metaclass == 0) {
			goto next;
		}
		// Check if this OSMetaClass instance has an associated class name.
		const char *class_name = get_class_name_for_metaclass(state, metaclass);
		if (class_name == NULL) {
			goto next;
		}
		// So, we now think that this OSMetaClass instance is valid. Get the vtable address
		// and try to add the symbols.
		kaddr_t vtable = mapped_region_address(&state->data_const_const, v);
		add_symbols(&kext->symtab, class_name, metaclass, vtable);
next:;
	}
}

void
kext_find_vtables(struct kext *kext) {
	struct state state;
	if (!init_state(&state, kext)) {
		return;
	}
	if (!collect_metaclasses(&state)) {
		goto end;
	}
	search_for_vtables(&state, kext);
end:
	deinit_state(&state);
}
