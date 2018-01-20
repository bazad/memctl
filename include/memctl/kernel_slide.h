#ifndef MEMCTL__KERNEL_SLIDE_H_
#define MEMCTL__KERNEL_SLIDE_H_

#include "memctl/memctl_types.h"

/*
 * kernel_slide
 *
 * Description:
 * 	The kASLR slide.
 */
extern kword_t kernel_slide;

/*
 * kernel_slide_init
 *
 * Description:
 * 	Find the kernel slide.
 *
 * Dependencies:
 * 	kernel_task
 * 	kernel
 *
 * Notes:
 * 	This function may employ one of a number of different methods of finding the kernel slide,
 * 	depending on the platform and version. If a safe method is not available, unsafe methods
 * 	will be used. Unsafe kernel slide detection methods may panic the system.
 */
bool kernel_slide_init(void);

#endif
