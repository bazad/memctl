#include "oskext.h"

#include "core.h"
#include "kernel.h"
#include "kernel_slide.h"
#include "memctl_common.h"
#include "memctl_error.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <mach-o/arch.h>

/* ---- START OSKext API ---- */

typedef struct __OSKext *OSKextRef;

static const CFStringRef kCFBundleLoadAddressKey = CFSTR("OSBundleLoadAddress");
static const CFStringRef kCFBundleLoadSizeKey    = CFSTR("OSBundleLoadSize");

extern CFDictionaryRef
OSKextCopyLoadedKextInfo(
		CFArrayRef kextIdentifiers,
		CFArrayRef infoKeys);

extern OSKextRef
OSKextCreateWithIdentifier(
		CFAllocatorRef allocator,
		CFStringRef    kextIdentifier);

extern CFDataRef
OSKextCopyExecutableForArchitecture(
		OSKextRef         kext,
		const NXArchInfo *arch);

extern const NXArchInfo *
OSKextGetRunningKernelArchitecture(void);

/* ---- END OSKext API ---- */

/*
 * cfstring_nocopy
 *
 * Description:
 * 	Call CFStringCreateWithCStringNoCopy with the default arguments.
 */
static CFStringRef
cfstring_nocopy(const char *str) {
	return CFStringCreateWithCStringNoCopy(
			kCFAllocatorDefault, str,
			kCFStringEncodingUTF8, kCFAllocatorNull);
}

/*
 * oskext_info_get_load_address_and_size
 *
 * Description:
 * 	Retrieve the real load address and size of the kext from the kext info dictionary.
 *
 * Dependencies:
 * 	kernel_slide
 */
static void
oskext_info_get_load_address_and_size(CFDictionaryRef info, kaddr_t *load_address, size_t *size) {
	CFNumberRef number;
	number = (CFNumberRef)CFDictionaryGetValue(info, kCFBundleLoadAddressKey);
	assert(number != NULL);
	assert(CFGetTypeID(number) == CFNumberGetTypeID());
	bool success = CFNumberGetValue(number, kCFNumberSInt64Type, load_address);
	assert(success);
	*load_address += kernel_slide;
	if (size != NULL) {
		number = (CFNumberRef)CFDictionaryGetValue(info, kCFBundleLoadSizeKey);
		assert(number != NULL);
		assert(CFGetTypeID(number) == CFNumberGetTypeID());
		success = CFNumberGetValue(number, kCFNumberSInt64Type, size);
		assert(success);
	}
}

/*
 * oskext_get_load_address_and_size_info
 *
 * Description:
 * 	Retrieve the kext info dictionary for the specified kext. The dictionary must be released
 * 	by the caller.
 */
static kext_result
oskext_get_load_address_and_size_info(const char *kext, CFDictionaryRef *kext_info) {
	CFStringRef cfkext = cfstring_nocopy(kext);
	if (cfkext == NULL) {
		goto oom;
	}
	CFArrayRef kexts = CFArrayCreate(kCFAllocatorDefault,
			(const void **)&cfkext, 1, &kCFTypeArrayCallBacks);
	CFRelease(cfkext);
	if (kexts == NULL) {
		goto oom;
	}
	CFStringRef key_strings[2] = {
		kCFBundleLoadAddressKey, kCFBundleLoadSizeKey
	};
	CFArrayRef keys = CFArrayCreate(kCFAllocatorDefault,
			(const void **)key_strings, 2, &kCFTypeArrayCallBacks);
	if (keys == NULL) {
		CFRelease(kexts);
		goto oom;
	}
	CFDictionaryRef info = OSKextCopyLoadedKextInfo(kexts, keys);
	CFRelease(kexts);
	CFRelease(keys);
	if (info == NULL) {
		error_api_unavailable("OSKextCopyLoadedKextInfo");
		return KEXT_ERROR;
	}
	size_t count = (size_t)CFDictionaryGetCount(info);
	if (count == 0) {
		CFRelease(info);
		return KEXT_NOT_FOUND;
	}
	assert(count == 1);
	CFDictionaryGetKeysAndValues(info, NULL, (const void **)kext_info);
	CFRetain(*kext_info);
	CFRelease(info);
	return KEXT_SUCCESS;
oom:
	error_out_of_memory();
	return KEXT_ERROR;
}

kext_result
oskext_get_address(const char *bundle_id, kaddr_t *base, size_t *size) {
	CFDictionaryRef kext_info;
	kext_result kr = oskext_get_load_address_and_size_info(bundle_id, &kext_info);
	if (kr != KEXT_SUCCESS) {
		return kr;
	}
	oskext_info_get_load_address_and_size(kext_info, base, size);
	CFRelease(kext_info);
	return KEXT_SUCCESS;
}

bool
oskext_for_each(kext_for_each_callback_fn callback, void *context) {
	bool success = false;
	CFStringRef key_strings[3] = {
		kCFBundleIdentifierKey, kCFBundleLoadAddressKey,
		kCFBundleLoadSizeKey
	};
	CFArrayRef keys = CFArrayCreate(kCFAllocatorDefault,
			(const void **)key_strings, 3, &kCFTypeArrayCallBacks);
	if (keys == NULL) {
		error_out_of_memory();
		goto fail_0;
	}
	CFDictionaryRef dict = OSKextCopyLoadedKextInfo(NULL, keys);
	CFRelease(keys);
	if (dict == NULL) {
		error_api_unavailable("OSKextCopyLoadedKextInfo");
		goto fail_0;
	}
	size_t count = (size_t)CFDictionaryGetCount(dict);
	assert(count > 0);
	CFDictionaryRef *kext_info = (CFDictionaryRef *)malloc(count * sizeof(CFDictionaryRef));
	if (kext_info == NULL) {
		error_out_of_memory();
		goto fail_1;
	}
	CFDictionaryGetKeysAndValues(dict, NULL, (const void **)kext_info);
	for (size_t i = 0; i < count; i++) {
		kaddr_t base;
		size_t size;
		oskext_info_get_load_address_and_size(kext_info[i], &base, &size);
		CFStringRef cfbundleid = (CFStringRef)CFDictionaryGetValue(kext_info[i],
				kCFBundleIdentifierKey);
		char buf[BUNDLE_ID_BUFFER_SIZE];
		const char *bundle_id = CFStringGetCStringOrConvert(cfbundleid, buf, sizeof(buf));
		bool halt = callback(context, kext_info[i], bundle_id, base, size);
		if (halt) {
			break;
		}
	}
	success = true;
	free(kext_info);
fail_1:
	CFRelease(dict);
fail_0:
	return success;
}

/*
 * struct oskext_find_data
 *
 * Description:
 * 	oskext_for_each_callback_fn context for oskext_find_containing_address.
 */
struct oskext_find_data {
	kaddr_t kaddr;
	char *bundle_id;
	kaddr_t base;
	size_t size;
	bool error;
};

/*
 * oskext_find_callback
 *
 * Description:
 * 	oskext_for_each_callback_fn for oskext_find_containing_address.
 */
static bool
oskext_find_callback(void *context, CFDictionaryRef info, const char *bundle_id, kaddr_t base,
		size_t size) {
	struct oskext_find_data *c = context;
	if (c->kaddr < base || base + size <= c->kaddr) {
		return false;
	}
	c->bundle_id = strdup(bundle_id);
	if (c->bundle_id == NULL) {
		error_out_of_memory();
		c->error = true;
	}
	c->base = base;
	c->size = size;
	return true;
}

kext_result
oskext_find_containing_address(kaddr_t kaddr, char **bundle_id, kaddr_t *base, size_t *size) {
	struct oskext_find_data context = { kaddr };
	bool success = oskext_for_each(oskext_find_callback, &context);
	if (!success || context.error) {
		return KEXT_ERROR;
	}
	if (context.bundle_id == NULL) {
		return KEXT_NOT_FOUND;
	}
	*bundle_id = context.bundle_id;
	*base = context.base;
	*size = context.size;
	return KEXT_SUCCESS;
}

/*
 * oskext_get_executable
 *
 * Description:
 * 	Get the binary image for a kext.
 */
static kext_result
oskext_copy_binary(const char *bundle_id, void **binary, size_t *size) {
	CFStringRef cf_bundle_id = cfstring_nocopy(bundle_id);
	if (cf_bundle_id == NULL) {
		error_out_of_memory();
		return KEXT_ERROR;
	}
	OSKextRef kext = OSKextCreateWithIdentifier(kCFAllocatorDefault, cf_bundle_id);
	CFRelease(cf_bundle_id);
	if (kext == NULL) {
		return KEXT_NOT_FOUND;
	}
	CFDataRef executable = OSKextCopyExecutableForArchitecture(kext,
			OSKextGetRunningKernelArchitecture());
	CFRelease(kext);
	if (executable == NULL) {
		return KEXT_NOT_FOUND;
	}
	size_t data_size = CFDataGetLength(executable);
	const void *data = CFDataGetBytePtr(executable);
	void *data_copy = malloc(data_size);
	if (data_copy == NULL) {
		CFRelease(executable);
		error_out_of_memory();
		return KEXT_ERROR;
	}
	memcpy(data_copy, data, data_size);
	CFRelease(executable);
	*binary = data_copy;
	*size = data_size;
	return KEXT_SUCCESS;
}

kext_result
oskext_init_macho(struct macho *macho, const char *bundle_id) {
	void *binary;
	size_t size;
	kext_result kr = oskext_copy_binary(bundle_id, &binary, &size);
	if (kr != KEXT_SUCCESS) {
		return kr;
	}
	macho_result mr = macho_validate(binary, size);
	if (mr != MACHO_SUCCESS) {
		assert(mr == MACHO_ERROR);
		free(binary);
		return KEXT_ERROR;
	}
	macho->mh = binary;
	macho->size = size;
	return KEXT_SUCCESS;
}

void
oskext_deinit_macho(struct macho *macho) {
	assert(macho->mh != kernel.macho.mh);
	free(macho->mh);
}
