#include "cli.h"
#include "disassemble.h"
#include "error.h"
#include "find.h"
#include "format.h"
#include "initialize.h"
#include "memory.h"
#include "read.h"
#include "vmmap.h"

#include "memctl/kernel.h"
#include "memctl/kernel_slide.h"
#include "memctl/kernelcache.h"
#include "memctl/memctl_signal.h"
#include "memctl/platform.h"
#include "memctl/privilege_escalation.h"
#include "memctl/process.h"
#include "memctl/vtable.h"

#include <stdio.h>

#if MEMCTL_REPL
#include <histedit.h>
#endif

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
	// HACK: Print out the prompt even when there's no TTY.
	//
	// libedit doesn't like to show a prompt when there's no TTY. However, I very much like my
	// prompt; I put a lot of effort into that prompt. Thus, pretend we're running inside an
	// emacs shell if there's no TTY to get the prompt back. This will cause libedit to disable
	// editing, which will in turn prevent libedit from permanently disabling prompt printing
	// during initialization.
	//
	// Unfortunately, this seems to be an implementation detail, and the behavior can't be
	// achieved by setting the terminal type after creation.
	if (!isatty(fileno(stdout))) {
		setenv("TERM", "emacs", 1);
	}
	// Initialize libedit.
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
	// Run the REPL.
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
	success = true;
	free(prompt_string);
	in_repl = false;
	// Clean up.
	history_end(hist);
fail2:
	tok_end(tok);
fail1:
	el_end(el);
fail0:
	return success;
}

bool
quit_command() {
	if (!in_repl) {
		error_usage("quit", NULL, "not currently in a REPL");
		return false;
	}
	in_repl = false;
	return true;
}

#endif // MEMCTL_REPL

/*
 * make_memflags
 *
 * Description:
 * 	Create flags for read_kernel/write_kernel based on command-line parameters.
 */
static memflags
make_memflags(bool force, bool physical) {
	return (force ? MEM_FORCE : 0) | (physical ? MEM_PHYS : 0);
}

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

/*
 * lookup_vtable
 *
 * Print the class name and possibly offset if allow_internal is true for the given vtable address.
 */
static bool
lookup_vtable(kaddr_t address, bool allow_internal) {
	if (!initialize(KERNEL_SYMBOLS)) {
		return false;
	}
	char *class_name;
	size_t offset;
	kext_result kr = vtable_lookup(address, &class_name, &offset);
	if (kr == KEXT_NO_KEXT) {
		error_message("address "KADDR_XFMT" is not a vtable", address);
		return false;
	} else if (kr == KEXT_NOT_FOUND) {
		error_message("cannot find class for vtable "KADDR_XFMT, address);
		return false;
	} else if (kext_error(kr, NULL, NULL, address)) {
		return false;
	}
	bool success = false;
	if (!allow_internal && offset != VTABLE_OFFSET_SIZE) {
		const char *where = (offset < VTABLE_OFFSET_SIZE ? "before" : "inside");
		error_message("address "KADDR_XFMT" is %s the vtable for class %s",
		              address, where, class_name);
		goto end;
	}
	success = true;
	ssize_t soffset = offset - VTABLE_OFFSET_SIZE;
	if (soffset == 0) {
		printf("%s\n", class_name);
	} else {
		printf("%s  (%+zd)\n", class_name, soffset);
	}
end:
	free(class_name);
	return success;
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
r_command(kaddr_t address, size_t length, bool force, bool physical, size_t width, size_t access,
		bool dump) {
	if (!force && !check_address(address, length, physical)) {
		return false;
	}
	memflags flags = make_memflags(force, physical);
	if (dump) {
		return memctl_dump(address, length, flags, width, access);
	} else {
		return memctl_read(address, length, flags, width, access);
	}
}

bool
rb_command(kaddr_t address, size_t length, bool force, bool physical, size_t access) {
	if (!force && !check_address(address, length, physical)) {
		return false;
	}
	memflags flags = make_memflags(force, physical);
	return memctl_dump_binary(address, length, flags, access);
}

#if MEMCTL_DISASSEMBLY

bool
ri_command(kaddr_t address, size_t length, bool force, bool physical, size_t access) {
	if (!force && !check_address(address, length, physical)) {
		return false;
	}
	memflags flags = make_memflags(force, physical);
	return memctl_disassemble(address, length, flags, access);
}

bool
rif_command(const char *function, const char *kext, bool force, size_t access) {
	if (!initialize(KERNEL_SYMBOLS)) {
		return false;
	}
	kaddr_t address;
	size_t size;
	kext_result kr = resolve_symbol(kext, function, &address, &size);
	if (kext_error(kr, kext, function, 0)) {
		return false;
	}
	memflags flags = make_memflags(force, false);
	return memctl_disassemble(address, size, flags, access);
}

#endif // MEMCTL_DISASSEMBLY

bool
rs_command(kaddr_t address, size_t length, bool force, bool physical, size_t access) {
	// If the user didn't specify a length, then length is -1, which will result in an overflow
	// error. Instead we check for one page of validity.
	if (!force && !check_address(address, page_size, physical)) {
		return false;
	}
	memflags flags = make_memflags(force, physical);
	return memctl_read_string(address, length, flags, access);
}

bool
w_command(kaddr_t address, kword_t value, bool force, bool physical, size_t width, size_t access) {
	return wd_command(address, &value, width, force, physical, access);
}

bool
wd_command(kaddr_t address, const void *data, size_t length, bool force, bool physical,
		size_t access) {
	if (!force && !check_address(address, length, physical)) {
		return false;
	}
	memflags flags = make_memflags(force, physical);
	return write_kernel(address, &length, data, flags, access);
}

bool
ws_command(kaddr_t address, const char *string, bool force, bool physical, size_t access) {
	size_t length = strlen(string) + 1;
	return wd_command(address, string, length, force, physical, access);
}

bool
f_command(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical, bool heap,
		size_t access, size_t alignment) {
	return memctl_find(start, end, value, width, physical, heap, access, alignment);
}

bool
fpr_command(pid_t pid) {
	if (!initialize(PROCESS)) {
		return false;
	}
	if (proc_find == NULL) {
		error_api_unavailable("proc_find");
		return false;
	}
	kaddr_t proc;
	bool success = proc_find(&proc, pid, true);
	if (!success) {
		return false;
	}
	printf(KADDR_XFMT"\n", proc);
	return true;
}

bool
fc_command(kaddr_t start, kaddr_t end, const char *classname, const char *bundle_id,
		size_t access) {
	kaddr_t address;
	bool success = find_vtable(classname, bundle_id, &address, NULL);
	if (!success) {
		return false;
	}
	return memctl_find(start, end, address, sizeof(address), false, true, access,
			sizeof(address));
}

bool
lc_command(kaddr_t address) {
	kaddr_t vtable;
	size_t size = sizeof(vtable);
	return read_kernel(address, &size, &vtable, 0, 0)
		&& lookup_vtable(vtable, false);
}

bool
kp_command(kaddr_t address) {
	if (!initialize(KERNEL_MEMORY)) {
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
	if (!initialize(KERNEL_MEMORY)) {
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
zs_command(kaddr_t address) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	if (zone_element_size == NULL) {
		error_api_unavailable("zone_element_size");
		return false;
	}
	size_t size;
	bool success = zone_element_size(address, &size);
	if (!success) {
		return false;
	}
	printf("%zu\n", size);
	return true;
}

bool
pca_command(kaddr_t address, bool is_virtual) {
	if (!initialize(KERNEL_MEMORY)) {
		return false;
	}
	if (pmap_cache_attributes == NULL) {
		error_api_unavailable("pmap_cache_attributes");
		return false;
	}
	if (is_virtual) {
		kaddr_t physaddr;
		bool success = kernel_virtual_to_physical(address, &physaddr);
		if (!success) {
			return false;
		}
		if (physaddr == 0) {
			error_address_unmapped(address);
			return false;
		}
		address = physaddr;
	}
	unsigned cacheattr;
	bool success = pmap_cache_attributes(&cacheattr, address >> page_shift);
	if (!success) {
		return false;
	}
	printf("%x\n", cacheattr);
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
vtl_command(kaddr_t address) {
	return lookup_vtable(address, true);
}

bool
vm_command(kaddr_t address, unsigned depth) {
	return memctl_vmmap(address, address, depth);
}

bool
vmm_command(kaddr_t start, kaddr_t end, unsigned depth) {
	return memctl_vmmap(start, end, depth);
}

bool
vma_command(size_t size) {
	if (!initialize(KERNEL_TASK)) {
		return false;
	}
	kaddr_t address;
	bool success = kernel_allocate(&address, size);
	if (!success) {
		error_message("could not allocate region of size %zu", size);
		return false;
	}
	printf(KADDR_XFMT"\n", address);
	return true;
}

bool
vmd_command(kaddr_t address, size_t size) {
	if (!initialize(KERNEL_TASK)) {
		return false;
	}
	bool success = kernel_deallocate(address, size, true);
	if (!success) {
		error_message("could not deallocate %zu bytes at address "KADDR_XFMT,
				size, address);
	}
	return success;
}

bool
vmp_command(kaddr_t address, size_t length, int prot) {
	if (!check_address(address, length, false)) {
		return false;
	}
	return memctl_vmprotect(address, length, prot);
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
	const struct kext *kext;
	kext_result kr = kernel_kext_containing_address(&kext, address);
	if (kext_error(kr, NULL, NULL, address)) {
		return false;
	}
	const char *segname = NULL;
	size_t segoffset;
	get_segment_offset(&kext->macho, address - kext->slide, &segname, &segoffset);
	const char *name = NULL;
	size_t size = 0;
	size_t offset = 0;
	kr = kext_resolve_address(kext, address, &name, &size, &offset);
	bool is_error = false;
	if (kr == KEXT_SUCCESS && strlen(name) > 0 && offset < size) {
		assert(segname != NULL);
		if (offset == 0) {
			printf("%s %s: %s  (%zu)\n", kext->bundle_id, segname, name, size);
		} else {
			printf("%s %s: %s+%zu  (%zu)\n", kext->bundle_id, segname, name, offset, size);
		}
	} else if (segname == NULL) {
		offset = address - kext->base;
		if (offset == 0) {
			printf("%s\n", kext->bundle_id);
		} else {
			printf("%s+%zu\n", kext->bundle_id, offset);
		}
	} else if (kr == KEXT_SUCCESS || kr == KEXT_NOT_FOUND) {
		if (segoffset == 0) {
			printf("%s %s\n", kext->bundle_id, segname);
		} else {
			printf("%s %s+%zu\n", kext->bundle_id, segname, segoffset);
		}
	} else {
		is_error = kext_error(kr, kext->bundle_id, NULL, 0);
	}
	kext_release(kext);
	return !is_error;
}

bool
kcd_command(const char *kernelcache_path, const char *output_path) {
	bool success = false;
	int ofd;
	struct kernelcache *kc;
	struct kernelcache kc_local;
	if (output_path == NULL) {
		ofd = fileno(stdout);
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

bool
root_command() {
	if (!initialize(PRIVESC)) {
		return false;
	}
	if (!setuid_root()) {
		error_message("could not elevate privileges");
		return false;
	}
	deinitialize(false);
	char *argv[] = { "/bin/sh", NULL };
	execve(argv[0], argv, NULL);
	fprintf(stderr, "error: could not exec %s\n", argv[0]);
	exit(1);
}

int
main(int argc, const char *argv[]) {
	default_initialize();
	bool success = command_run_argv(argc - 1, argv + 1);
	print_errors();
	deinitialize(false);
	return (success ? 0 : 1);
}
