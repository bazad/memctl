#include "diagnostic.h"

#if MEMCTL_DIAGNOSTIC

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void
memctl_issue_diagnostic(const char *function, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	char *msg = NULL;
	vasprintf(&msg, format, ap);
	assert(msg != NULL);
	fprintf(stderr, "DIAGNOSTIC: %s: %s\n", function, msg);
	free(msg);
	va_end(ap);
}

#else // MEMCTL_DIAGNOSTIC

// To avoid: "error: ISO C requires a translation unit to contain at least one declaration"
typedef int memctl_diagnostic;

#endif // MEMCTL_DIAGNOSTIC
