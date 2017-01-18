#include "cli/command.h"

#include "cli/error.h"

#include "kernel.h"
#include "utility.h"

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
	// The current argument being populated.
	struct argument *argument;
	// The current (or most recently processed) option, or NULL if options are not currently
	// being processed.
	const struct argspec *option;
	// When processing nameless options, the error message for a failed match will be
	// discarded. The keep_error flag signals that the match succeeded enough that the error
	// message should still be displayed and the command aborted.
	bool keep_error;
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
 * state_save
 *
 * Description:
 * 	Save the current parsing position in the given savepoint.
 */
static void
state_save(const struct state *s, struct savepoint *save) {
	save->arg  = s->arg;
	save->argc = s->argc;
	save->argv = s->argv;
}

/*
 * state_restore
 *
 * Description:
 * 	Restore the current parsing position to the given savepoint.
 */
static void
state_restore(struct state *s, const struct savepoint *save) {
	s->arg  = save->arg;
	s->argc = save->argc;
	s->argv = save->argv;
}

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

/*
 * advance
 *
 * Description:
 * 	Advance arg to the next argv if the current argv has no more data. Returns true if the
 * 	state has been advanced to a new argv or if there is no more data available.
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
		assert(s->argc > 0 || s->arg == NULL);
		return true;
	}
	return false;
}

/*
 * require
 *
 * Description:
 * 	Require that argument data be available in arg. If no more data is available, return false.
 *
 * Notes:
 * 	This function only advances at most one argument. If arg[0] == 0 after calling require,
 * 	this means that the state was at the end of an argument and require caused it to advance to
 * 	the next one, but the new argument is an empty string.
 *
 * 	This function is not idempotent.
 */
static bool
require(struct state *s) {
	if (advance(s)) {
		return (s->arg != NULL);
	}
	return true;
}

static bool
parse_none(struct state *s) {
	// s->argument->type is already ARG_NONE.
	return true;
}

static bool
parse_int(struct state *s) {
	int argc = s->argc;
	if (!require(s)) {
		ERROR_OPTION(s, "missing integer");
		return false;
	}
	char *end;
	s->argument->sint = strtoimax(s->arg, &end, 0);
	if (end == s->arg) {
		goto fail;
	}
	// If we are not processing options, or if we are but changed arguments to get from the end
	// of the option to the beginning of its argument, then we must process the whole of the
	// current argument.
	if ((s->option == NULL || argc != s->argc) && *end != 0) {
		goto fail;
	}
	s->argument->type = ARG_INT;
	s->arg = end;
	return true;
fail:
	ERROR_OPTION(s, "not a valid integer: '%s'", s->arg);
	return false;
}

static bool
parse_uint_internal(struct state *s, int argc) {
	char *end;
	s->argument->uint = strtoumax(s->arg, &end, 0);
	if (end == s->arg) {
		goto fail;
	}
	// If we are not processing options, or if we are but changed arguments to get from the end
	// of the option to the beginning of its argument, then we must process the whole of the
	// current argument.
	if ((s->option == NULL || argc != s->argc) && *end != 0) {
		goto fail;
	}
	s->argument->type = ARG_UINT;
	s->arg = end;
	return true;
fail:
	ERROR_OPTION(s, "not a valid integer: '%s'", s->arg);
	return false;
}

static bool
parse_uint(struct state *s) {
	int argc = s->argc;
	if (!require(s)) {
		ERROR_OPTION(s, "missing integer");
		return false;
	}
	return parse_uint_internal(s, argc);
}

static bool
parse_width(struct state *s) {
	int argc = s->argc;
	if (!require(s)) {
		ERROR_OPTION(s, "missing width");
		return false;
	}
	if (!parse_uint_internal(s, argc)) {
		return false;
	}
	size_t width = s->argument->uint;
	if (width == 0 || width > sizeof(kword_t) || !ispow2(width)) {
		s->keep_error = true;
		ERROR_OPTION(s, "invalid width %zd", width);
		return false;
	}
	s->argument->width = width;
	s->argument->type = ARG_WIDTH;
	return true;
}

/*
 * hex_digit
 *
 * Description:
 * 	Convert the given hexadecimal digit into its numeric value, returning -1 if the character
 * 	is not valid hexadecimal.
 */
static int
hex_digit(int c) {
	if ('0' <= c && c <= '9') {
		return c - '0';
	} else if ('A' <= c && c <= 'F') {
		return c - 'A' + 0xa;
	} else if ('a' <= c && c <= 'f') {
		return c - 'a' + 0xa;
	} else {
		return -1;
	}
}

static bool
parse_data(struct state *s) {
	if (!require(s)) {
		ERROR_OPTION(s, "missing data");
		return false;
	}
	const char *str = s->arg;
	if (strcmp(str, "0x") == 0) {
		str += 2;
	}
	size_t size = strlen(str);
	if (size & 1) {
		ERROR_OPTION(s, "odd-length hex data");
		return false;
	}
	size /= 2;
	uint8_t *data = malloc(size);
	if (data == NULL) {
		error_out_of_memory();
		return false;
	}
	for (size_t i = 0; i < size; i++, str += 2) {
		int h1 = hex_digit(str[0]);
		int h2 = hex_digit(str[1]);
		if (h1 < 0 || h2 < 0) {
			ERROR_OPTION(s, "invalid hexadecimal digit '%c'", (h1 < 0 ? str[0] : str[1]));
			return false;
		}
		data[i] = (h1 << 4) | h2;
	}
	s->argument->data.data = data;
	s->argument->data.length = size;
	s->argument->type = ARG_DATA;
	s->arg = str;
	assert(*s->arg == 0);
	return true;
}

static bool
parse_string(struct state *s) {
	if (!require(s)) {
		ERROR_OPTION(s, "missing string");
		return false;
	}
	s->argument->string = s->arg;
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

static bool
parse_symbol(struct state *s) {
	if (!require(s)) {
		ERROR_OPTION(s, "missing symbol");
		return false;
	}
	char *str = strdup(s->arg);
	char *sep = strchr(s->arg, ':');
	if (sep == NULL) {
		s->argument->symbol.kext = KERNEL_ID;
		s->argument->symbol.symbol = str;
	} else if (sep == str) {
		s->argument->symbol.kext = NULL;
		s->argument->symbol.symbol = str + 1;
	} else {
		*sep = 0;
		s->argument->symbol.kext = str;
		s->argument->symbol.symbol = sep + 1;
	}
	s->argument->type = ARG_SYMBOL;
	s->arg += strlen(s->arg);
	assert(*s->arg == 0);
	return true;
}

// TODO: Resolve symbols + arithmetic.
static bool
parse_address(struct state *s) {
	if (!require(s)) {
		ERROR_OPTION(s, "missing address");
		return false;
	}
	if (s->arg[0] == 0 || s->arg[0] == '-') {
		goto fail;
	}
	char *end;
	s->argument->address = strtoumax(s->arg, &end, 16);
	if (*end != 0) {
		goto fail;
	}
	s->argument->type = ARG_ADDRESS;
	s->arg = end;
	return true;
fail:
	ERROR_OPTION(s, "invalid address: '%s'", s->arg);
	return false;
}

static bool
parse_range(struct state *s) {
	if (!require(s)) {
		ERROR_OPTION(s, "missing address range");
		return false;
	}
	const char *str = s->arg;
	char *end = (char *)str;
	if (str[0] == 0) {
		goto fail1;
	} else if (str[0] == '-') {
		s->argument->range.start = 0;
	} else {
		s->argument->range.start = strtoumax(str, &end, 16);
		if (*end != '-') {
			goto fail1;
		}
	}
	str = end + 1;
	if (str[0] == '-') {
		goto fail2;
	} else if (str[0] == 0) {
		s->argument->range.end = (kaddr_t)(-1);
	} else {
		s->argument->range.end = strtoumax(str, &end, 16);
		if (*end != 0) {
			goto fail2;
		}
	}
	s->argument->type = ARG_RANGE;
	s->arg = end;
fail1:
	ERROR_OPTION(s, "invalid address range: '%s'", s->arg);
	return false;
fail2:
	ERROR_OPTION(s, "invalid address: '%s'", str);
	return false;
}

/*
 * cleanup_argument
 *
 * Description:
 * 	Free any resources allocated to an argument.
 */
static void
cleanup_argument(struct argument *argument) {
	void *to_free;
	switch (argument->type) {
		case ARG_DATA:
			free(argument->data.data);
			break;
		case ARG_SYMBOL:
			if (argument->symbol.kext == KERNEL_ID) {
				to_free = (void *)argument->symbol.symbol;
			} else if (argument->symbol.kext == NULL) {
				to_free = (void *)(argument->symbol.symbol - 1);
			} else {
				to_free = (void *)argument->symbol.kext;
			}
			free(to_free);
		default:
			break;
	}
}

/*
 * find_command
 *
 * Description:
 * 	Find which command best matches the given first argument.
 */
static const struct command *
find_command(const char *cmd) {
	const struct command *c = cli.commands;
	const struct command *end = c + cli.command_count;
	const struct command *best = NULL;
	size_t match = 0;
	for (; c < end; c++) {
		size_t len = strlen(c->command);
		if (strncmp(c->command, cmd, len) == 0) {
			if (len > match) {
				best = c;
				match = len;
			}
		}
	}
	return best;
}

/*
 * init_arguments
 *
 * Description:
 * 	Initialize the arguments vector for the given command.
 */
static void
init_arguments(const struct command *command, struct argument *arguments) {
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
	s->argument   = NULL;
	s->option     = NULL;
	s->keep_error = false;
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
		s->arg++;
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
	// Try to parse the option's argument.
	assert(s->option->type < sizeof(parse_fns) / sizeof(parse_fns[0]));
	if (!(parse_fns[s->option->type])(s)) {
		// If this is an unnamed option, clear the errors. We'll skip trying to match this
		// option from now on.
		if (s->option->option[0] == 0 && error_last()->type == usage_error
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
			// If the issue was that no data was left and we've reached the optional
			// arguments, clear the error and stop processing.
			if (s->arg == NULL && spec->option == OPTIONAL &&
					error_last()->type == usage_error) {
				error_clear();
				break;
			}
			return false;
		}
		s->argument->present = true;
	}
	// If there's any leftover data, emit an error.
	if (require(s)) {
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
command_print_help(const char *command) {
	error_internal("command_print_help");//TODO
	return false;
}

bool
command_run_argv(int argc, const char *argv[]) {
	assert(argv[argc] == NULL);
	if (argc < 1) {
		return cli.default_action();
	}
	const struct command *c = find_command(argv[0]);
	if (c == NULL) {
		ERROR_USAGE("unknown command '%s'", argv[0]);
		return false;
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
