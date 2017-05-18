#include "cli.h"
#include "disassemble.h"
#include "error.h"
#include "find.h"
#include "format.h"
#include "memory.h"
#include "read.h"
#include "vmmap.h"

#include "memctl/core.h"
#include "memctl/kernel.h"
#include "memctl/kernel_call.h"
#include "memctl/kernel_slide.h"
#include "memctl/kernelcache.h"
#include "memctl/memctl_offsets.h"
#include "memctl/memctl_signal.h"
#include "memctl/platform.h"
#include "memctl/vtable.h"

#include <mach/mach_vm.h>
#include <stdio.h>

#if MEMCTL_REPL
#include <histedit.h>
#endif

/*
 * feature_t
 *
 * Description:
 * 	Flags for libmemctl and memctl features.
 */
typedef enum {
	KERNEL_TASK         = 0x01,
	KERNEL_MEMORY_BASIC = 0x02 | KERNEL_TASK,
	KERNEL_IMAGE        = 0x04,
	KERNEL_SLIDE        = 0x08 | KERNEL_TASK | KERNEL_IMAGE,
	KERNEL_SYMBOLS      = KERNEL_IMAGE | KERNEL_SLIDE,
	OFFSETS             = 0x10,
	KERNEL_CALL         = 0x20 | KERNEL_MEMORY_BASIC | KERNEL_SYMBOLS | OFFSETS,
	KERNEL_MEMORY       = 0x40 | KERNEL_CALL,
} feature_t;

/*
 * loaded_features
 *
 * Description:
 * 	The libmemctl or memctl features that have already been loaded.
 */
static feature_t loaded_features;

#if MEMCTL_REPL

/*
 * in_repl
 *
 * Description:
 * 	True if we are currently in a REPL.
 */
static bool in_repl;

/*
 * prompt_string
 *
 * Description:
 * 	The REPL prompt string.
 */
static char *prompt_string;

#endif // MEMCTL_REPL

// Forward declaration.
static void deinitialize(bool critical);

/*
 * signal_handler
 *
 * Description:
 * 	The signal handler for terminating signals. When we receive a signal, libmemctl's
 * 	interrupted global is set to 1. If the signal is not SIGINT, then all critical state is
 * 	de-initialized and the signal is re-raised.
 */
static void
signal_handler(int signum) {
	if (interrupted && signum == SIGINT) {
		deinitialize(true);
		exit(1);
	}
	interrupted = 1;
	if (signum != SIGINT) {
		deinitialize(true);
		raise(signum);
	}
}

/*
 * install_signal_handler
 *
 * Description:
 * 	Install the signal handler for all terminating signals. For all signals besides SIGINT, the
 * 	signal handler will be reset to SIG_DFL the first time it is received. This allows us to
 * 	re-raise the signal to kill the program.
 */
static bool
install_signal_handler() {
	int signals[] = {
		SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGEMT, SIGFPE, SIGBUS,
		SIGSEGV, SIGSYS, SIGPIPE, SIGALRM, SIGTERM, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF,
		SIGUSR1, SIGUSR2,
	};
	struct sigaction act = { .sa_handler = signal_handler };
	for (size_t i = 0; i < sizeof(signals) / sizeof(*signals); i++) {
		act.sa_flags = (signals[i] == SIGINT ? 0 : SA_RESETHAND);
		int err = sigaction(signals[i], &act, NULL);
		if (err < 0) {
			error_internal("could not install signal handler");
			return false;
		}
	}
	return true;
}

/*
 * default_initialize
 *
 * Description:
 * 	Initialize the basic set of features that should always be present.
 */
static bool
default_initialize() {
	platform_init();
	if (!install_signal_handler()) {
		return false;
	}
	return true;
}

// Test if all bits set in x are also set in y.
#define ALL_SET(x,y)	(((x) & (y)) == (x))

/*
 * initialize
 *
 * Description:
 * 	Initialize the required functionality from libmemctl or memctl.
 */
static bool
initialize(feature_t features) {
#define NEED(feature)	(ALL_SET(feature, features) && !ALL_SET(feature, loaded_features))
#define LOADED(feature)	loaded_features |= feature
	if (NEED(KERNEL_TASK)) {
		if (!core_load()) {
			error_message("could not load libmemctl core");
			return false;
		}
		LOADED(KERNEL_TASK);
	}
	if (NEED(KERNEL_MEMORY_BASIC)) {
		if (!kernel_memory_init()) {
			error_message("could not initialize kernel memory");
			return false;
		}
		LOADED(KERNEL_MEMORY_BASIC);
	}
	if (NEED(KERNEL_IMAGE)) {
		if (!kernel_init(NULL)) {
			error_message("could not initialize kernel image");
			return false;
		}
		LOADED(KERNEL_IMAGE);
	}
	if (NEED(KERNEL_SLIDE)) {
		if (!kernel_slide_init()) {
			error_message("could not find the kASLR slide");
			return false;
		}
		// Re-initialize the kernel image.
		kernel_init(NULL);
		LOADED(KERNEL_SLIDE);
	}
	if (NEED(OFFSETS)) {
		offsets_default_init();
		LOADED(OFFSETS);
	}
	if (NEED(KERNEL_CALL)) {
		printf("setting up kernel function call...\n");
		if (!kernel_call_init()) {
			error_message("could not set up kernel function call system");
			return false;
		}
		LOADED(KERNEL_CALL);
	}
	if (NEED(KERNEL_MEMORY)) {
		if (!kernel_memory_init()) {
			error_message("could not initialize kernel memory");
			return false;
		}
		LOADED(KERNEL_MEMORY);
	}
	assert(ALL_SET(features, loaded_features));
	return true;
#undef NEED
#undef LOADED
}

/*
 * deinitialize
 *
 * Description:
 * 	Unload loaded features.
 *
 * Parameters:
 * 		critical		If true, only system-critical features will be unloaded.
 * 					Otherwise, all features will be unloaded.
 *
 * Notes:
 * 	After this call, loaded_features is reset to 0, even if some features are not unloaded.
 */
static void
deinitialize(bool critical) {
#define LOADED(feature)	(ALL_SET(feature, loaded_features))
	if (LOADED(KERNEL_CALL)) {
		kernel_call_deinit();
	}
	if (!critical && LOADED(KERNEL_IMAGE)) {
		kernel_deinit();
	}
	if (critical) {
		fprintf(stderr, "deinitialized\n");
	}
	loaded_features = 0;
#undef LOADED
}

#if MEMCTL_REPL

/*
 * repl_prompt
 *
 * Description:
 * 	A callback for libedit.
 */
static char *
repl_prompt(EditLine *el) {
	return prompt_string;
}

/*
 * repl_getc
 *
 * Description:
 * 	A callback for libedit to read a character.
 */
static int
repl_getc(EditLine *el, char *c) {
	for (;;) {
		errno = 0;
		int ch = fgetc(stdin);
		if (ch != EOF) {
			*c = ch;
			return 1;
		}
		if (interrupted || errno == 0) {
			return 0;
		}
		if (errno != EINTR) {
			return -1;
		}
		// Try again for EINTR.
	}
}

/*
 * run_repl
 *
 * Description:
 * 	Run a memctl REPL.
 */
static bool
run_repl() {
	assert(!in_repl);
	bool success = false;
	EditLine *el = el_init(getprogname(), stdin, stdout, stderr);
	if (el == NULL) {
		error_out_of_memory();
		goto fail0;
	}
	Tokenizer *tok = tok_init(NULL);
	if (tok == NULL) {
		error_out_of_memory();
		goto fail1;
	}
	History *hist = history_init();
	if (hist == NULL) {
		error_out_of_memory();
		goto fail2;
	}
	HistEvent ev;
	history(hist, &ev, H_SETSIZE, 256);
	el_set(el, EL_HIST, history, hist);
	el_set(el, EL_SIGNAL, 0);
	el_set(el, EL_PROMPT, repl_prompt);
	el_set(el, EL_EDITOR, "emacs");
	el_set(el, EL_GETCFN, repl_getc);
	in_repl = true;
	asprintf(&prompt_string, "%s> ", getprogname());
	while (in_repl) {
		int n;
		interrupted = 0;
		const char *line = el_gets(el, &n);
		if (interrupted) {
			fprintf(stdout, "^C\n");
			interrupted = 0;
			continue;
		}
		if (line == NULL || n == 0) {
			printf("\n");
			success = true;
			break;
		}
		int argc;
		const char **argv;
		tok_str(tok, line, &argc, &argv);
		if (argc > 0) {
			history(hist, &ev, H_ENTER, line);
			command_run_argv(argc, argv);
		}
		tok_reset(tok);
		print_errors();
	}
	free(prompt_string);
	in_repl = false;
	history_end(hist);
fail2:
	tok_end(tok);
fail1:
	el_end(el);
fail0:
	return success;
}

#endif // MEMCTL_REPL

/*
 * looks_like_physical_address
 *
 * Description:
 * 	Returns true if the given address looks like it could be a physical address.
 */
static bool
looks_like_physical_address(paddr_t address) {
#if KERNEL_BITS == 32
	return true;
#else
	return ((address & 0xffff000000000000) == 0);
#endif
}

/*
 * looks_like_kernel_address
 *
 * Description:
 * 	Returns true if the given address looks like it could be a kernel address.
 */
static bool
looks_like_kernel_address(kaddr_t address) {
#if KERNEL_BITS == 32
	return (address >= 0xc0000000);
#else
	return ((address >> 40) == 0xffffff);
#endif
}

/*
 * check_address
 *
 * Description:
 * 	Checks that the given address looks valid.
 */
static bool
check_address(kaddr_t address, size_t length, bool physical) {
	if (address + length < address) {
		error_usage(NULL, NULL, "overflow at address "KADDR_XFMT, address);
		return false;
	}
	if (physical) {
		if (!looks_like_physical_address(address)) {
			error_usage(NULL, NULL, "address "KADDR_XFMT" does not look like a "
			            "physical address", address);
			return false;
		}
	} else {
		if (!looks_like_kernel_address(address)) {
			error_usage(NULL, NULL, "address "KADDR_XFMT" does not look like a "
			            "kernel virtual address", address);
			return false;
		}
	}
	return true;
}

/*
 * kext_error
 *
 * Description:
 * 	Propagate a kext_result code into an error.
 */
static bool
kext_error(kext_result kr, const char *bundle_id, const char *symbol, kaddr_t address) {
	switch (kr) {
		case KEXT_SUCCESS:
			return false;
		case KEXT_ERROR:
			return true;
		case KEXT_NO_KEXT:
			if (bundle_id == NULL) {
				error_message("no kernel component contains address "KADDR_XFMT,
				              address);
			} else {
				error_kext_not_found(bundle_id);
			}
			return true;
		case KEXT_NO_SYMBOLS:
			error_kext_no_symbols(bundle_id);
			return true;
		case KEXT_NOT_FOUND:
			error_kext_symbol_not_found(bundle_id, symbol);
			return true;
	}
}

/*
 * find_vtable
 *
 * Description:
 * 	Find the vtable for a given class and bundle ID, reporting an error message as appropriate.
 */
static bool
find_vtable(const char *classname, const char *bundle_id, kaddr_t *address, size_t *size) {
	if (!initialize(KERNEL_SYMBOLS)) {
		return false;
	}
	kext_result kr = vtable_for_class(classname, bundle_id, address, size);
	if (kr == KEXT_NOT_FOUND) {
		if (bundle_id == NULL) {
			error_message("class %s not found", classname);
		} else if (strcmp(bundle_id, KERNEL_ID) == 0) {
			error_message("class %s not found in kernel", classname);
		} else {
			error_message("class %s not found in kernel extension %s", classname,
			              bundle_id);
		}
		return false;
	} else if (kext_error(kr, bundle_id, NULL, 0)) {
		return false;
	}
	return true;
}

bool
default_action(void) {
#if MEMCTL_REPL
	return run_repl();
#else
	return command_print_help(NULL);
#endif
}

bool
i_command() {
	char *cpu_type_name;
	char *cpu_subtype_name;
	slot_name(platform.cpu_type, platform.cpu_subtype, &cpu_type_name, &cpu_subtype_name);
	char memory_size_str[5];
	format_display_size(memory_size_str, platform.memory);
	char page_size_str[5];
	format_display_size(page_size_str, page_size);
	printf("release:            %d.%d.%d\n"
	       "version:            %s\n"
	       "machine:            %s\n"
	       "cpu type:           0x%x  (%s)\n"
	       "cpu subtype:        0x%x  (%s)\n"
	       "cpus:               %u cores / %u threads\n"
	       "memory:             0x%zx  (%s)\n"
	       "page size:          0x%lx  (%s)\n",
	       platform.release.major, platform.release.minor, platform.release.patch,
	       platform.version,
	       platform.machine,
	       platform.cpu_type, cpu_type_name,
	       platform.cpu_subtype, cpu_subtype_name,
	       platform.physical_cpu, platform.logical_cpu,
	       platform.memory, memory_size_str,
	       page_size, page_size_str);
	mach_port_t host = mach_host_self();
	if (host != MACH_PORT_NULL) {
		host_can_has_debugger_info_data_t debug_info;
		mach_msg_type_number_t count = HOST_CAN_HAS_DEBUGGER_COUNT;
		kern_return_t kr = host_info(host, HOST_CAN_HAS_DEBUGGER,
				(host_info_t) &debug_info, &count);
		if (kr == KERN_SUCCESS) {
			printf("can has debugger:   %s\n",
			       debug_info.can_has_debugger ? "yes" : "no");
		}
		mach_port_deallocate(mach_task_self(), host);
	}
	return true;
}

bool
r_command(kaddr_t address, size_t length, bool physical, size_t width, size_t access, bool dump) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	if (!check_address(address, length, physical)) {
		return false;
	}
	if (dump) {
		return memctl_dump(address, length, physical, width, access);
	} else {
		return memctl_read(address, length, physical, width, access);
	}
}

bool
rb_command(kaddr_t address, size_t length, bool physical, size_t access) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	if (!check_address(address, length, physical)) {
		return false;
	}
	return memctl_dump_binary(address, length, physical, access);
}

#if MEMCTL_DISASSEMBLY

bool
ri_command(kaddr_t address, size_t length, bool physical, size_t access) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	if (!check_address(address, length, physical)) {
		return false;
	}
	return memctl_disassemble(address, length, physical, access);
}

bool
rif_command(const char *function, const char *kext, size_t access) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	kaddr_t address;
	size_t size;
	kext_result kr = resolve_symbol(kext, function, &address, &size);
	if (kext_error(kr, kext, function, 0)) {
		return false;
	}
	return memctl_disassemble(address, size, false, access);
}

#endif // MEMCTL_DISASSEMBLY

bool
rs_command(kaddr_t address, size_t length, bool physical, size_t access) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	// If the user didn't specify a length, then length is -1, which will result in an overflow
	// error. Instead we check for one page of validity.
	if (!check_address(address, page_size, physical)) {
		return false;
	}
	return memctl_read_string(address, length, physical, access);
}

bool
w_command(kaddr_t address, kword_t value, bool physical, size_t width, size_t access) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	return wd_command(address, &value, width, physical, access);
}

bool
wd_command(kaddr_t address, const void *data, size_t length, bool physical, size_t access) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	if (!check_address(address, length, physical)) {
		return false;
	}
	return write_memory(address, &length, data, physical, access);
}

bool
ws_command(kaddr_t address, const char *string, bool physical, size_t access) {
	size_t length = strlen(string) + 1;
	return wd_command(address, string, length, physical, access);
}

bool
f_command(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical, bool heap,
		size_t access, size_t alignment) {
	if (!initialize(KERNEL_CALL | KERNEL_MEMORY)) {
		return false;
	}
	return memctl_find(start, end, value, width, physical, heap, access, alignment);
}

bool
fpr_command(pid_t pid) {
	if (!initialize(KERNEL_CALL)) {
		return false;
	}
	static kword_t _proc_find = 0;
	if (_proc_find == 0) {
		kext_result kr = kernel_symbol("_proc_find", &_proc_find, NULL);
		if (kext_error(kr, KERNEL_ID, "_proc_find", 0)) {
			return false;
		}
	}
	kword_t args[] = { pid };
	kaddr_t address;
	bool success = kernel_call(&address, sizeof(address), _proc_find, 1, args);
	if (!success) {
		error_message("proc_find(%d) failed", pid);
		return false;
	}
	printf(KADDR_XFMT"\n", address);
	return true;
}

bool
fi_command(kaddr_t start, kaddr_t end, const char *classname, const char *bundle_id,
		size_t access) {
	kaddr_t address;
	bool success = find_vtable(classname, bundle_id, &address, NULL);
	if (!success) {
		return false;
	}
	return f_command(start, end, address, sizeof(address), false, true, access,
			sizeof(address));
}

bool
kp_command(kaddr_t address) {
	if (!initialize(KERNEL_CALL)) {
		return false;
	}
	paddr_t paddr;
	bool success = kernel_virtual_to_physical(address, &paddr);
	if (!success) {
		return false;
	}
	printf(PADDR_XFMT"\n", paddr);
	return true;
}

bool
kpm_command(kaddr_t start, kaddr_t end) {
	if (!initialize(KERNEL_CALL)) {
		return false;
	}
	for (kaddr_t page = start & ~page_mask; page <= end; page += page_size) {
		paddr_t ppage;
		bool success = kernel_virtual_to_physical(page, &ppage);
		if (!success) {
			return false;
		}
		if (interrupted) {
			error_interrupt();
			return false;
		}
		if (ppage != 0) {
			printf(KADDR_FMT"  "PADDR_XFMT"\n", page, ppage);
		}
	}
	return true;
}

bool
vt_command(const char *classname, const char *bundle_id) {
	kaddr_t address;
	size_t size;
	bool success = find_vtable(classname, bundle_id, &address, &size);
	if (!success) {
		return false;
	}
	printf(KADDR_XFMT"  (%zu)\n", address, size);
	return true;
}

bool
vm_command(kaddr_t address, unsigned depth) {
	if (!initialize(KERNEL_TASK)) {
		return false;
	}
	return memctl_vmmap(address, address, depth);
}

bool
vmm_command(kaddr_t start, kaddr_t end, unsigned depth) {
	if (!initialize(KERNEL_TASK)) {
		return false;
	}
	return memctl_vmmap(start, end, depth);
}

bool
vmp_command(kaddr_t address, size_t length, int prot) {
	if (!initialize(KERNEL_TASK)) {
		return false;
	}
	if (!check_address(address, length, false)) {
		return false;
	}
	kern_return_t kr = mach_vm_protect(kernel_task, address, length, false, prot);
	if (kr != KERN_SUCCESS) {
		error_internal("mach_vm_protect failed: %s", mach_error_string(kr));
		return false;
	}
	return true;
}

bool
ks_command(kaddr_t address, bool unslide) {
	if (!initialize(KERNEL_SLIDE)) {
		return false;
	}
	printf(KADDR_XFMT"\n", address + (unslide ? -1 : 1) * kernel_slide);
	return true;
}

bool
a_command(const char *symbol, const char *kext) {
	if (!initialize(KERNEL_SYMBOLS)) {
		return false;
	}
	kaddr_t address;
	size_t size;
	kext_result kr = resolve_symbol(kext, symbol, &address, &size);
	if (kext_error(kr, kext, symbol, 0)) {
		return false;
	}
	printf(KADDR_XFMT"  (%zu)\n", address, size);
	return true;
}

bool
ap_command(kaddr_t address, bool unpermute) {
	if (!initialize(KERNEL_SYMBOLS | KERNEL_MEMORY_BASIC)) {
		return false;
	}
	static kword_t kernel_addrperm = 0;
	if (kernel_addrperm == 0) {
		kword_t _vm_kernel_addrperm;
		kext_result kr = kernel_symbol("_vm_kernel_addrperm", &_vm_kernel_addrperm, NULL);
		if (kext_error(kr, KERNEL_ID, "_vm_kernel_addrperm", 0)) {
			return false;
		}
		kernel_io_result kior = kernel_read_word(kernel_read_unsafe, _vm_kernel_addrperm,
				&kernel_addrperm, sizeof(kernel_addrperm), 0);
		if (kior != KERNEL_IO_SUCCESS) {
			error_internal("could not read %s", "_vm_kernel_addrperm");
			return false;
		}
	}
	printf(KADDR_XFMT"\n", address + (unpermute ? -1 : 1) * kernel_addrperm);
	return true;
}

static void
get_segment_offset(const struct macho *macho, kaddr_t address,
		const char **segname, size_t *offset) {
	const struct load_command *lc = macho_segment_containing_address(macho, address);
	if (lc == NULL) {
		return;
	}
	kaddr_t vmaddr;
	if (lc->cmd == LC_SEGMENT_64) {
		const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
		*segname = sc->segname;
		vmaddr   = sc->vmaddr;
	} else {
		const struct segment_command *sc = (const struct segment_command *)lc;
		*segname = sc->segname;
		vmaddr   = sc->vmaddr;
	}
	*offset = address - vmaddr;
}

bool
s_command(kaddr_t address) {
	if (!initialize(KERNEL_SYMBOLS)) {
		return false;
	}
	char *bundle_id = NULL;
	kext_result kr = kext_containing_address(address, &bundle_id);
	if (kext_error(kr, NULL, NULL, address)) {
		return false;
	}
	struct kext kext;
	kr = kext_init(&kext, bundle_id);
	bool is_error = kext_error(kr, bundle_id, NULL, 0);
	free(bundle_id);
	if (is_error) {
		return false;
	}
	const char *segname = NULL;
	size_t segoffset;
	get_segment_offset(&kext.macho, address - kernel_slide, &segname, &segoffset);
	const char *name = NULL;
	size_t size = 0;
	size_t offset = 0;
	kr = kext_resolve_address(&kext, address, &name, &size, &offset);
	if (kr == KEXT_SUCCESS && strlen(name) > 0 && offset < size) {
		assert(segname != NULL);
		if (offset == 0) {
			printf("%s.%s: %s  (%zu)\n", kext.bundle_id, segname, name, size);
		} else {
			printf("%s.%s: %s+%zu  (%zu)\n", kext.bundle_id, segname, name, offset, size);
		}
	} else if (segname == NULL) {
		offset = address - kext.base;
		if (offset == 0) {
			printf("%s\n", kext.bundle_id);
		} else {
			printf("%s+%zu\n", kext.bundle_id, offset);
		}
	} else if (kr == KEXT_SUCCESS || kr == KEXT_NOT_FOUND || kr == KEXT_NO_SYMBOLS) {
		if (offset == 0) {
			printf("%s.%s\n", kext.bundle_id, segname);
		} else {
			printf("%s.%s+%zu\n", kext.bundle_id, segname, segoffset);
		}
	} else {
		is_error = kext_error(kr, kext.bundle_id, NULL, 0);
	}
	kext_deinit(&kext);
	return !is_error;
}

bool
kcd_command(const char *kernelcache_path, const char *output_path) {
	bool success = false;
	int ofd;
	struct kernelcache *kc;
	struct kernelcache kc_local;
	if (output_path == NULL) {
		ofd = STDOUT_FILENO;
	} else {
		ofd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (ofd < 0) {
			error_internal("could not open output file '%s'", output_path);
			goto fail0;
		}
	}
	if (kernelcache_path == NULL) {
#if KERNELCACHE
		if (!initialize(KERNEL_IMAGE)) {
			goto fail1;
		}
		kc = &kernelcache;
#else
		assert(false);
#endif
	} else {
		kext_result kr = kernelcache_init_file(&kc_local, kernelcache_path);
		if (kr != KEXT_SUCCESS) {
			assert(kr == KEXT_ERROR);
			goto fail1;
		}
		kc = &kc_local;
	}
	uint8_t *p = kc->kernel.mh;
	size_t left = kc->kernel.size;
	for (;;) {
		if (left == 0) {
			success = true;
			break;
		}
		ssize_t written = write(ofd, p, left);
		if (interrupted) {
			error_interrupt();
			break;
		}
		if (written <= 0) {
			error_internal("could not write to output file");
			break;
		}
		left -= written;
		p += written;
	}
	if (kernelcache_path != NULL) {
		kernelcache_deinit(&kc_local);
	}
fail1:
	if (output_path != NULL) {
		close(ofd);
	}
fail0:
	return success;
}

int
main(int argc, const char *argv[]) {
	bool success = default_initialize();
	if (success) {
		success = command_run_argv(argc - 1, argv + 1);
	}
	deinitialize(false);
	print_errors();
	return (success ? 0 : 1);
}
