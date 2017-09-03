#ifndef MEMCTL__AARCH64__FINDER__KAUTH_CRED_SETSVUIDGID_H_
#define MEMCTL__AARCH64__FINDER__KAUTH_CRED_SETSVUIDGID_H_

#include "memctl/kernel.h"

/*
 * kernel_find_kauth_cred_setsvuidgid
 *
 * Description:
 * 	A special symbol finder for _kauth_cred_setsvuidgid.
 */
void kernel_find_kauth_cred_setsvuidgid(struct kext *kernel);

#endif
