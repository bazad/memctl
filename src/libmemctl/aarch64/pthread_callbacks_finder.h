#ifndef MEMCTL__AARCH64__PTHREAD_CALLBACKS_FINDER_H_
#define MEMCTL__AARCH64__PTHREAD_CALLBACKS_FINDER_H_

/*
 * kernel_symbol_finder_init_pthread_callbacks
 *
 * Description:
 * 	Add a kernel symbol finder for the pthread callbacks supplied by pthread_kext_register in
 * 	pthread_shims.c.
 */
void kernel_symbol_finder_init_pthread_callbacks(void);

#endif
