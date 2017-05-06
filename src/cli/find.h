#ifndef MEMCTL__CLI__FIND_H_
#define MEMCTL__CLI__FIND_H_

#include "memctl_types.h"

/*
 * TODO
 */
bool memctl_find(kaddr_t start, kaddr_t end, kword_t value, size_t width, bool physical, bool heap,
		size_t access, size_t alignment);

#endif
