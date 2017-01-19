#include "cli/vmmap.h"

#include "cli/error.h"

#include "core.h"
#include "memctl_signal.h"

#include <mach/mach_vm.h>
#include <stdio.h>

/*
 * share_mode_name
 *
 * Description:
 * 	Get the name of the given share mode. The returned string is always 3 characters.
 */
static const char *
share_mode_name(unsigned char share_mode) {
	switch (share_mode) {
		case SM_COW:                    return "COW";
		case SM_PRIVATE:                return "PRV";
		case SM_EMPTY:                  return "NUL";
		case SM_SHARED:                 return "ALI";
		case SM_TRUESHARED:             return "SHR";
		case SM_PRIVATE_ALIASED:        return "P/A";
		case SM_SHARED_ALIASED:         return "S/A";
		case SM_LARGE_PAGE:             return "LPG";
		default:                        return "???";
	}
}

/*
 * format_display_size
 *
 * Description:
 * 	Format the given size in bytes as a short display size. The resulting string is
 * 	guaranteed to be no more than 4 characters. buf must be at least 5 bytes.
 */
static void
format_display_size(char *buf, uint64_t size) {
	const char scale[] = { 'K', 'M', 'G', 'T', 'P', 'E' };
	float display_size = size / 1024.0;
	unsigned scale_index = 0;
	while (display_size >= 1000.0) {
		display_size /= 1024;
		scale_index++;
	}
	int precision = 0;
	if (display_size < 10.0 && display_size - (float)((int)display_size) > 0) {
		precision = 1;
	}
	snprintf(buf, 5, "%.*f%c", precision, display_size, scale[scale_index]);
}

/*
 * format_memory_protection
 *
 * Description:
 * 	Format the given memory protection in the form "rwx", where non-present permissions are
 * 	replaced with "-". buf must be at least 4 bytes.
 */
static void
format_memory_protection(char *buf, int prot) {
	snprintf(buf, 4, "%c%c%c",
			(prot & VM_PROT_READ    ? 'r' : '-'),
			(prot & VM_PROT_WRITE   ? 'w' : '-'),
			(prot & VM_PROT_EXECUTE ? 'x' : '-'));
}

bool
memctl_vmmap(kaddr_t kaddr, kaddr_t end, uint32_t depth) {
	for (bool first = true;; first = false) {
		mach_vm_address_t address = kaddr;
		mach_vm_size_t size = 0;
		uint32_t depth0 = depth;
		vm_region_submap_info_data_64_t info;
		mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
		kern_return_t kr = mach_vm_region_recurse(kernel_task, &address, &size,
				&depth0, (vm_region_recurse_info_t)&info, &count);
		if (interrupted) {
			error_interrupt();
			return false;
		}
		if (kr != KERN_SUCCESS || address > end) {
			if (first) {
				if (kaddr == end) {
					printf("no virtual memory region contains address %p\n",
							(void *)kaddr);
				} else {
					printf("no virtual memory region intersects %p-%p\n",
							(void *)kaddr, (void *)end);
				}
			}
			break;
		}
		if (first) {
			printf("          START - END             [ VSIZE ] "
					"PRT/MAX SHRMOD DEPTH RESIDENT REFCNT TAG\n");
		}
		char vsize[5];
		format_display_size(vsize, size);
		char cur_prot[4];
		format_memory_protection(cur_prot, info.protection);
		char max_prot[4];
		format_memory_protection(max_prot, info.max_protection);
		printf("%016llx-%016llx [ %5s ] %s/%s %6s %5u %8u %6u %3u\n",
				address, address + size,
				vsize,
				cur_prot, max_prot,
				share_mode_name(info.share_mode),
				depth0,
				info.pages_resident,
				info.ref_count,
				info.user_tag);
		kaddr = address + size;
	}
	return true;
}
