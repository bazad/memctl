#ifndef MEMCTL__ARM64__FINDER__PTHREAD_CALLBACKS_H_
#define MEMCTL__ARM64__FINDER__PTHREAD_CALLBACKS_H_

#include "memctl/kernel.h"

/*
 * kernel_find_pthread_callbacks
 *
 * Description:
 * 	A special symbol finder for the _pthread_callbacks structure and some of its function
 * 	pointers. _pthread_callbacks is defined in pthread_shims.c in XNU.
 */
void kernel_find_pthread_callbacks(struct kext *kernel);

#endif
