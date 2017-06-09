#ifndef MEMCTL__OFFSET_H_
#define MEMCTL__OFFSET_H_

#include "memctl/memctl_types.h"

#include <assert.h>

/*
 * struct offset
 *
 * Description:
 * 	A structure representing an offset or address.
 */
struct offset {
	// The offset of the object relative to some base.
	kword_t offset;
	// A validity count. 0 means unknown, negative means unusable.
	int valid;
};

/*
 * macro OFFSET
 *
 * Description:
 * 	A convenience macro for generating a name for an offset.
 */
#define OFFSET(base, object)	(base##__offset__##object)

/*
 * macro OFFSETOF
 *
 * Description:
 * 	A macro for accessing an offset. Raises an assertion error if the offset is not valid.
 */
#define OFFSETOF(base, object)					\
	({ assert(OFFSET(base, object).valid > 0);		\
	   OFFSET(base, object).offset; })

/*
 * macro ADDRESS
 *
 * Description:
 * 	A convenience macro for generating a name for an address offset.
 */
#define ADDRESS(kext, object)	(kext##__address__##object)

/*
 * macro ADDRESSOF
 *
 * Description:
 * 	A macro for accessing an address offset. Raises an assertion error if the offset is not
 * 	valid.
 */
#define ADDRESSOF(kext, object)					\
	({ assert(ADDRESS(kext, object).valid > 0);		\
	   ADDRESS(kext, object).offset; })

/*
 * macro DECLARE_OFFSET
 *
 * Description:
 * 	Declare an offset object.
 */
#define DECLARE_OFFSET(base, object)	\
extern struct offset OFFSET(base, object)

/*
 * macro DEFINE_OFFSET
 *
 * Description:
 * 	Define the offset object.
 */
#define DEFINE_OFFSET(base, object)	\
struct offset OFFSET(base, object)

/*
 * macro DECLARE_ADDRESS
 *
 * Description:
 * 	Declare the offset object for an address.
 */
#define DECLARE_ADDRESS(kext, object)	\
extern struct offset ADDRESS(kext, object)

/*
 * macro DEFINE_ADDRESS
 *
 * Description:
 * 	Define the offset object for an address.
 */
#define DEFINE_ADDRESS(kext, object)	\
struct offset ADDRESS(kext, object)

#endif
