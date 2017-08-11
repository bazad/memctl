#include "memctl/symbol_finders.h"

#if __arm64__
#include "aarch64/finder/kauth_cred_setsvuidgid.h"
#include "aarch64/finder/pthread_callbacks.h"
#include "aarch64/finder/zone_element_size.h"
#endif

void
kernel_symbol_finders_init() {
#if __arm64__
	kernel_symbol_finder_init_kauth_cred_setsvuidgid();
	kernel_symbol_finder_init_pthread_callbacks();
	kernel_symbol_finder_init_zone_element_size();
#endif
}
