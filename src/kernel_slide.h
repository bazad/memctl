#ifndef MEMCTL__KERNEL_SLIDE_H_
#define MEMCTL__KERNEL_SLIDE_H_

#include "memctl_types.h"

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
 */
bool kernel_slide_init(void);

#endif
