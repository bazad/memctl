#include "initialize.h"

#include "error.h"
#include "memory.h"

#include "memctl/core.h"
#include "memctl/kernel.h"
#include "memctl/kernel_call.h"
#include "memctl/kernel_memory.h"
#include "memctl/kernel_slide.h"
#include "memctl/memctl_signal.h"
#include "memctl/platform.h"
#include "memctl/process.h"
#include "memctl/symbol_finders.h"

/*
 * loaded_features
 *
 * Description:
 * 	The libmemctl or memctl features that have already been loaded.
 */
static feature_t loaded_features;

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
static void
install_signal_handler() {
	const int signals[] = {
		SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGEMT, SIGFPE, SIGBUS,
		SIGSEGV, SIGSYS, SIGPIPE, SIGALRM, SIGTERM, SIGXCPU, SIGXFSZ, SIGVTALRM, SIGPROF,
		SIGUSR1, SIGUSR2,
	};
	struct sigaction act = { .sa_handler = signal_handler };
	for (size_t i = 0; i < sizeof(signals) / sizeof(*signals); i++) {
		act.sa_flags = (signals[i] == SIGINT ? 0 : SA_RESETHAND);
		int err = sigaction(signals[i], &act, NULL);
		if (err < 0) {
			memctl_warning("could not install signal handler for signal %d",
					signals[i]);
		}
	}
}

/*
 * initialize_environment
 *
 * Description:
 * 	Initialize memctl state that is controlled by environment variables.
 */
static void
initialize_environment() {
	const char *env_safe_memory = getenv("MEMCTL_SAFE_MEMORY");
	if (env_safe_memory != NULL && strtol(env_safe_memory, NULL, 10) > 0) {
		memctl_warning("using safe memory functions; "
		               "some functionality may be unavailable");
		safe_memory = true;
	}
}

void
default_initialize() {
	error_init();
	platform_init();
	install_signal_handler();
	initialize_environment();
}

// Test if all bits set in x are also set in y.
#define ALL_SET(x,y)	(((x) & (y)) == (x))

bool
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
		kernel_memory_init();
		if (kernel_read_unsafe == NULL) {
			error_message("could not initialize unsafe kernel memory functions");
			return false;
		}
		LOADED(KERNEL_MEMORY_BASIC);
	}
	if (NEED(KERNEL_IMAGE)) {
		kernel_symbol_finders_init();
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
		// Re-initialize the kernel image to set kernel.slide.
		kernel_init(NULL);
		LOADED(KERNEL_SLIDE);
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
		kernel_memory_init();
		if (kernel_read_all == NULL || kernel_virtual_to_physical == NULL) {
			error_message("could not initialize safe kernel memory functions");
			return false;
		}
		if (physical_read_unsafe == NULL || physical_write_unsafe == NULL) {
			error_message("could not initialize unsafe physical memory functions");
			return false;
		}
		LOADED(KERNEL_MEMORY);
	}
	if (NEED(PROCESS)) {
		process_init();
		LOADED(PROCESS);
	}
	assert(ALL_SET(features, loaded_features));
	return true;
#undef NEED
#undef LOADED
}

void
deinitialize(bool critical) {
#define LOADED(feature)	(ALL_SET(feature, loaded_features))
	if (LOADED(KERNEL_CALL)) {
		kernel_call_deinit();
	}
	if (!critical && LOADED(KERNEL_IMAGE)) {
		kernel_deinit();
	}
	if (!critical) {
		error_free();
	}
	if (critical) {
		fprintf(stderr, "deinitialized\n");
	}
	loaded_features = 0;
#undef LOADED
}
