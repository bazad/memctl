#include "cli.h"

#include "error.h"
#include "format.h"

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
// TODO: This is a hack. It should be possible to have memctl configure these by loading the core
// and querying the kernel map.
#if __arm64__
# define RANGE_DEFAULT_START	0xfffffff000000000
# define RANGE_DEFAULT_END	0xfffffff280000000
#elif __x86_64__
# define RANGE_DEFAULT_START	0xffffff7f80000000
# define RANGE_DEFAULT_END	0xffffff9400000000
#else
# error No default kernel virtual memory range for architecture.
#endif

/*
 * default_virtual_range
 *
 * Description:
 * 	If either endpoint of the range was a default, then specify a realistic virtual address
 * 	instead.
 */
static void
default_virtual_range(struct argrange *range) {
	if (range->default_start) {
		range->start = RANGE_DEFAULT_START;
	}
	if (range->default_end) {
		range->end = RANGE_DEFAULT_END;
	}
}

/*
 * parse_protection
 *
 * Description:
 * 	Parse the given protection string (e.g. 'r-x').
 */
static bool
parse_protection(const char *command, const char *protection, int *pprot) {
	int prot = 0;
	for (size_t i = 0; ; i++) {
		char c = protection[i];
		if (c == 0) {
			if (i == 0) {
				break;
			}
			*pprot = prot;
			return true;
		} else if (i == 3) {
			break;
		}
		c = tolower(c);
		int flag;
		if (c == 'r') {
			flag = VM_PROT_READ;
		} else if (c == 'w') {
			flag = VM_PROT_WRITE;
		} else if (c == 'x') {
			flag = VM_PROT_EXECUTE;
		} else if (c == '-') {
			continue;
		} else {
			break;
		}
		if (prot & flag) {
			break;
		}
		prot |= flag;
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
	struct argrange range = ARG_GET_RANGE(5, "range");
	kword_t value         = ARG_GET_UINT(6, "value");
	if (physical && heap) {
		error_usage("f", NULL, "p and h options are mutually exclusive");
		return false;
	}
	if (!heap && !physical) {
		default_virtual_range(&range);
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
	struct argrange range = ARG_GET_RANGE(3, "range");
	const char *classname = ARG_GET_STRING(4, "class");
	if (in_kernel) {
		if (bundle_id != NULL) {
			error_usage("fi", NULL, "b and k options are mutually exclusive");
			return false;
		}
		bundle_id = KERNEL_ID;
	}
	return fc_command(range.start, range.end, classname, bundle_id, access);
}

HANDLER(kp_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return kp_command(address);
}

HANDLER(kpm_handler) {
	struct argrange range = ARG_GET_RANGE(0, "range");
	default_virtual_range(&range);
	return kpm_command(range.start, range.end);
}

HANDLER(kz_handler) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return kz_command(address);
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
	if (in_kernel) {
		if (bundle_id != NULL) {
			error_usage("vt", NULL, "b and k options are mutually exclusive");
			return false;
		}
		bundle_id = KERNEL_ID;
	}
	return vt_command(classname, bundle_id);
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

static struct command commands[] = {
	{
		"i", NULL, i_handler,
		"print system information",
		0, NULL,
	}, {
		"r", NULL, r_handler,
		"read kernel memory",
		7, (struct argspec *) &(struct argspec[7]) {
			{ "",       "width",   ARG_WIDTH,   "display width"               },
			{ "d",      NULL,      ARG_NONE,    "use dump format"             },
			{ "f",      NULL,      ARG_NONE,    "force read (unsafe)"         },
			{ "p",      NULL,      ARG_NONE,    "read physical memory"        },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"         },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to read"         },
			{ OPTIONAL, "length",  ARG_UINT,    "the number of bytes to read" },
		},
	}, {
		"rb", "r", rb_handler,
		"read raw binary data",
		5, (struct argspec *) &(struct argspec[5]) {
			{ "f",      NULL,      ARG_NONE,    "force read (unsafe)"         },
			{ "p",      NULL,      ARG_NONE,    "read physical memory"        },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"         },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to read"         },
			{ ARGUMENT, "length",  ARG_UINT,    "the number of bytes to read" },
		},
#if MEMCTL_DISASSEMBLY
	}, {
		"ri", "r", ri_handler,
		"read instructions (disassemble)",
		5, (struct argspec *) &(struct argspec[5]) {
			{ "f",      NULL,      ARG_NONE,    "force read (unsafe)"                },
			{ "p",      NULL,      ARG_NONE,    "read physical memory"               },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"                },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to read"                },
			{ ARGUMENT, "length",  ARG_UINT,    "the number of bytes to disassemble" },
		},
	}, {
		"rif", "ri", rif_handler,
		"disassemble function",
		3, (struct argspec *) &(struct argspec[3]) {
			{ "f",      NULL,       ARG_NONE,   "force read (unsafe)"         },
			{ "x",      "access",   ARG_WIDTH,  "memory access width"         },
			{ ARGUMENT, "function", ARG_SYMBOL, "the function to disassemble" },
		},
#endif // MEMCTL_DISASSEMBLY
	}, {
		"rs", "r", rs_handler,
		"read string",
		5, (struct argspec *) &(struct argspec[5]) {
			{ "f",      NULL,      ARG_NONE,    "force read (unsafe)"   },
			{ "p",      NULL,      ARG_NONE,    "read physical memory"  },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to read"   },
			{ OPTIONAL, "length",  ARG_UINT,    "maximum string length" },
		},
	}, {
		"w", NULL, w_handler,
		"write memory",
		6, (struct argspec *) &(struct argspec[6]) {
			{ "",       "width",   ARG_WIDTH,   "value width"           },
			{ "f",      NULL,      ARG_NONE,    "force write (unsafe)"  },
			{ "p",      NULL,      ARG_NONE,    "write physical memory" },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to write"  },
			{ ARGUMENT, "value",   ARG_UINT,    "the value to write"    },
		},
	}, {
		"wd", "w", wd_handler,
		"write data to memory",
		5, (struct argspec *) &(struct argspec[5]) {
			{ "f",      NULL,      ARG_NONE,    "force write (unsafe)"  },
			{ "p",      NULL,      ARG_NONE,    "write physical memory" },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to write"  },
			{ ARGUMENT, "data",    ARG_DATA,    "the data to write"     },
		},
	}, {
		"ws", "w", ws_handler,
		"write string to memory",
		5, (struct argspec *) &(struct argspec[5]) {
			{ "f",      NULL,      ARG_NONE,    "force write (unsafe)"  },
			{ "p",      NULL,      ARG_NONE,    "write physical memory" },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to write"  },
			{ ARGUMENT, "string",  ARG_STRING,  "the string to write"   },
		},
	}, {
		"f", NULL, f_handler,
		"find in memory",
		7, (struct argspec *) &(struct argspec[7]) {
			{ "",       "width",  ARG_WIDTH, "value width"                 },
			{ "p",      NULL,     ARG_NONE,  "search physical memory"      },
			{ "h",      NULL,     ARG_NONE,  "search heap memory"          },
			{ "a",      "align",  ARG_WIDTH, "search alignment"            },
			{ "x",      "access", ARG_WIDTH, "memory access width"         },
			{ ARGUMENT, "range",  ARG_RANGE, "the address range to search" },
			{ ARGUMENT, "value",  ARG_UINT,  "the value to find"           },
		},
	}, {
		"fpr", "f", fpr_handler,
		"find proc struct",
		1, (struct argspec *) &(struct argspec[1]) {
			{ ARGUMENT, "pid", ARG_INT, "pid of process" },
		},
	}, {
		"fc", "f", fc_handler,
		"find instances of C++ class",
		5, (struct argspec *) &(struct argspec[5]) {
			{ "b",      "kext",   ARG_STRING, "bundle ID of kext defining class" },
			{ "k",      NULL,     ARG_NONE,   "class defined in kernel"          },
			{ "x",      "access", ARG_WIDTH,  "memory access width"              },
			{ ARGUMENT, "range",  ARG_RANGE,  "the address range to search"      },
			{ ARGUMENT, "class",  ARG_STRING, "C++ class name"                   },
		},
	}, {
		"kp", NULL, kp_handler,
		"translate to physical address",
		1, (struct argspec *) &(struct argspec[1]) {
			{ ARGUMENT, "address", ARG_ADDRESS, "kernel virtual address" },
		},
	}, {
		"kpm", "kp", kpm_handler,
		"map to physical pages",
		1, (struct argspec *) &(struct argspec[1]) {
			{ ARGUMENT, "range", ARG_RANGE, "range of virtual addresses" },
		},
	}, {
		"kz", NULL, kz_handler,
		"get size of kalloc memory",
		1, (struct argspec *) &(struct argspec[1]) {
			{ ARGUMENT, "address", ARG_ADDRESS, "kalloc address" },
		},
	}, {
		"pca", NULL, pca_handler,
		"show physical cache attributes",
		2, (struct argspec *) &(struct argspec[2]) {
			{ "v",      NULL,      ARG_NONE,    "use a virtual address"       },
			{ ARGUMENT, "address", ARG_ADDRESS, "physical or virtual address" },
		},
	}, {
		"vt", NULL, vt_handler,
		"vtable address of C++ class",
		3, (struct argspec *) &(struct argspec[3]) {
			{ "b",      "kext",  ARG_STRING, "bundle ID of kext defining class" },
			{ "k",      NULL,    ARG_NONE,   "class defined in kernel"          },
			{ ARGUMENT, "class", ARG_STRING, "C++ class name"                   },
		},
	}, {
		"vm", NULL, vm_handler,
		"show virtual memory info",
		2, (struct argspec *) &(struct argspec[2]) {
			{ "d",      "depth",   ARG_UINT,    "submap depth"           },
			{ ARGUMENT, "address", ARG_ADDRESS, "kernel virtual address" },
		},
	}, {
		"vmm", "vm", vmm_handler,
		"show virtual memory info",
		2, (struct argspec *) &(struct argspec[2]) {
			{ "d",      "depth", ARG_UINT,  "submap depth"             },
			{ OPTIONAL, "range", ARG_RANGE, "kernel virtual addresses" },
		},
	}, {
		"vmp", "vm", vmp_handler,
		"set virtual memory protection",
		3, (struct argspec *) &(struct argspec[3]) {
			{ ARGUMENT, "protection", ARG_STRING,  "protection"         },
			{ ARGUMENT, "address",    ARG_ADDRESS, "address to protect" },
			{ OPTIONAL, "length",     ARG_UINT,    "length of region"   },
		},
	}, {
		"ks", NULL, ks_handler,
		"kernel slide",
		2, (struct argspec *) &(struct argspec[2]) {
			{ "u",      NULL,      ARG_NONE,    "unslide" },
			{ OPTIONAL, "address", ARG_ADDRESS, "address" },
		},
	}, {
		"a", NULL, a_handler,
		"address of symbol",
		1, (struct argspec *) &(struct argspec[1]) {
			{ ARGUMENT, "symbol", ARG_SYMBOL, "symbol to resolve" },
		},
	}, {
		"ap", "a", ap_handler,
		"address permutation",
		2, (struct argspec *) &(struct argspec[2]) {
			{ "u",      NULL,      ARG_NONE,    "unpermute" },
			{ OPTIONAL, "address", ARG_ADDRESS, "address"   },
		},
	}, {
		"s", NULL, s_handler,
		"symbol for address",
		1, (struct argspec *) &(struct argspec[1]) {
			{ ARGUMENT, "address", ARG_ADDRESS, "address to resolve" },
		},
	}, {
		"kcd", NULL, kcd_handler,
		"kernelcache decompress",
		2, (struct argspec *) &(struct argspec[2]) {
			{ "o",      "output", ARG_STRING, "output file"      },
#if KERNELCACHE
			{ OPTIONAL, "file",   ARG_STRING, "kernelcache file" },
#else
			{ ARGUMENT, "file",   ARG_STRING, "kernelcache file" },
#endif
		},
	}, {
		"root", NULL, root_handler,
		"exec a root shell",
		0, NULL,
	}
#if MEMCTL_REPL
	, {
		"quit", NULL, quit_handler,
		"exit the REPL",
		0, NULL,
	}
#endif
};

struct cli cli = {
	default_action,
	sizeof(commands) / sizeof(commands[0]),
	commands,
};
