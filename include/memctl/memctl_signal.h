#ifndef MEMCTL__MEMCTL_SIGNAL_H_
#define MEMCTL__MEMCTL_SIGNAL_H_

#include <signal.h>

/*
 * interrupted
 *
 * Description:
 * 	A flag indicating that this process has received an interrupt signal. Set this to 1 to
 * 	cause libmemctl to exit any long running loops with an error.
 */
extern volatile sig_atomic_t interrupted;

#endif
