#ifndef MEMCTL_CLI__COMMAND_H_
#define MEMCTL_CLI__COMMAND_H_

#include "memctl/memctl_types.h"

#include <assert.h>

/*
 * ARGUMENT
 *
 * Description:
 * 	Indicates that this is a required argument.
 */
#define ARGUMENT ((const char *)(1))

/*
 * OPTIONAL
 *
 * Description:
 * 	Indicates that this is an optional argument.
 */
#define OPTIONAL ((const char *)(2))

/*
 * argtype
 *
 * Description:
 * 	An enumeration for the types of arguments recognized by the command processing system.
 */
typedef enum argtype {
	ARG_NONE,
	ARG_INT,
	ARG_UINT,
	ARG_WIDTH,
	ARG_DATA,
	ARG_STRING,
	ARG_ARGV,
	ARG_SYMBOL,
	ARG_ADDRESS,
	ARG_RANGE,
	ARG_WORD,
	ARG_WORDS,
} argtype;

/*
 * struct argument
 *
 * Description:
 * 	An argument or option parsed from a command.
 */
struct argument {
	// The name of the option, or ARGUMENT or OPTIONAL if this is a positional argument.
	const char *option;
	// The name of the argument.
	const char *argument;
	// Whether this option or argument was supplied.
	bool present;
	// The argument type.
	argtype type;
	// The argument data. Use type to determine which field to access.
	union {
		intptr_t sint;
		uintptr_t uint;
		size_t width;
		struct argdata {
			void *data;
			size_t length;
		} data;
		const char *string;
		const char **argv;
		struct argsymbol {
			const char *kext;
			const char *symbol;
		} symbol;
		kaddr_t address;
		struct argrange {
			kaddr_t start;
			kaddr_t end;
			bool    default_start;
			bool    default_end;
		} range;
		struct argword {
			size_t width;
			kword_t value;
		} word;
		struct argwords {
			size_t count;
			struct argword *words;
		} words;
	};
};

/*
 * struct argspec
 *
 * Description:
 * 	An argument or option specification.
 */
struct argspec {
	// The name of the option, or ARGUMENT or OPTIONAL if this is a positional argument.
	const char *option;
	// The name of the argument.
	const char *argument;
	// The type of the argument.
	argtype type;
	// A description of this option or argument.
	const char *description;
};

/*
 * handler_fn
 *
 * Description:
 * 	The type of a command handler.
 *
 * Parameters:
 * 		arguments		An array of options and arguments, in the exact same layout
 * 					as argspecv in the corresponding command structure.
 *
 * Returns:
 * 	A bool indicating whether the command executed successfully. This value is returned by
 * 	command_run_argv.
 */
typedef bool (*handler_fn)(const struct argument *arguments);

/*
 * struct command
 *
 * Description:
 * 	A structure describing a command.
 */
struct command {
	// The command string.
	const char *command;
	// The parent command.
	const char *parent;
	// A handler for this command.
	handler_fn handler;
	// A description of this command.
	const char *short_description;
	// A longer description of this command.
	const char *long_description;
	// The number of elements in the argspecv array.
	size_t argspecc;
	// An array of argspec structures describing the options and arguments.
	struct argspec *argspecv;
};

/*
 * default_action_fn
 *
 * Description:
 * 	The type of a default action function, to be executed when no arguments are given at all.
 */
typedef bool (*default_action_fn)(void);

/*
 * struct cli
 *
 * Description:
 * 	A specification of this program's command line interface.
 */
struct cli {
	// The default action if no arguments are given.
	default_action_fn default_action;
	// The number of commands.
	size_t command_count;
	// An array of commands.
	struct command *commands;
};

/*
 * cli
 *
 * Description:
 * 	The cli structure for this program.
 */
extern struct cli cli;

/*
 * command_print_help
 *
 * Description:
 * 	Print a help message, either for the specified command or for this utility.
 *
 * Parameters
 * 		command			The command to print specific help for. If NULL, then a
 * 					generic help message is printed.
 */
bool command_print_help(const struct command *command);

/*
 * command_run_argv
 *
 * Description:
 * 	Parses the argument vector and runs the appropriate command.
 *
 * Parameters:
 * 		argc			The number of elements in argv.
 * 		argv			The argument vector. The first element should be the
 * 					command. Must be NULL terminated.
 *
 * Returns:
 * 	True if the command ran successfully.
 */
bool command_run_argv(int argc, const char *argv[]);

#endif
