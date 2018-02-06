#ifndef MEMCTL__DIAGNOSTIC_H_
#define MEMCTL__DIAGNOSTIC_H_

/*
 * memctl_diagnostic
 *
 * Description:
 * 	During development it's often helpful to get a sense as to what's happening inside of
 * 	memctl. Diagnostics can be used to print diagnostic information to stderr in development
 * 	builds.
 *
 * Parameters:
 * 	level				The diagnostic level, with 1 being the most severe.
 * 	format				A printf-style format string containing the message.
 * 	...				Arguments for the printf-style format string.
 *
 * Notes:
 * 	The MEMCTL_DIAGNOSTIC preprocessor definition controls which diagnostics are compiled into
 * 	the binary. Diagnostics at a higher level than MEMCTL_DIAGNOSTIC will be omitted. Setting
 * 	MEMCTL_DIAGNOSTIC to 0 will disable all diagnostics.
 */

#if MEMCTL_DIAGNOSTIC

#define memctl_diagnostic(_level, _format, ...)						\
	do {										\
		if (_level <= MEMCTL_DIAGNOSTIC) {					\
			memctl_issue_diagnostic(__func__, _format, ##__VA_ARGS__);	\
		}									\
	} while (0)

void memctl_issue_diagnostic(const char *function, const char *format, ...);

#else // MEMCTL_DIAGNOSTIC

#define memctl_diagnostic(_level, _format, ...)		do { } while (0)

#endif // MEMCTL_DIAGNOSTIC

#endif
