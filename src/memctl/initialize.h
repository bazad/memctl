#ifndef MEMCTL_CLI__INITIALIZE_H_
#define MEMCTL_CLI__INITIALIZE_H_

#include "memctl/memctl_types.h"

/*
 * feature_t
 *
 * Description:
 * 	Flags for libmemctl and memctl features.
 */
typedef enum {
	KERNEL_TASK         = 0x01,
	KERNEL_MEMORY_BASIC = 0x02 | KERNEL_TASK,
	KERNEL_IMAGE        = 0x04,
	KERNEL_SLIDE        = 0x08 | KERNEL_TASK | KERNEL_IMAGE,
	KERNEL_SYMBOLS      = KERNEL_IMAGE | KERNEL_SLIDE,
	KERNEL_CALL         = 0x10 | KERNEL_MEMORY_BASIC | KERNEL_SYMBOLS,
	KERNEL_MEMORY       = 0x20 | KERNEL_CALL,
	PROCESS             = 0x40 | KERNEL_CALL | KERNEL_SYMBOLS,
	PRIVESC             = PROCESS | KERNEL_MEMORY_BASIC,
	CLASS               = 0x80 | KERNEL_CALL | KERNEL_SYMBOLS,
} feature_t;

/*
 * default_initialize
 *
 * Description:
 * 	Initialize the basic set of features that should always be present.
 */
void default_initialize(void);

/*
 * initialize
 *
 * Description:
 * 	Initialize the required functionality from libmemctl or memctl.
 */
bool initialize(feature_t features);

/*
 * deinitialize
 *
 * Description:
 * 	Unload loaded features.
 *
 * Parameters:
 * 		critical		If true, only system-critical features will be unloaded.
 * 					Otherwise, all features will be unloaded.
 *
 * Notes:
 * 	After this call, loaded_features is reset to 0, even if some features are not unloaded.
 */
void deinitialize(bool critical);

#endif
