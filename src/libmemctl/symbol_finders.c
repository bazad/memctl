#include "memctl/symbol_finders.h"

#include "memctl/memctl_error.h"

#if __arm64__
#include "aarch64/finder/kauth_cred_setsvuidgid.h"
#include "aarch64/finder/pmap_cache_attributes.h"
#include "aarch64/finder/pthread_callbacks.h"
#include "aarch64/finder/vtables.h"
#include "aarch64/finder/zone_element_size.h"
#endif

void
kernel_symbol_finders_init() {
	error_stop();
#if __arm64__
	kext_add_symbol_finder(KERNEL_ID, kernel_find_kauth_cred_setsvuidgid);
	kext_add_symbol_finder(KERNEL_ID, kernel_find_pmap_cache_attributes);
	kext_add_symbol_finder(KERNEL_ID, kernel_find_pthread_callbacks);
	kext_add_symbol_finder(KERNEL_ID, kernel_find_zone_element_size);
	kext_add_symbol_finder(NULL,      kext_find_vtables);
#endif
	error_start();
}
