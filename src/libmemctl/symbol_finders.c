#include "memctl/symbol_finders.h"

#if __arm64__
#include "aarch64/pthread_callbacks_finder.h"
#include "aarch64/zone_element_size_finder.h"
#endif

void
kernel_symbol_finders_init() {
#if __arm64__
	kernel_symbol_finder_init_pthread_callbacks();
	kernel_symbol_finder_init_zone_element_size();
#endif
}
