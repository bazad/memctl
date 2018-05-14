#include "command.h"

#include "error.h"
#include "format.h"
#include "initialize.h"
#include "strparse.h"

#include "memctl/kernel.h"
#include "memctl/utility.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * struct state
 *
 * Description:
 * 	State needed during option and argument parsing.
 */
struct state {
	// The current argv string. If part of the current argument string has already been
	// processed, arg might point into the middle of the argv string. NULL when no more
	// arguments are left.
	const char *arg;
	// The argument index. This is used to determine whether an argument may be part of the
	// compact format.
	int argidx;
	// The current argument being populated.
	struct argument *argument;
	// The current (or most recently processed) option, or NULL if options are not currently
	// being processed.
	const struct argspec *option;
	// When processing nameless options, the error message for a failed match will be
	// discarded. The keep_error flag signals that the match succeeded enough that the error
	// message should still be displayed and the command aborted.
	bool keep_error;
	// When processing an option starting with a dash, we sometimes can't tell if this is an
	// unrecognized option or an argument that starts with a dash. If option processing fails,
	// it will fall back to argument parsing. If that also fails, it will print the
	// unrecognized option error message.
	const char *bad_option;
	// The start index for iterating options or arguments.
	unsigned start;
	// The end index for iterating options or arguments.
	unsigned end;
	// The command that has been matched.
	const struct command *command;
	// The vector of arguments to populate.
	struct argument *arguments;
	// argc.
	int argc;
	// argv. Must be NULL-terminated.
	const char **argv;
};

/*
 * struct savepoint
 *
 * Description:
 * 	Represents a position in processing the stream of arguments.
 */
struct savepoint {
	const char *arg;
	int argidx;
	int argc;
	const char **argv;
};

typedef bool (*parse_fn)(struct state *);

static bool parse_none(struct state *s);
static bool parse_int(struct state *s);
static bool parse_uint(struct state *s);
static bool parse_width(struct state *s);
static bool parse_data(struct state *s);
static bool parse_string(struct state *s);
static bool parse_argv(struct state *s);
static bool parse_symbol(struct state *s);
static bool parse_address(struct state *s);
static bool parse_range(struct state *s);
static bool parse_word(struct state *s);

static parse_fn parse_fns[] = {
	parse_none,
	parse_int,
	parse_uint,
	parse_width,
	parse_data,
	parse_string,
	parse_argv,
	parse_symbol,
	parse_address,
	parse_range,
	parse_word,
};

#define ERROR_USAGE(fmt, ...)				\
	error_usage(NULL, NULL, fmt, ##__VA_ARGS__)

#define ERROR_COMMAND(s, fmt, ...)						\
	({ struct state *s0 = (s);						\
	   error_usage(s0->command->command, NULL, fmt, ##__VA_ARGS__); })

#define ERROR_OPTION(s, fmt, ...)						\
	({ struct state *s0 = (s);						\
	   const char *opt = (s0->option == NULL ? NULL : s0->option->option);	\
	   error_usage(s0->command->command, opt, fmt, ##__VA_ARGS__); })

/*
 * spec_is_option
 *
 * Description:
 * 	Returns true if the given argspec represents an option instead of an argument.
 */
static bool
spec_is_option(const struct argspec *spec) {
	return !(spec->option == ARGUMENT || spec->option == OPTIONAL);
}

// Macros used in the write_* functions below.
#define BUF()	(buf == NULL ? NULL : buf + written)
#define SIZE()	(buf == NULL ? 0 : size - written)
#define WRITE(...)										\
	written += snprintf(BUF(), SIZE(), __VA_ARGS__);					\
	if (buf != NULL && written >= size) {							\
		goto fail;									\
	}

/*
 * write_argspec_usage_oneline
 *
 * Description:
 * 	Write a string describing the argspec to the given buffer. The number of characters needed
 * 	to store the full string is returned.
 */
static size_t
write_argspec_usage_oneline(const struct argspec *argspec, char *buf, size_t size) {
	size_t written = 0;
	if (argspec->option == ARGUMENT) {
		WRITE("<%s>", argspec->argument);
	} else if (argspec->option == OPTIONAL) {
		WRITE("[%s]", argspec->argument);
	} else {
		if (argspec->type == ARG_NONE) {
			WRITE("[-%s]", argspec->option);
		} else if (argspec->option[0] != 0) {
			WRITE("[-%s %s]", argspec->option, argspec->argument);
		} else {
			assert(argspec->type != ARG_NONE);
			WRITE("[%s]", argspec->argument);
		}
	}
	return written;
fail:
	return 0;
}

/*
 * write_command_usage_oneline
 *
 * Description:
 * 	Write a string describing the usage of the command to the given buffer. The number of
 * 	characters needed to store the full string is returned.
 */
static size_t
write_command_usage_oneline(const struct command *command, char *buf, size_t size,
		bool abbreviated) {
	size_t written = 0;
	WRITE("%s", command->command);
	for (size_t i = 0; i < command->argspecc; i++) {
		const struct argspec *s = &command->argspecv[i];
		// If we're doing an abbreviated usage, skip options.
		if (abbreviated && spec_is_option(s)) {
			continue;
		}
		// Insert a space unless it's an unnamed option.
		if (!(spec_is_option(s) && s->option[0] == 0)) {
			WRITE(" ");
		}
		written += write_argspec_usage_oneline(s, BUF(), SIZE());
	}
	return written;
fail:
	return 0;
}

#undef BUF
#undef SIZE
#undef WRITE

/*
 * help_all
 *
 * Description:
 * 	Generate a general help message.
 */
static bool
help_all() {
	const struct command *c = cli.commands;
	const struct command *end = c + cli.command_count;
	// Get the length of the usage string.
	size_t usage_length = 4;
	for (; c < end; c++) {
		size_t length = write_command_usage_oneline(c, NULL, 0, true);
		if (length > usage_length) {
			usage_length = length;
		}
	}
	// Print the usage strings.
	usage_length += 1;
	char *buf = malloc(usage_length);
	if (buf == NULL) {
		error_out_of_memory();
		return false;
	}
	for (c = cli.commands; c < end; c++) {
		write_command_usage_oneline(c, buf, usage_length, true);
		fprintf(stderr, "%-*s %s\n", (int)usage_length, buf, c->short_description);
	}
	free(buf);
	return true;
}

/*
 * break_and_print_indented_string
 *
 * Description:
 * 	Print a string with indentation, breaking it with newlines every so often.
 */
static void
break_and_print_indented_string(FILE *file, size_t indent, const char *string) {
	const size_t columns = 80;
	const size_t max_chars = columns - indent;
	size_t next_pos = 0;
	for (;;) {
		// Loop until:
		// 1. We reach the maximum line length, in which case we will print print_len
		//    characters and then start the next line at position next_pos;
		// 2. We encounter a NULL terminator, in which case we print the full line and
		//    abort;
		// 3. We encounter a newline, which means we are starting a new paragraph.
		bool new_paragraph = false;
		size_t begin     = next_pos;
		size_t pos       = begin;
		size_t print_len = 0;
		for (size_t n = 0;; n++, pos++) {
			if (n >= max_chars) {
				// If we haven't yet found a place to divide, just print the whole
				// block.
				if (print_len == 0) {
					print_len = n;
					next_pos = pos;
				}
				break;
			}
			if (string[pos] == 0) {
				print_len = n;
				next_pos  = 0;
				break;
			}
			if (string[pos] == ' ' || string[pos] == '\n') {
				print_len = n;
				next_pos = pos + 1;
			}
			if (string[pos] == '\n') {
				// Do a new paragraph.
				new_paragraph = true;
				break;
			}
		}
		// Now, print the print_len characters from the line starting at position begin.
		fprintf(file, "%*s%.*s\n", (int)indent, "", (int)print_len, string + begin);
		// Handle a new paragraph.
		if (new_paragraph) {
			fprintf(file, "\n");
		}
		// End the loop once we finish the string.
		if (next_pos == 0) {
			break;
		}
	}
}

/*
 * help_command
 *
 * Description:
 * 	Generate a help message for the given command.
 */
static bool
help_command(const struct command *command) {
	bool success = false;
	// Print out the oneline usage and description.
	size_t length = write_command_usage_oneline(command, NULL, 0, false);
	char *buf = malloc(length + 1);
	if (buf == NULL) {
		error_out_of_memory();
		goto fail;
	}
	write_command_usage_oneline(command, buf, length + 1, false);
	fprintf(stderr, "\n%s\n\n", buf);
	free(buf);
	break_and_print_indented_string(stderr, 4, command->long_description);
	// Get the argspec length.
	const struct argspec *s = command->argspecv;
	const struct argspec *const send = s + command->argspecc;
	size_t argspec_length = 4;
	for (; s < send; s++) {
		length = write_argspec_usage_oneline(s, NULL, 0);
		if (length > argspec_length) {
			argspec_length = length;
		}
	}
	// Print out the options and arguments.
	argspec_length += 1;
	buf = malloc(argspec_length);
	if (buf == NULL) {
		error_out_of_memory();
		goto fail;
	}
	bool printed_options = false;
	bool printed_arguments = false;
	for (s = command->argspecv; s < send; s++) {
		// Print out the section header.
		if (spec_is_option(s) && !printed_options) {
			fprintf(stderr, "\nOptions:\n");
			printed_options = true;
		} else if (!spec_is_option(s) && !printed_arguments) {
			fprintf(stderr, "\nArguments:\n");
			printed_arguments = true;
		}
		// Print out details for this option or argument.
		write_argspec_usage_oneline(s, buf, argspec_length);
		fprintf(stderr, "    %-*s %s\n", (int)argspec_length, buf, s->description);
	}
	fprintf(stderr, "\n");
	free(buf);
	success = true;
fail:
	return success;
}

/*
 * state_save
 *
 * Description:
 * 	Save the current parsing position in the given savepoint.
 */
static void
state_save(const struct state *s, struct savepoint *save) {
	save->arg    = s->arg;
	save->argidx = s->argidx;
	save->argc   = s->argc;
	save->argv   = s->argv;
}

/*
 * state_restore
 *
 * Description:
 * 	Restore the current parsing position to the given savepoint.
 */
static void
state_restore(struct state *s, const struct savepoint *save) {
	s->arg    = save->arg;
	s->argidx = save->argidx;
	s->argc   = save->argc;
	s->argv   = save->argv;
}

/*
 * advance
 *
 * Description:
 * 	Advance arg to the next argv if the current argv has no more data. Returns true if the
 * 	state has been advanced to a new argv or if there is no more data available.
 *
 * Notes:
 * 	This function is not idempotent. In particular, if we are at the end of argument 1 and
 * 	argument 2 is the empty string, the first call to advance will move to argument 2 and the
 * 	next call to advance will move to argument 3.
 */
static bool
advance(struct state *s) {
	if (s->arg == NULL) {
		assert(s->argc == 0);
		return true;
	}
	if (*s->arg == 0) {
		s->argv++;
		s->argc--;
		s->arg = s->argv[0];
		s->argidx++;
		assert(s->argc > 0 || s->arg == NULL);
		return true;
	}
	return false;
}

static bool
parse_none(struct state *s) {
	// s->argument->type is already ARG_NONE.
	return true;
}

/*
 * parse_int_internal
 *
 * Description:
 * 	Parse an integer value of the given argument type.
 */
static bool
parse_int_internal(struct state *s, struct argument *argument, enum argtype argtype, size_t len) {
	const char *end;
	uintmax_t address;
	unsigned base = 10;
	uintmax_t *intptr;
	bool is_signed = false;
	const char *typename = "integer";
	if (argtype == ARG_ADDRESS) {
		base = 16;
		intptr = &address;
		typename = "address";
	} else if (argtype == ARG_UINT) {
		intptr = &argument->uint;
	} else {
		intptr = (uintmax_t *)&argument->sint;
		is_signed = true;
	}
	enum strtoint_result sr = strtoint(s->arg, len, true, is_signed, base, intptr, &end);
	if (sr == STRTOINT_OVERFLOW) {
		ERROR_OPTION(s, "integer overflow: '%.*s'", len, s->arg);
		return false;
	} else if (sr == STRTOINT_NODIGITS) {
fail:
		ERROR_OPTION(s, "invalid %s: '%.*s'", typename, len, s->arg);
		return false;
	}
	// If we didn't process the whole current argument, and if either this is not the first
	// argument or we are parsing an address, fail.
	if (sr == STRTOINT_BADDIGIT && (s->argidx != 0 || argtype == ARG_ADDRESS)) {
		goto fail;
	}
	if (argtype == ARG_ADDRESS) {
		argument->address = address;
	}
	argument->type = argtype;
	s->arg = end;
	return true;
}

static bool
parse_int(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing integer");
		return false;
	}
	return parse_int_internal(s, s->argument, ARG_INT, -1);
}

static bool
parse_uint(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing integer");
		return false;
	}
	return parse_int_internal(s, s->argument, ARG_UINT, -1);
}

static bool
verify_width(struct state *s, size_t width) {
	if (width == 0 || width > sizeof(kword_t) || !ispow2(width)) {
		s->keep_error = true;
		ERROR_OPTION(s, "invalid width %zu", width);
		return false;
	}
	return true;
}

static bool
parse_width(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing width");
		return false;
	}
	if (!parse_int_internal(s, s->argument, ARG_UINT, -1)) {
		return false;
	}
	size_t width = s->argument->uint;
	if (!verify_width(s, width)) {
		return false;
	}
	s->argument->width = width;
	s->argument->type = ARG_WIDTH;
	return true;
}

static bool
parse_data(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing data");
		return false;
	}
	size_t size = 0;
	const char *end;
	enum strtodata_result sr = strtodata(s->arg, 16, NULL, &size, &end);
	if (sr == STRTODATA_BADBASE) {
		ERROR_OPTION(s, "bad base: '%s'", s->arg);
		return false;
	} else if (sr == STRTODATA_BADDIGIT) {
		ERROR_OPTION(s, "invalid digit '%c': '%s'", *end, s->arg);
		return false;
	} else if (sr == STRTODATA_NEEDDIGIT) {
		ERROR_OPTION(s, "incomplete final byte: '%s'", s->arg);
		return false;
	}
	uint8_t *data = malloc(size);
	sr = strtodata(s->arg, 16, data, &size, &end);
	assert(sr == STRTODATA_OK);
	s->argument->data.data = data;
	s->argument->data.length = size;
	s->argument->type = ARG_DATA;
	s->arg = end;
	assert(*s->arg == 0);
	return true;
}

static bool
parse_string(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing string");
		return false;
	}
	s->argument->string = s->arg;
	s->argument->type = ARG_STRING;
	s->arg += strlen(s->arg);
	assert(*s->arg == 0);
	return true;
}

static bool
parse_argv(struct state *s) {
	assert(s->option == NULL); // TODO: Allow argv in options.
	s->argument->argv = s->argv;
	s->argument->type = ARG_ARGV;
	s->argv += s->argc;
	s->argc  = 0;
	s->arg   = NULL;
	return true;
}

/*
 * extract_symbol
 *
 * Description:
 * 	Initialize a symbol from the given string.
 */
static bool
extract_symbol(struct argument *argument, const char *str, size_t len, bool force) {
	if (!force) {
		// We consider a string as a potential symbol if either it begins with "_" or
		// contains ":".
		if (str[0] != '_' && strnchr(str, len, ':') == NULL) {
			return false;
		}
	}
	char *sym = strndup(str, len);
	char *sep = strchr(sym, ':');
	if (sep == NULL) {
		argument->symbol.kext   = KERNEL_ID;
		argument->symbol.symbol = sym;
	} else if (sep == sym) {
		argument->symbol.kext   = NULL;
		argument->symbol.symbol = sym + 1;
	} else {
		*sep = 0;
		argument->symbol.kext   = sym;
		argument->symbol.symbol = sep + 1;
	}
	argument->type = ARG_SYMBOL;
	return true;
}

/*
 * free_symbol
 *
 * Description:
 * 	Free a symbol initialized with extract_symbol.
 */
static void
free_symbol(struct argument *argument) {
	assert(argument->type == ARG_SYMBOL);
	void *to_free;
	if (argument->symbol.kext == KERNEL_ID) {
		to_free = (void *)argument->symbol.symbol;
	} else if (argument->symbol.kext == NULL) {
		to_free = (void *)(argument->symbol.symbol - 1);
	} else {
		to_free = (void *)argument->symbol.kext;
	}
	free(to_free);
	argument->type = ARG_NONE;
}

static bool
parse_symbol(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing symbol");
		return false;
	}
	size_t len = strlen(s->arg);
	extract_symbol(s->argument, s->arg, len, true);
	s->arg += len;
	assert(*s->arg == 0);
	return true;
}

/*
 * resolve_symbol_address
 *
 * Description:
 * 	Resolve the symbol into an address.
 */
static bool
resolve_symbol_address(kaddr_t *address, const struct argsymbol *symbol) {
	if (!initialize(KERNEL_SYMBOLS)) {
		return false;
	}
	kext_result kr = resolve_symbol(symbol->kext, symbol->symbol, address, NULL);
	if (kr == KEXT_NOT_FOUND) {
		error_kext_symbol_not_found(symbol->kext, symbol->symbol);
	} else if (kr == KEXT_NO_KEXT) {
		error_kext_not_found(symbol->kext);
	}
	return (kr == KEXT_SUCCESS);
}

/*
 * parse_address_internal
 *
 * Description:
 * 	Parse an address. If the current argument looks like a symbol, it will be resolved into an
 * 	address.
 */
static bool
parse_address_internal(struct state *s, struct argument *argument, size_t len) {
	if (extract_symbol(argument, s->arg, len, false)) {
		kaddr_t address;
		bool found = resolve_symbol_address(&address, &argument->symbol);
		free_symbol(argument);
		if (!found) {
			ERROR_OPTION(s, "could not resolve symbol '%.*s' to address", len, s->arg);
			return false;
		}
		argument->address = address;
		argument->type = ARG_ADDRESS;
		s->arg += len;
		return true;
	}
	return parse_int_internal(s, argument, ARG_ADDRESS, len);
}

// TODO: Arithmetic.
static bool
parse_address(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing address");
		return false;
	}
	return parse_address_internal(s, s->argument, strlen(s->arg));
}

static bool
parse_range(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing address range");
		return false;
	}
	const char *arg_start = s->arg;
	struct argument address;
	// Parse the first component of the range.
	if (s->arg[0] == 0) {
		goto fail_1;
	} else if (s->arg[0] == '-') {
		s->argument->range.start         = 0;
		s->argument->range.default_start = true;
	} else {
		const char *end = strchr(s->arg, '-');
		if (end == NULL) {
			goto fail_1;
		}
		size_t len = end - s->arg;
		if (!parse_address_internal(s, &address, len)) {
			goto fail_1;
		}
		s->argument->range.start         = address.address;
		s->argument->range.default_start = false;
	}
	// Check that we have the separator.
	if (s->arg[0] != '-') {
		goto fail_1;
	}
	s->arg++;
	// Parse the second component of the range.
	if (s->arg[0] == 0) {
		s->argument->range.end         = (kaddr_t)(-1);
		s->argument->range.default_end = true;
	} else {
		size_t len = strlen(s->arg);
		if (!parse_address_internal(s, &address, len)) {
			goto fail_1;
		}
		if (s->arg[0] != 0) {
			goto fail_1;
		}
		s->argument->range.end         = address.address;
		s->argument->range.default_end = false;
	}
	// Check that the range is well-defined.
	if (s->argument->range.start > s->argument->range.end) {
		goto fail_2;
	}
	// Finish.
	s->argument->type = ARG_RANGE;
	assert(s->arg[0] == 0);
	return true;
fail_1:
	s->arg = arg_start;
	ERROR_OPTION(s, "invalid address range: '%s'", s->arg);
	return false;
fail_2:
	s->arg = arg_start;
	ERROR_OPTION(s, "bad address range "KADDR_XFMT"-"KADDR_XFMT,
			s->argument->range.start, s->argument->range.end);
	return false;
}

static bool
parse_word(struct state *s) {
	if (s->arg == NULL) {
		ERROR_OPTION(s, "missing word");
		return false;
	}
	size_t len = -1;
	const char *sep = strchr(s->arg, ':');
	if (sep != NULL) {
		len = sep - s->arg;
	}
	bool ok = parse_int_internal(s, s->argument, ARG_UINT, len);
	if (!ok) {
		return false;
	}
	kword_t word = s->argument->sint;
	size_t width = sizeof(kword_t);
	if (sep != NULL) {
		assert(s->arg == sep);
		s->arg++;
		ok = parse_int_internal(s, s->argument, ARG_UINT, -1);
		if (!ok) {
			return false;
		}
		width = s->argument->uint;
		ok = verify_width(s, width);
		if (!ok) {
			return false;
		}
	}
	s->argument->word.value = word;
	s->argument->word.width = width;
	s->argument->type = ARG_WORD;
	return true;
}

/*
 * cleanup_argument
 *
 * Description:
 * 	Free any resources allocated to an argument.
 */
static void
cleanup_argument(struct argument *argument) {
	switch (argument->type) {
		case ARG_DATA:
			free(argument->data.data);
			break;
		case ARG_SYMBOL:
			free_symbol(argument);
			break;
		default:
			break;
	}
}

/*
 * find_command
 *
 * Description:
 * 	Find which command best matches the given first argument. If no command matches, generates
 * 	a usage error. If help is indicated, prints a help message. Returns true if no errors were
 * 	generated.
 */
static bool
find_command(const char *str, const struct command **command) {
	if (strcmp(str, "?") == 0) {
		return help_all();
	}
	const struct command *c = cli.commands;
	const struct command *end = c + cli.command_count;
	const struct command *best = NULL;
	size_t match = 0;
	for (; c < end; c++) {
		size_t len = strlen(c->command);
		if (strncmp(c->command, str, len) == 0) {
			if (len > match) {
				best = c;
				match = len;
			}
		}
	}
	if (best == NULL) {
		ERROR_USAGE("unknown command '%s'", str);
		return false;
	}
	if (strcmp(str + match, "?") == 0) {
		return help_command(best);
	}
	*command = best;
	return true;
}

/*
 * init_arguments
 *
 * Description:
 * 	Initialize the arguments vector for the given command.
 */
static void
init_arguments(const struct command *command, struct argument *arguments) {
	memset(arguments, 0, command->argspecc * sizeof(*arguments));
	for (size_t i = 0; i < command->argspecc; i++) {
		arguments[i].option   = command->argspecv[i].option;
		arguments[i].argument = command->argspecv[i].argument;
		arguments[i].present  = false;
		arguments[i].type     = ARG_NONE;
	}
}

/*
 * option_count
 *
 * Description:
 * 	Get the number of options for a command.
 */
static size_t
option_count(const struct command *command) {
	size_t n = 0;
	while (n < command->argspecc && spec_is_option(&command->argspecv[n])) {
		n++;
	}
	return n;
}

/*
 * init_state
 *
 * Description:
 * 	Initialize the parsing state for processing the options.
 */
static void
init_state(struct state *s, const struct command *command, int argc, const char **argv,
		struct argument *arguments) {
	assert(argc > 0);
	s->arg        = argv[0] + strlen(command->command);
	s->argidx     = 0;
	s->argument   = NULL;
	s->option     = NULL;
	s->keep_error = false;
	s->bad_option = NULL;
	s->start      = 0;
	s->end        = option_count(command);
	s->command    = command;
	s->arguments  = arguments;
	s->argc       = argc;
	s->argv       = argv;
}

/*
 * reinit_state
 *
 * Description:
 * 	Reinitialize the state for processing the positional arguments.
 */
static void
reinit_state(struct state *s) {
	assert(s->command != NULL);
	s->option = NULL;
	s->start  = s->end;
	s->end    = s->command->argspecc;
}

/*
 * parse_option
 *
 * Description:
 * 	Try to parse an option. Returns false if there is an error.
 */
static bool
parse_option(struct state *s) {
	s->option = NULL;
	bool have_dash = false;
	if (advance(s)) {
		if (s->arg == NULL) {
			// We've processed all elements of argv. Nothing left to do.
			return true;
		}
		if (s->arg[0] != '-') {
			// We tried to read the next option, but it doesn't start with a dash, so
			// it's not an option.
			return true;
		}
		if (s->arg[1] == 0) {
			// The next "option" is "-", that is, just a dash. This is not a valid
			// option, so let the arguments handle it.
			return true;
		}
		s->arg++;
		have_dash = true;
	}
	// Check if this matches any option.
	const struct argspec *specs = s->command->argspecv;
	for (size_t i = s->start; i < s->end; i++) {
		size_t len = strlen(specs[i].option);
		if (strncmp(specs[i].option, s->arg, len) == 0) {
			s->option = &specs[i];
			s->argument = &s->arguments[i];
			s->arg += len;
			break;
		}
	}
	if (s->option == NULL) {
		if (have_dash) {
			// We have an option or argument, but we can't tell which. Save this arg as
			// a bad option, back out the dash, and then return true to start
			// processing the arguments.
			s->bad_option = s->arg;
			s->arg--;
			return true;
		}
		ERROR_COMMAND(s, "unrecognized option '%s'", s->arg);
		return false;
	}
	// If this is an unnamed option, do not try matching it again.
	if (s->option->option[0] == 0) {
		s->start++;
	}
	// Disallow duplicate options.
	if (s->argument->present) {
		ERROR_COMMAND(s, "option '%s' given multiple times", s->option->option);
		return false;
	}
	s->argument->present = true;
	// Create a save point in case we have to undo parsing an unnamed option.
	struct savepoint save;
	if (s->option->option[0] == 0) {
		state_save(s, &save);
	}
	// If we're at the end of the current arg string and we expect an argument, advance to the
	// next arg string. Otherwise, stay in the same place so that we can detect the transition
	// to arguments.
	if (*s->arg == 0 && s->option->type != ARG_NONE) {
		advance(s);
	}
	// Try to parse the option's argument.
	assert(s->option->type < sizeof(parse_fns) / sizeof(parse_fns[0]));
	if (!(parse_fns[s->option->type])(s)) {
		// If this is an unnamed option, clear the errors. We'll skip trying to match this
		// option from now on, but continue parsing options on the next iteration.
		if (s->option->option[0] == 0 && error_last()->type == &usage_error
				&& !s->keep_error) {
			error_clear();
			state_restore(s, &save);
			s->argument->present = false;
			return true;
		}
		s->keep_error = false;
		return false;
	}
	return true;
}

/*
 * parse_arguments
 *
 * Description:
 * 	Parse the positional arguments.
 *
 * Notes:
 * 	It is assumed that all required arguments come first, then optional arguments follow.
 */
static bool
parse_arguments(struct state *s) {
	for (size_t i = s->start; i < s->end; i++) {
		const struct argspec *spec = &s->command->argspecv[i];
		s->argument = &s->arguments[i];
		// Try to parse the argument.
		assert(spec->type < sizeof(parse_fns) / sizeof(parse_fns[0]));
		if (!(parse_fns[spec->type])(s)) {
			// If we previously had a bad option and it also failed argument parsing,
			// then print the bad option message.
			if (s->bad_option != NULL && error_last()->type == &usage_error) {
				error_clear();
				ERROR_COMMAND(s, "unrecognized option '%s'", s->arg);
				return false;
			}
			// If the issue was that no data was left and we've reached the optional
			// arguments, clear the error and stop processing.
			if (s->arg == NULL && spec->option == OPTIONAL &&
					error_last()->type == &usage_error) {
				error_clear();
				break;
			}
			return false;
		}
		s->argument->present = true;
		// We've processed any pending bad options successfully.
		s->bad_option = NULL;
		// We've finished parsing this argument, but we're still at the end of that arg
		// string. Advance to the next one.
		assert(s->arg == NULL || *s->arg == 0);
		advance(s);
	}
	// If there's any leftover data, emit an error.
	if (s->arg != NULL) {
		ERROR_COMMAND(s, "unexpected argument '%s'", s->arg);
		return false;
	}
	return true;
}

/*
 * parse_command
 *
 * Description:
 * 	Parse the argv for the given command into the given array of arguments.
 */
static bool
parse_command(const struct command *command, int argc, const char **argv, struct argument *arguments) {
	init_arguments(command, arguments);
	struct state s;
	init_state(&s, command, argc, argv, arguments);
	// Process all the options.
	do {
		if (!parse_option(&s)) {
			return false;
		}
	} while (s.option != NULL);
	// Process all the arguments.
	reinit_state(&s);
	if (!parse_arguments(&s)) {
		return false;
	}
	return true;
}

/*
 * cleanup_arguments
 *
 * Description:
 * 	Clean up the any resources allocated for the arguments array.
 */
static void
cleanup_arguments(const struct command *command, struct argument *arguments) {
	for (size_t i = 0; i < command->argspecc; i++) {
		cleanup_argument(&arguments[i]);
	}
}

/*
 * run_command
 *
 * Description:
 * 	Run the handler for the given command with the arguments.
 */
static bool
run_command(const struct command *command, struct argument *arguments) {
	return command->handler(arguments);
}

bool
command_print_help(const struct command *command) {
	if (command == NULL) {
		return help_all();
	} else {
		return help_command(command);
	}
}

bool
command_run_argv(int argc, const char *argv[]) {
	assert(argv[argc] == NULL);
	if (argc < 1) {
		return cli.default_action();
	}
	const struct command *c = NULL;
	if (!find_command(argv[0], &c)) {
		return false;
	}
	if (c == NULL) {
		return true;
	}
	struct argument arguments[c->argspecc];
	bool success = false;
	if (!parse_command(c, argc, argv, arguments)) {
		goto fail;
	}
	success = run_command(c, arguments);
fail:
	cleanup_arguments(c, arguments);
	return success;
}
