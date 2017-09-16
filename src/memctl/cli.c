#include "cli.h"

#include "error.h"
#include "format.h"
#include "initialize.h"
#include "vmmap.h"

#include "memctl/core.h"
#include "memctl/kernel.h"

#include <stdio.h>
#include <string.h>

#define HANDLER(name)							\
	static bool name(const struct argument *arguments)

#define OPT_GET_OR_(n_, opt_, arg_, val_, type_, field_)		\
	({ const struct argument *a = &arguments[n_];			\
	   assert(strcmp(a->option, opt_) == 0);			\
	   assert(strcmp(a->argument, arg_) == 0);			\
	   assert(a->type == type_ || a->type == ARG_NONE);		\
	   (a->present ? a->field_ : val_); })

#define ARG_GET_(n_, arg_, type_, field_)				\
	({ const struct argument *a = &arguments[n_];			\
	   assert(a->present);						\
	   assert(a->option == ARGUMENT || a->option == OPTIONAL);	\
	   assert(strcmp(a->argument, arg_) == 0);			\
	   assert(a->type == type_);					\
	   a->field_; })

#define ARG_GET_OR_(n_, arg_, val_, type_, field_)			\
	({ const struct argument *a = &arguments[n_];			\
	   assert(a->option == ARGUMENT || a->option == OPTIONAL);	\
	   assert(strcmp(a->argument, arg_) == 0);			\
	   assert(a->type == type_ || a->type == ARG_NONE);		\
	   (a->present ? a->field_ : val_); })

#define OPT_PRESENT(n_, opt_)						\
	({ const struct argument *a = &arguments[n_];			\
	   assert(strcmp(a->option, opt_) == 0);			\
	   a->present; })

#define ARG_PRESENT(n_, arg_)						\
	({ const struct argument *a = &arguments[n_];			\
	   assert(a->option == OPTIONAL);				\
	   assert(strcmp(a->argument, arg_) == 0);			\
	   a->present; })

#define OPT_GET_INT_OR(n_, opt_, arg_, val_)		OPT_GET_OR_(n_, opt_, arg_, val_, ARG_INT, sint)
#define OPT_GET_UINT_OR(n_, opt_, arg_, val_)		OPT_GET_OR_(n_, opt_, arg_, val_, ARG_UINT, uint)
#define OPT_GET_WIDTH_OR(n_, opt_, arg_, val_)		OPT_GET_OR_(n_, opt_, arg_, val_, ARG_WIDTH, width)
#define OPT_GET_DATA_OR(n_, opt_, arg_, val_)		OPT_GET_OR_(n_, opt_, arg_, val_, ARG_DATA, data)
#define OPT_GET_STRING_OR(n_, opt_, arg_, val_)		OPT_GET_OR_(n_, opt_, arg_, val_, ARG_STRING, string)
#define OPT_GET_ARGV_OR(n_, opt_, arg_, val_)		OPT_GET_OR_(n_, opt_, arg_, val_, ARG_ARGV, argv)
#define OPT_GET_SYMBOL_OR(n_, opt_, arg_, val_)		OPT_GET_OR_(n_, opt_, arg_, val_, ARG_SYMBOL, symbol)
#define OPT_GET_ADDRESS_OR(n_, opt_, arg_, val_)	OPT_GET_OR_(n_, opt_, arg_, val_, ARG_ADDRESS, address)
#define OPT_GET_RANGE_OR(n_, opt_, arg_, start_, end_)	\
	OPT_GET_OR_(n_, opt_, arg_, ((struct argrange) { start_, end_, true, true }), ARG_RANGE, range)

#define ARG_GET_INT(n_, arg_)				ARG_GET_(n_, arg_, ARG_INT, sint)
#define ARG_GET_UINT(n_, arg_)				ARG_GET_(n_, arg_, ARG_UINT, uint)
#define ARG_GET_WIDTH(n_, arg_)				ARG_GET_(n_, arg_, ARG_WIDTH, width)
#define ARG_GET_DATA(n_, arg_)				ARG_GET_(n_, arg_, ARG_DATA, data)
#define ARG_GET_STRING(n_, arg_)			ARG_GET_(n_, arg_, ARG_STRING, string)
#define ARG_GET_ARGV(n_, arg_)				ARG_GET_(n_, arg_, ARG_ARGV, argv)
#define ARG_GET_SYMBOL(n_, arg_)			ARG_GET_(n_, arg_, ARG_SYMBOL, symbol)
#define ARG_GET_ADDRESS(n_, arg_)			ARG_GET_(n_, arg_, ARG_ADDRESS, address)
#define ARG_GET_RANGE(n_, arg_)				ARG_GET_(n_, arg_, ARG_RANGE, range)

#define ARG_GET_INT_OR(n_, arg_, val_)			ARG_GET_OR_(n_, arg_, val_, ARG_INT, sint)
#define ARG_GET_UINT_OR(n_, arg_, val_)			ARG_GET_OR_(n_, arg_, val_, ARG_UINT, uint)
#define ARG_GET_WIDTH_OR(n_, arg_, val_)		ARG_GET_OR_(n_, arg_, val_, ARG_WIDTH, width)
#define ARG_GET_DATA_OR(n_, arg_, val_)			ARG_GET_OR_(n_, arg_, val_, ARG_DATA, data)
#define ARG_GET_STRING_OR(n_, arg_, val_)		ARG_GET_OR_(n_, arg_, val_, ARG_STRING, string)
#define ARG_GET_ARGV_OR(n_, arg_, val_)			ARG_GET_OR_(n_, arg_, val_, ARG_ARGV, argv)
#define ARG_GET_SYMBOL_OR(n_, arg_, val_)		ARG_GET_OR_(n_, arg_, val_, ARG_SYMBOL, symbol)
#define ARG_GET_ADDRESS_OR(n_, arg_, val_)		ARG_GET_OR_(n_, arg_, val_, ARG_ADDRESS, address)
#define ARG_GET_RANGE_OR(n_, arg_, start_, end_)	\
	ARG_GET_OR_(n_, arg_, ((struct argrange) { start_, end_, true, true }), ARG_RANGE, range)

// Default values for argrange start and end values.
kaddr_t range_default_virtual_start;
kaddr_t range_default_virtual_end;

/*
 * range_default_init
 *
 * Description:
 * 	Initialize default values for memory ranges.
 */
static bool
range_default_init() {
	if (range_default_virtual_start == 0) {
		kaddr_t start = 0;
		size_t size = 0;
		if (!memctl_vmregion(&start, &size, start) || size == 0) {
			goto fail;
		}
		kaddr_t end = start;
		for (;;) {
			end += size;
			size = 0;
			if (!memctl_vmregion(&end, &size, end)) {
				goto fail;
			}
			if (size == 0) {
				range_default_virtual_start = start;
				range_default_virtual_end   = end;
				break;
			}
		}
	}
	return true;
fail:
	error_internal("could not initialize default address range");
	return false;
}

/*
 * validate_options_b_k
 *
 * Description:
 * 	Handle the (mutually exclusive) [-b kext] and [-k] options.
 */
static bool
validate_options_b_k(const char *command, const char **bundle_id, bool in_kernel) {
	if (in_kernel) {
		if (*bundle_id != NULL) {
			error_usage(command, NULL, "b and k options are mutually exclusive");
			return false;
		}
		*bundle_id = KERNEL_ID;
	}
	return true;
}

/*
 * default_virtual_range
 *
 * Description:
 * 	If either endpoint of the range was a default, then specify a realistic virtual address
 * 	instead.
 */
static bool
default_virtual_range(struct argrange *range) {
	if (!range_default_init()) {
		return false;
	}
	if (range->default_start) {
		range->start = range_default_virtual_start;
	}
	if (range->default_end) {
		range->end = range_default_virtual_end;
	}
	return true;
}

/*
 * vm_region_size
 *
 * Description:
 * 	Try to get the size for a virtual memory region.
 */
static bool
vm_region_size(size_t *size, kaddr_t address) {
	kaddr_t vmaddress;
	size_t vmsize;
	if (!memctl_vmregion(&vmaddress, &vmsize, address)) {
		error_internal("could not find region size for address "KADDR_XFMT, address);
		return false;
	}
	*size = vmsize - (address - vmaddress);
	return true;
}

/*
 * parse_protection
 *
 * Description:
 * 	Parse the given protection string (e.g. 'r-x').
 */
static bool
parse_protection(const char *command, const char *protection, int *prot) {
	int prot_ = 0;
	const char *template = "rwx";
	const int flag[] = { VM_PROT_READ, VM_PROT_WRITE, VM_PROT_EXECUTE };
	const char *p = protection;
	for (size_t i = 0; ; i++) {
		// Handle end-of-argument.
		char pc = *p;
		if (pc == 0) {
			if (p == protection) {
				break;
			}
			*prot = prot_;
			return true;
		}
		// Make sure protection isn't too long.
		char tc = template[i];
		if (tc == 0) {
			break;
		}
		// Advance to the next character if we have a match or skip.
		if (pc == '-' || pc == tc) {
			p++;
		}
		// If this isn't a match, try the next character.
		if (tc != pc) {
			continue;
		}
		// Set the flag.
		assert((prot_ & flag[i]) == 0);
		prot_ |= flag[i];
	}
	error_usage(command, NULL, "invalid protection '%s'", protection);
	return false;
}

HANDLER(i_handler) {
	return i_command();
}

HANDLER(r_handler) {
	size_t width    = OPT_GET_WIDTH_OR(0, "", "width", sizeof(kword_t));
	bool dump       = OPT_PRESENT(1, "d");
	bool force      = OPT_PRESENT(2, "f");
	bool physical   = OPT_PRESENT(3, "p");
	size_t access   = OPT_GET_WIDTH_OR(4, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(5, "address");
	size_t length;
	if (ARG_PRESENT(6, "length")) {
		length = ARG_GET_UINT(6, "length");
	} else if (dump) {
		length = 256;
	} else {
		length = width;
	}
	return r_command(address, length, force, physical, width, access, dump);
}

HANDLER(rb_handler) {
	bool force      = OPT_PRESENT(0, "f");
	bool physical   = OPT_PRESENT(1, "p");
	size_t access   = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(3, "address");
	size_t length   = ARG_GET_UINT(4, "length");
	return rb_command(address, length, force, physical, access);
}

#if MEMCTL_DISASSEMBLY

HANDLER(ri_handler) {
	bool force      = OPT_PRESENT(0, "f");
	bool physical   = OPT_PRESENT(1, "p");
	size_t access   = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(3, "address");
	size_t length   = ARG_GET_UINT(4, "length");
#if __arm__ || __arm64__
	if (address & 3) {
		error_usage("ri", NULL, "address "KADDR_XFMT" is unaligned", address);
		return false;
	}
#endif
	return ri_command(address, length, force, physical, access);
}

HANDLER(rif_handler) {
	bool   force              = OPT_PRESENT(0, "f");
	size_t access             = OPT_GET_WIDTH_OR(1, "x", "access", 0);
	struct argsymbol function = ARG_GET_SYMBOL(2, "function");
	return rif_command(function.symbol, function.kext, force, access);
}

#endif // MEMCTL_DISASSEMBLY

HANDLER(rs_handler) {
	bool force      = OPT_PRESENT(0, "f");
	bool physical   = OPT_PRESENT(1, "p");
	size_t access   = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(3, "address");
	size_t length   = ARG_GET_UINT_OR(4, "length", -1);
	return rs_command(address, length, force, physical, access);
}

HANDLER(w_handler) {
	size_t width    = OPT_GET_WIDTH_OR(0, "", "width", sizeof(kword_t));
	bool force      = OPT_PRESENT(1, "f");
	bool physical   = OPT_PRESENT(2, "p");
	size_t access   = OPT_GET_WIDTH_OR(3, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(4, "address");
	kword_t value   = ARG_GET_UINT(5, "value");
	return w_command(address, value, force, physical, width, access);
}

HANDLER(wd_handler) {
	bool force          = OPT_PRESENT(0, "f");
	bool physical       = OPT_PRESENT(1, "p");
	size_t access       = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	kaddr_t address     = ARG_GET_ADDRESS(3, "address");
	struct argdata data = ARG_GET_DATA(4, "data");
	return wd_command(address, data.data, data.length, force, physical, access);
}

HANDLER(ws_handler) {
	bool force         = OPT_PRESENT(0, "f");
	bool physical      = OPT_PRESENT(1, "p");
	size_t access      = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	kaddr_t address    = ARG_GET_ADDRESS(3, "address");
	const char *string = ARG_GET_STRING(4, "string");
	return ws_command(address, string, force, physical, access);
}

HANDLER(f_handler) {
	size_t width          = OPT_GET_WIDTH_OR(0, "", "width", sizeof(kword_t));
	bool physical         = OPT_PRESENT(1, "p");
	bool heap             = OPT_PRESENT(2, "h");
	size_t alignment      = OPT_GET_WIDTH_OR(3, "a", "align", width);
	size_t access         = OPT_GET_WIDTH_OR(4, "x", "access", 0);
	kword_t value         = ARG_GET_UINT(5, "value");
	struct argrange range = ARG_GET_RANGE_OR(6, "range", 0, -1);
	if (physical && heap) {
		error_usage("f", NULL, "p and h options are mutually exclusive");
		return false;
	}
	if (!heap && !physical) {
		// memctl_find internally uses kernel_read_all, which does not skip regions just
		// because they are not listed in the virtual memory map. If we don't truncate the
		// range appropriately, the find will take a long time.
		if (!default_virtual_range(&range)) {
			return false;
		}
	}
	return f_command(range.start, range.end, value, width, physical, heap, access, alignment);
}

HANDLER(fpr_handler) {
	intmax_t ipid = ARG_GET_INT(0, "pid");
	pid_t pid = ipid;
	if ((intmax_t)pid != ipid) {
		error_usage("fpr", NULL, "invalid PID %lld", ipid);
		return false;
	}
	return fpr_command(pid);
}

HANDLER(fc_handler) {
	const char *bundle_id = OPT_GET_STRING_OR(0, "b", "kext", NULL);
	bool in_kernel        = OPT_PRESENT(1, "k");
	size_t access         = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	const char *classname = ARG_GET_STRING(3, "class");
	struct argrange range = ARG_GET_RANGE_OR(4, "range", 0, -1);
	if (!validate_options_b_k("fc", &bundle_id, in_kernel)) {
		return false;
	}
	return fc_command(range.start, range.end, classname, bundle_id, access);
}

HANDLER(lc_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return lc_command(address);
}

HANDLER(cm_handler) {
	const char *bundle_id = OPT_GET_STRING_OR(0, "b", "kext", NULL);
	bool in_kernel        = OPT_PRESENT(1, "k");
	const char *classname = ARG_GET_STRING(2, "class");
	if (!validate_options_b_k("cm", &bundle_id, in_kernel)) {
		return false;
	}
	return cm_command(classname, NULL);
}

HANDLER(cz_handler) {
	const char *bundle_id = OPT_GET_STRING_OR(0, "b", "kext", NULL);
	bool in_kernel        = OPT_PRESENT(1, "k");
	const char *classname = ARG_GET_STRING(2, "class");
	if (!validate_options_b_k("cz", &bundle_id, in_kernel)) {
		return false;
	}
	return cz_command(classname, bundle_id);
}

HANDLER(kp_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return kp_command(address);
}

HANDLER(kpm_handler) {
	struct argrange range = ARG_GET_RANGE(0, "range");
	if (!default_virtual_range(&range)) {
		return false;
	}
	return kpm_command(range.start, range.end);
}

HANDLER(zs_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return zs_command(address);
}

HANDLER(pca_handler) {
	bool is_virtual = OPT_PRESENT(0, "v");
	kaddr_t address = ARG_GET_ADDRESS(1, "address");
	return pca_command(address, is_virtual);
}

HANDLER(vt_handler) {
	const char *bundle_id = OPT_GET_STRING_OR(0, "b", "kext", NULL);
	bool in_kernel        = OPT_PRESENT(1, "k");
	const char *classname = ARG_GET_STRING(2, "class");
	if (!validate_options_b_k("vt", &bundle_id, in_kernel)) {
		return false;
	}
	return vt_command(classname, bundle_id);
}

HANDLER(vtl_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return vtl_command(address);
}

HANDLER(vm_handler) {
	unsigned depth  = OPT_GET_UINT_OR(0, "d", "depth", 2048);
	kaddr_t address = ARG_GET_ADDRESS(1, "address");
	return vm_command(address, depth);
}

HANDLER(vmm_handler) {
	unsigned depth        = OPT_GET_UINT_OR(0, "d", "depth", 2048);
	struct argrange range = ARG_GET_RANGE_OR(1, "range", 0, -1);
	return vmm_command(range.start, range.end, depth);
}

HANDLER(vma_handler) {
	size_t size = ARG_GET_UINT(0, "size");
	return vma_command(size);
}

HANDLER(vmd_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	size_t size;
	if (ARG_PRESENT(1, "size")) {
		size = ARG_GET_UINT(1, "size");
	} else {
		if (!vm_region_size(&size, address)) {
			return false;
		}
	}
	return vmd_command(address, size);
}

HANDLER(vmp_handler) {
	const char *protection = ARG_GET_STRING(0, "protection");
	kaddr_t address        = ARG_GET_ADDRESS(1, "address");
	size_t length          = ARG_GET_UINT_OR(2, "length", 1);
	int prot;
	if (!parse_protection("vmp", protection, &prot)) {
		return false;
	}
	return vmp_command(address, length, prot);
}

HANDLER(ks_handler) {
	bool unslide    = OPT_PRESENT(0, "u");
	kaddr_t address = ARG_GET_ADDRESS_OR(1, "address", 0);
	return ks_command(address, unslide);
}

HANDLER(a_handler) {
	struct argsymbol symbol = ARG_GET_SYMBOL(0, "symbol");
	return a_command(symbol.symbol, symbol.kext);
}

HANDLER(ap_handler) {
	bool unpermute  = OPT_PRESENT(0, "u");
	kaddr_t address = ARG_GET_ADDRESS_OR(1, "address", 0);
	return ap_command(address, unpermute);
}

HANDLER(s_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return s_command(address);
}

HANDLER(kcd_handler) {
	const char *output = OPT_GET_STRING_OR(0, "o", "output", NULL);
#if KERNELCACHE
	const char *kernelcache = ARG_GET_STRING_OR(1, "file", NULL);
#else
	const char *kernelcache = ARG_GET_STRING(1, "file");
#endif
	return kcd_command(kernelcache, output);
}

HANDLER(root_handler) {
	return root_command();
}

#if MEMCTL_REPL

HANDLER(quit_handler) {
	return quit_command();
}

#endif

#define ARGSPEC(n)	n, (struct argspec *) &(struct argspec[n])

static struct command commands[] = {
	{
		"i", NULL, i_handler,
		"Print system information",
		"Print general information about the system.",
		0, NULL,
	}, {
		"r", NULL, r_handler,
		"Read and print formatted memory",
		"Read data from kernel virtual or physical memory and print it with the specified "
		"formatting.",
		ARGSPEC(7) {
			{ "",       "width",   ARG_WIDTH,   "The width to display each value" },
			{ "d",      NULL,      ARG_NONE,    "Use dump format with ASCII"      },
			{ "f",      NULL,      ARG_NONE,    "Force read (unsafe)"             },
			{ "p",      NULL,      ARG_NONE,    "Read physical memory"            },
			{ "x",      "access",  ARG_WIDTH,   "The memory access width"         },
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to read"             },
			{ OPTIONAL, "length",  ARG_UINT,    "The number of bytes to read"     },
		},
	}, {
		"rb", "r", rb_handler,
		"Print raw binary data from memory",
		"Read data from kernel virtual or physical memory and write the binary data "
		"directly to stdout.",
		ARGSPEC(5) {
			{ "f",      NULL,      ARG_NONE,    "Force read (unsafe)"         },
			{ "p",      NULL,      ARG_NONE,    "Read physical memory"        },
			{ "x",      "access",  ARG_WIDTH,   "The memory access width"     },
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to read"         },
			{ ARGUMENT, "length",  ARG_UINT,    "The number of bytes to read" },
		},
#if MEMCTL_DISASSEMBLY
	}, {
		"ri", "r", ri_handler,
		"Disassemble kernel memory",
		"Print the disassembly of kernel memory.",
		ARGSPEC(5) {
			{ "f",      NULL,      ARG_NONE,    "Force read (unsafe)"                },
			{ "p",      NULL,      ARG_NONE,    "Read physical memory"               },
			{ "x",      "access",  ARG_WIDTH,   "The memory access width"            },
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to read"                },
			{ ARGUMENT, "length",  ARG_UINT,    "The number of bytes to disassemble" },
		},
	}, {
		"rif", "ri", rif_handler,
		"Disassemble a function",
		"Disassemble the named kernel function.",
		ARGSPEC(3) {
			{ "f",      NULL,       ARG_NONE,   "Force read (unsafe)"         },
			{ "x",      "access",   ARG_WIDTH,  "The memory access width"     },
			{ ARGUMENT, "function", ARG_SYMBOL, "The function to disassemble" },
		},
#endif // MEMCTL_DISASSEMBLY
	}, {
		"rs", "r", rs_handler,
		"Read a string from memory",
		"Read and print an ASCII string from kernel memory.",
		ARGSPEC(5) {
			{ "f",      NULL,      ARG_NONE,    "Force read (unsafe)"       },
			{ "p",      NULL,      ARG_NONE,    "Read physical memory"      },
			{ "x",      "access",  ARG_WIDTH,   "The memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to read"       },
			{ OPTIONAL, "length",  ARG_UINT,    "The maximum string length" },
		},
	}, {
		"w", NULL, w_handler,
		"Write an integer to memory",
		"Write an integer to kernel virtual or physical memory.",
		ARGSPEC(6) {
			{ "",       "width",   ARG_WIDTH,   "The width of the value"  },
			{ "f",      NULL,      ARG_NONE,    "Force write (unsafe)"    },
			{ "p",      NULL,      ARG_NONE,    "Write physical memory"   },
			{ "x",      "access",  ARG_WIDTH,   "The memory access width" },
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to write"    },
			{ ARGUMENT, "value",   ARG_UINT,    "The value to write"      },
		},
	}, {
		"wd", "w", wd_handler,
		"Write arbitrary data to memory",
		"Write data (specified as a hexadecimal string) to kernel memory.",
		ARGSPEC(5) {
			{ "f",      NULL,      ARG_NONE,    "Force write (unsafe)"    },
			{ "p",      NULL,      ARG_NONE,    "Write physical memory"   },
			{ "x",      "access",  ARG_WIDTH,   "The memory access width" },
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to write"    },
			{ ARGUMENT, "data",    ARG_DATA,    "The data to write"       },
		},
	}, {
		"ws", "w", ws_handler,
		"Write a string to memory",
		"Write a NULL-terminated ASCII string to kernel memory.",
		ARGSPEC(5) {
			{ "f",      NULL,      ARG_NONE,    "Force write (unsafe)"    },
			{ "p",      NULL,      ARG_NONE,    "Write physical memory"   },
			{ "x",      "access",  ARG_WIDTH,   "The memory access width" },
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to write"    },
			{ ARGUMENT, "string",  ARG_STRING,  "The string to write"     },
		},
	}, {
		"f", NULL, f_handler,
		"Find an integer in memory",
		"Scan through kernel memory for an integer and print all addresses at which it "
		"is found.",
		ARGSPEC(7) {
			{ "",       "width",  ARG_WIDTH, "The width of the value"      },
			{ "p",      NULL,     ARG_NONE,  "Search physical memory"      },
			{ "h",      NULL,     ARG_NONE,  "Search heap memory"          },
			{ "a",      "align",  ARG_WIDTH, "The alignment of the value"  },
			{ "x",      "access", ARG_WIDTH, "The memory access width"     },
			{ ARGUMENT, "value",  ARG_UINT,  "The value to find"           },
			{ OPTIONAL, "range",  ARG_RANGE, "The address range to search" },
		},
	}, {
		"fpr", "f", fpr_handler,
		"Find the proc struct for a process",
		"Find the in-kernel process structure (struct proc) for a process by its PID.",
		ARGSPEC(1) {
			{ ARGUMENT, "pid", ARG_INT, "The pid of the process" },
		},
	}, {
		"fc", "f", fc_handler,
		"Find instances of a C++ class",
		"Scan through kernel memory for instances of a C++ class and print the address of "
		"each instance.",
		ARGSPEC(5) {
			{ "b",      "kext",   ARG_STRING, "The bundle ID of the kext defining the class" },
			{ "k",      NULL,     ARG_NONE,   "The class is defined in the kernel"           },
			{ "x",      "access", ARG_WIDTH,  "The memory access width"                      },
			{ ARGUMENT, "class",  ARG_STRING, "The C++ class name"                           },
			{ OPTIONAL, "range",  ARG_RANGE,  "The address range to search"                  },
		},
	}, {
		"lc", NULL, lc_handler,
		"Look up the class of a C++ object",
		"Try to determine the C++ class from a C++ object pointer.",
		ARGSPEC(1) {
			{ ARGUMENT, "address", ARG_ADDRESS, "The address of the C++ object" },
		},
	}, {
		"cm", NULL, cm_handler,
		"Get the C++ metaclass pointer",
		"Find the C++ metaclass instance for a class.",
		ARGSPEC(3) {
			{ "b",      "kext",  ARG_STRING, "The bundle ID of the kext defining the class" },
			{ "k",      NULL,    ARG_NONE,   "The class is defined in the kernel"           },
			{ ARGUMENT, "class", ARG_STRING, "The C++ class name"                           },
		},
	}, {
		"cz", NULL, cz_handler,
		"Get the size of a C++ class",
		"Find the size of a C++ class.",
		ARGSPEC(3) {
			{ "b",      "kext",  ARG_STRING, "The bundle ID of the kext defining the class" },
			{ "k",      NULL,    ARG_NONE,   "The class is defined in the kernel"           },
			{ ARGUMENT, "class", ARG_STRING, "The C++ class name"                           },
		},
	}, {
		"kp", NULL, kp_handler,
		"Translate virtual to physical address",
		"Translate a kernel virtual address to a physical address.",
		ARGSPEC(1) {
			{ ARGUMENT, "address", ARG_ADDRESS, "The kernel virtual address" },
		},
	}, {
		"kpm", "kp", kpm_handler,
		"Print virtual to physical address map",
		"Print a mapping from kernel virtual addresses to physical addresses.",
		ARGSPEC(1) {
			{ ARGUMENT, "range", ARG_RANGE, "The range of virtual addresses" },
		},
	}, {
		"zs", NULL, zs_handler,
		"Get zalloc memory size",
		"Call the kernel function zone_element_size to determine the allocation size of "
		"the specified address.",
		ARGSPEC(1) {
			{ ARGUMENT, "address", ARG_ADDRESS, "The virtual address" },
		},
	}, {
		"pca", NULL, pca_handler,
		"Show physical cache attributes",
		"Call the kernel function pmap_cache_attributes to retrieve physical cache "
		"attributes (WIMG bits) for the specified physical (or virtual) address.",
		ARGSPEC(2) {
			{ "v",      NULL,      ARG_NONE,    "Use a virtual address"           },
			{ ARGUMENT, "address", ARG_ADDRESS, "The physical or virtual address" },
		},
	}, {
		"vt", NULL, vt_handler,
		"Find the vtable of a C++ class",
		"Print the address of the virtual method table for a C++ class.",
		ARGSPEC(3) {
			{ "b",      "kext",  ARG_STRING, "The bundle ID of the kext defining the class" },
			{ "k",      NULL,    ARG_NONE,   "The class is defined in the kernel"           },
			{ ARGUMENT, "class", ARG_STRING, "The C++ class name"                           },
		},
	}, {
		"vtl", "vt", vtl_handler,
		"Look up the class name for a vtable",
		"Print the name of the C++ class for a given C++ virtual method table.",
		ARGSPEC(1) {
			{ ARGUMENT, "address", ARG_ADDRESS, "The vtable address" },
		},
	}, {
		"vm", NULL, vm_handler,
		"Show virtual memory information",
		"Show virtual memory information for the memory region containing the given "
		"address.",
		ARGSPEC(2) {
			{ "d",      "depth",   ARG_UINT,    "The maximum submap depth" },
			{ ARGUMENT, "address", ARG_ADDRESS, "The virtual address"      },
		},
	}, {
		"vmm", "vm", vmm_handler,
		"Show virtual memory information for range",
		"Show virtual memory information for all memory regions intersecting the given "
		"memory range.",
		ARGSPEC(2) {
			{ "d",      "depth", ARG_UINT,  "The maximum submap depth"       },
			{ OPTIONAL, "range", ARG_RANGE, "The range of virtual addresses" },
		},
	}, {
		"vma", "vm", vma_handler,
		"Allocate virtual memory",
		"Allocate virtual memory using mach_vm_allocate.",
		ARGSPEC(1) {
			{ ARGUMENT, "size", ARG_UINT, "The number of bytes to allocate" },
		}
	}, {
		"vmd", "vm", vmd_handler,
		"Deallocate virtual memory",
		"Deallocate virtual memory using mach_vm_deallocate."
		"\nIf size is not specified, then it defaults to the number of bytes from address "
		"to the end of the region.",
		ARGSPEC(2) {
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to deallocate"         },
			{ OPTIONAL, "size",    ARG_UINT,    "The number of bytes to deallocate" },
		}
	}, {
		"vmp", "vm", vmp_handler,
		"Set virtual memory protection",
		"Set the virtual memory protection bits for a virtual memory region."
		"\nIf length is not specified, then it defaults to 1, meaning the new protection "
		"will be set on only the one page containing the given address.",
		ARGSPEC(3) {
			{ ARGUMENT, "protection", ARG_STRING,  "The new virtual memory protection (e.g. 'r-x')" },
			{ ARGUMENT, "address",    ARG_ADDRESS, "The address to protect"                         },
			{ OPTIONAL, "length",     ARG_UINT,    "The length of the region"                       },
		},
	}, {
		"ks", NULL, ks_handler,
		"Kernel slide",
		"Print the kernel slide, or if address is specified, add the kernel slide to the "
		"address.",
		ARGSPEC(2) {
			{ "u",      NULL,      ARG_NONE,    "Unslide the address"  },
			{ OPTIONAL, "address", ARG_ADDRESS, "The address to slide" },
		},
	}, {
		"a", NULL, a_handler,
		"Find the address of a symbol",
		"Print the address of a kernel symbol.",
		ARGSPEC(1) {
			{ ARGUMENT, "symbol", ARG_SYMBOL, "The symbol to resolve" },
		},
	}, {
		"ap", "a", ap_handler,
		"Address permutation",
		"Print the address permutation value, or if address is specified, permute the "
		"address."
		"\nThis functionality may not be available on iOS.",
		ARGSPEC(2) {
			{ "u",      NULL,      ARG_NONE,    "Unpermute the address"  },
			{ OPTIONAL, "address", ARG_ADDRESS, "The address to permute" },
		},
	}, {
		"s", NULL, s_handler,
		"Find the symbol for an address",
		"Print information about the kernel symbol containing an address.",
		ARGSPEC(1) {
			{ ARGUMENT, "address", ARG_ADDRESS, "The address to resolve" },
		},
	}, {
		"kcd", NULL, kcd_handler,
		"Decompress a kernelcache",
		"Decompress a kernelcache and write it to stdout or a file.",
		ARGSPEC(2) {
			{ "o",      "output", ARG_STRING, "The output file"      },
#if KERNELCACHE
			{ OPTIONAL, "file",   ARG_STRING, "The kernelcache file" },
#else
			{ ARGUMENT, "file",   ARG_STRING, "The kernelcache file" },
#endif
		},
	}, {
		"root", NULL, root_handler,
		"Exec a root shell",
		"Execute a shell with UID and GID 0. The shell replaces the current process.",
		0, NULL,
	}
#if MEMCTL_REPL
	, {
		"quit", NULL, quit_handler,
		"Exit the REPL",
		"If running in a REPL, exit the REPL.",
		0, NULL,
	}
#endif
};

struct cli cli = {
	default_action,
	sizeof(commands) / sizeof(commands[0]),
	commands,
};
