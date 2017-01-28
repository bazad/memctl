#include "cli/cli.h"

#include "cli/error.h"

#include "kernel.h"

#include <stdio.h>
#include <string.h>

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
	OPT_GET_OR_(n_, opt_, arg_, ((struct argrange) { start_, end_ }), ARG_RANGE, range)

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
	ARG_GET_OR_(n_, arg_, ((struct argrange) { start_, end_ }), ARG_RANGE, range)

static bool r_handler(const struct argument *arguments) {
	size_t width    = OPT_GET_WIDTH_OR(0, "", "width", sizeof(kword_t));
	bool dump       = OPT_PRESENT(1, "d");
	bool physical   = OPT_PRESENT(2, "p");
	size_t access   = OPT_GET_WIDTH_OR(3, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(4, "address");
	size_t length;
	if (ARG_PRESENT(5, "length")) {
		length = ARG_GET_UINT(5, "length");
	} else if (dump) {
		length = 256;
	} else {
		length = width;
	}
	return r_command(address, length, physical, width, access, dump);
}

static bool rb_handler(const struct argument *arguments) {
	bool physical   = OPT_PRESENT(0, "p");
	size_t access   = OPT_GET_WIDTH_OR(1, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(2, "address");
	size_t length   = ARG_GET_UINT(3, "length");
	return rb_command(address, length, physical, access);
}

static bool rs_handler(const struct argument *arguments) {
	bool physical   = OPT_PRESENT(0, "p");
	size_t access   = OPT_GET_WIDTH_OR(1, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(2, "address");
	size_t length   = ARG_GET_UINT_OR(3, "length", -1);
	return rs_command(address, length, physical, access);
}

static bool w_handler(const struct argument *arguments) {
	size_t width    = OPT_GET_WIDTH_OR(0, "", "width", sizeof(kword_t));
	bool physical   = OPT_PRESENT(1, "p");
	size_t access   = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	kaddr_t address = ARG_GET_ADDRESS(3, "address");
	kword_t value   = ARG_GET_UINT(4, "value");
	return w_command(address, value, physical, width, access);
}

static bool wd_handler(const struct argument *arguments) {
	bool physical       = OPT_PRESENT(0, "p");
	size_t access       = OPT_GET_WIDTH_OR(1, "x", "access", 0);
	kaddr_t address     = ARG_GET_ADDRESS(2, "address");
	struct argdata data = ARG_GET_DATA(3, "data");
	return wd_command(address, data.data, data.length, physical, access);
}

static bool ws_handler(const struct argument *arguments) {
	bool physical      = OPT_PRESENT(0, "p");
	size_t access      = OPT_GET_WIDTH_OR(1, "x", "access", 0);
	kaddr_t address    = ARG_GET_ADDRESS(2, "address");
	const char *string = ARG_GET_STRING(3, "string");
	return ws_command(address, string, physical, access);
}

static bool f_handler(const struct argument *arguments) {
	size_t width          = OPT_GET_WIDTH_OR(0, "", "width", sizeof(kword_t));
	bool physical         = OPT_PRESENT(1, "p");
	size_t access         = OPT_GET_WIDTH_OR(2, "x", "access", 0);
	size_t alignment      = OPT_GET_WIDTH_OR(3, "a", "alignment", width);
	struct argrange range = ARG_GET_RANGE(4, "range");
	kword_t value         = ARG_GET_UINT(5, "value");
	return f_command(range.start, range.end, value, width, physical, access, alignment);
}

static bool fpr_handler(const struct argument *arguments) {
	intmax_t ipid = ARG_GET_INT(0, "pid");
	pid_t pid = ipid;
	if ((intmax_t)pid != ipid) {
		error_usage("fpr", NULL, "invalid PID %lld\n", ipid);
		return false;
	}
	return fpr_command(pid);
}

static bool fi_handler(const struct argument *arguments) {
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
	return fi_command(range.start, range.end, classname, bundle_id, access);
}

static bool kp_handler(const struct argument *arguments) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return kp_command(address);
}

static bool kpm_handler(const struct argument *arguments) {
	struct argrange range = ARG_GET_RANGE(0, "range");
	return kpm_command(range.start, range.end);
}

static bool vt_handler(const struct argument *arguments) {
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

static bool vm_handler(const struct argument *arguments) {
	unsigned depth  = OPT_GET_UINT_OR(0, "d", "depth", 2048);
	kaddr_t address = ARG_GET_ADDRESS(1, "address");
	return vm_command(address, depth);
}

static bool vmm_handler(const struct argument *arguments) {
	unsigned depth        = OPT_GET_UINT_OR(0, "d", "depth", 2048);
	struct argrange range = ARG_GET_RANGE_OR(1, "range", 0, -1);
	return vmm_command(range.start, range.end, depth);
}

static bool ks_handler(const struct argument *arguments) {
	bool unslide    = OPT_PRESENT(0, "u");
	kaddr_t address = ARG_GET_ADDRESS_OR(1, "address", 0);
	return ks_command(address, unslide);
}

static bool a_handler(const struct argument *arguments) {
	struct argsymbol symbol = ARG_GET_SYMBOL(0, "symbol");
	return a_command(symbol.symbol, symbol.kext);
}

static bool ap_handler(const struct argument *arguments) {
	bool unpermute  = OPT_PRESENT(0, "u");
	kaddr_t address = ARG_GET_ADDRESS_OR(1, "address", 0);
	return ap_command(address, unpermute);
}

static bool s_handler(const struct argument *arguments) {
	kaddr_t address = ARG_GET_ADDRESS(0, "address");
	return s_command(address);
}

static struct command commands[] = {
	{
		"r", NULL, r_handler,
		"read memory",
		6, (struct argspec *) &(struct argspec[6]) {
			{ "",       "width",   ARG_WIDTH,   "display width"               },
			{ "d",      NULL,      ARG_NONE,    "use dump format"             },
			{ "p",      NULL,      ARG_NONE,    "read physical memory"        },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"         },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to read"         },
			{ OPTIONAL, "length",  ARG_UINT,    "the number of bytes to read" },
		},
	}, {
		"rb", "r", rb_handler,
		"read raw binary data",
		4, (struct argspec *) &(struct argspec[4]) {
			{ "p",      NULL,      ARG_NONE,    "read physical memory"        },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"         },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to read"         },
			{ ARGUMENT, "length",  ARG_UINT,    "the number of bytes to read" },
		},
	}, {
		"rs", "r", rs_handler,
		"read string",
		4, (struct argspec *) &(struct argspec[4]) {
			{ "p",      NULL,      ARG_NONE,    "read physical memory"  },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to read"   },
			{ OPTIONAL, "length",  ARG_UINT,    "maximum string length" },
		},
	}, {
		"w", NULL, w_handler,
		"write memory",
		5, (struct argspec *) &(struct argspec[5]) {
			{ "",       "width",   ARG_WIDTH,   "value width"           },
			{ "p",      NULL,      ARG_NONE,    "write physical memory" },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to write"  },
			{ ARGUMENT, "value",   ARG_UINT,    "the value to write"    },
		},
	}, {
		"wd", "w", wd_handler,
		"write data to memory",
		4, (struct argspec *) &(struct argspec[4]) {
			{ "p",      NULL,      ARG_NONE,    "write physical memory" },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to write"  },
			{ ARGUMENT, "data",    ARG_DATA,    "the data to write"     },
		},
	}, {
		"ws", "w", ws_handler,
		"write string to memory",
		4, (struct argspec *) &(struct argspec[4]) {
			{ "p",      NULL,      ARG_NONE,    "write physical memory" },
			{ "x",      "access",  ARG_WIDTH,   "memory access width"   },
			{ ARGUMENT, "address", ARG_ADDRESS, "the address to write"  },
			{ ARGUMENT, "string",  ARG_STRING,  "the string to write"   },
		},
	}, {
		"f", NULL, f_handler,
		"find in memory",
		6, (struct argspec *) &(struct argspec[6]) {
			{ "",       "width",  ARG_WIDTH, "value width"                 },
			{ "p",      NULL,     ARG_NONE,  "search physical memory"      },
			{ "x",      "access", ARG_WIDTH, "memory access width"         },
			{ "a",      "align",  ARG_WIDTH, "search alignment"            },
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
		"fi", "f", fi_handler,
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
		"vmm", NULL, vmm_handler,
		"show virtual memory info",
		2, (struct argspec *) &(struct argspec[2]) {
			{ "d",      "depth", ARG_UINT,  "submap depth"             },
			{ OPTIONAL, "range", ARG_RANGE, "kernel virtual addresses" },
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
	}
};

struct cli cli = {
	default_action,
	sizeof(commands) / sizeof(commands[0]),
	commands,
};
