#include "kernelcache.h"

#include "memctl_common.h"
#include "memctl_error.h"

#include <IOKit/IOCFUnserialize.h>
#include <libkern/prelink.h>
#include <string.h>
#include <sys/mman.h>

const CFStringRef kCFPrelinkInfoDictionaryKey = CFSTR(kPrelinkInfoDictionaryKey);
const CFStringRef kCFPrelinkExecutableLoadKey = CFSTR(kPrelinkExecutableLoadKey);
const CFStringRef kCFPrelinkExecutableSizeKey = CFSTR(kPrelinkExecutableSizeKey);

// TODO: reimplement
// Source:
// https://opensource.apple.com/source/BootX/BootX-81/bootx.tproj/sl.subproj/lzss.c.auto.html
static int
decompress_lzss(uint8_t *dst, const uint8_t *src, uint32_t srclen) {
#define N         4096  /* size of ring buffer - must be power of 2 */
#define F         18    /* upper limit for match_length */
#define THRESHOLD 2     /* encode string into position and length
                           if match_length is greater than this */
	/* ring buffer of size N, with extra F-1 bytes to aid string comparison */
	uint8_t text_buf[N + F - 1];
	uint8_t *dststart = dst;
	const uint8_t *srcend = src + srclen;
	int  i, j, k, r, c;
	unsigned int flags;

	for (i = 0; i < N - F; i++)
		text_buf[i] = ' ';
	r = N - F;
	flags = 0;
	for (;;) {
		if (((flags >>= 1) & 0x100) == 0) {
			if (src < srcend) c = *src++; else break;
			flags = c | 0xFF00;  /* uses higher byte cleverly to count eight */
		}
		if (flags & 1) {
			if (src < srcend) c = *src++; else break;
			*dst++ = c;
			text_buf[r++] = c;
			r &= (N - 1);
		} else {
			if (src < srcend) i = *src++; else break;
			if (src < srcend) j = *src++; else break;
			i |= ((j & 0xF0) << 4);
			j  =  (j & 0x0F) + THRESHOLD;
			for (k = 0; k <= j; k++) {
				c = text_buf[(i + k) & (N - 1)];
				*dst++ = c;
				text_buf[r++] = c;
				r &= (N - 1);
			}
		}
	}
	return dst - dststart;
}

/*
 * decompress_complzss
 *
 * Description:
 * 	Decompress the given LZSS-compressed data.
 */
static bool
decompress_complzss(const void *src, size_t srclen, void *dest, size_t destlen) {
	// TODO: Use a real implementation that safely decompresses into the buffer.
	return decompress_lzss(dest, src, srclen) == destlen;
}

/*
 * kernelcache_info_get_address_and_size
 *
 * Description:
 * 	Retrieve the address and size of the kext from the kext's info dictionary.
 */
static bool
kernelcache_info_get_address_and_size(CFDictionaryRef info, kaddr_t *address, size_t *size) {
	CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(info, kCFPrelinkExecutableLoadKey);
	if (number == NULL) {
		return false;
	}
	assert(CFGetTypeID(number) == CFNumberGetTypeID());
	bool success = CFNumberGetValue(number, kCFNumberSInt64Type, address);
	assert(success);
	number = (CFNumberRef)CFDictionaryGetValue(info, kCFPrelinkExecutableSizeKey);
	assert(number != NULL);
	assert(CFGetTypeID(number) == CFNumberGetTypeID());
	success = CFNumberGetValue(number, kCFNumberSInt64Type, size);
	assert(success);
	return true;
}

kext_result
kernelcache_init_file(struct kernelcache *kc, const char *file) {
	const void *data;
	size_t size;
	if (!mmap_file(file, &data, &size)) {
		return KEXT_ERROR;
	}
	return kernelcache_init(kc, data, size);
}

/*
 * kernelcache_init_decompress
 *
 * Description:
 * 	Decompress the kernelcache data and initialize.
 *
 * Notes:
 * 	This is quite ad-hoc. In the future this should be implemented by parsing the IM4P file.
 */
static kext_result
kernelcache_init_decompress(struct kernelcache *kc, const void *data, size_t size) {
	if (size < 0x1000) {
		error_kernelcache("kernelcache too small");
		goto fail_0;
	}
	uint8_t im4p[4] = "IM4P";
	if (memmem(data, 128, im4p, sizeof(im4p)) == NULL) {
		error_kernelcache("compressed kernelcache not IMG4");
		goto fail_0;
	}
	// Find the complzss header.
	struct compression_header {
		uint8_t compression_type[8];
		uint32_t reserved1;
		uint32_t uncompressed_size;
		uint32_t compressed_size;
		uint32_t reserved2;
	} *h;
	uint8_t complzss[8] = "complzss";
	h = memmem(data, size, complzss, sizeof(complzss));
	if (h == NULL) {
		error_kernelcache("unknown IMG4 format");
		goto fail_0;
	}
	// Allocate memory to decompress the kernelcache.
	size_t uncompressed_size = ntohl(h->uncompressed_size);
	size_t compressed_size   = ntohl(h->compressed_size);
	void *decompressed = mmap(NULL, uncompressed_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
	if (decompressed == MAP_FAILED) {
		error_out_of_memory();
		goto fail_0;
	}
	// Find the start of the compressed data.
	const uint8_t *end = (const uint8_t *)data + size;
	const uint8_t *p   = (const uint8_t *)(h + 1);
	for (;;) {
		if (p >= end) {
			error_internal("could not find compressed data");
			goto fail_1;
		}
		if (*p != 0) {
			break;
		}
		p++;
	}
	// Decompress the kernelcache.
	if (!decompress_complzss(p, compressed_size, decompressed, uncompressed_size)) {
		error_internal("decompression failed");
		goto fail_1;
	}
	munmap((void *)data, size);
	return kernelcache_init_uncompressed(kc, decompressed, uncompressed_size);
fail_1:
	munmap(decompressed, uncompressed_size);
fail_0:
	munmap((void *)data, size);
	return KEXT_ERROR;
}

kext_result
kernelcache_init(struct kernelcache *kc, const void *data, size_t size) {
	const struct mach_header *mh = data;
	if (mh->magic == MH_MAGIC || mh->magic == MH_MAGIC_64) {
		return kernelcache_init_uncompressed(kc, data, size);
	}
	return kernelcache_init_decompress(kc, data, size);
}

/*
 * missing_segment
 *
 * Description:
 * 	Generate an error for a missing segment in the kernelcache.
 */
static void
missing_segment(const char *segname) {
	error_kernelcache("could not find %s segment", segname);
}

/*
 * kernelcache_find_text
 *
 * Description:
 * 	Find the __TEXT segment of the kernelcache, which should also include the Mach-O header.
 *
 * TODO: There should be a better way of doing this.
 */
static kext_result
kernelcache_find_text(const struct macho *kernel, const struct segment_command_64 **text) {
	macho_result mr = macho_find_segment_command_64(kernel, text, SEG_TEXT);
	if (mr != MACHO_SUCCESS) {
		missing_segment(SEG_TEXT);
		return KEXT_ERROR;
	}
	if ((*text)->fileoff != 0) {
		error_kernelcache("%s segment does not include Mach-O header", SEG_TEXT);
		return KEXT_ERROR;
	}
	return KEXT_SUCCESS;
}

/*
 * kernelcache_find_prelink_text
 *
 * Description:
 * 	Find the __PRELINK_TEXT segment of the kernelcache.
 */
static kext_result
kernelcache_find_prelink_text(const struct macho *kernel,
		const struct segment_command_64 **prelink_text) {
	macho_result mr = macho_find_segment_command_64(kernel, prelink_text, kPrelinkTextSegment);
	if (mr != MACHO_SUCCESS) {
		missing_segment(kPrelinkTextSegment);
		return KEXT_ERROR;
	}
	assert((*prelink_text)->vmsize == (*prelink_text)->filesize);
	return KEXT_SUCCESS;
}

kext_result
kernelcache_init_uncompressed(struct kernelcache *kc, const void *data, size_t size) {
	kext_result kr = KEXT_ERROR;
	kc->kernel.mh    = (void *)data;
	kc->kernel.size  = size;
	kc->prelink_info = NULL;
	macho_result mr = macho_validate(kc->kernel.mh, kc->kernel.size);
	if (mr != MACHO_SUCCESS) {
		error_kernelcache("not a valid kernelcache");
		goto fail;
	}
	kr = kernelcache_parse_prelink_info(&kc->kernel, &kc->prelink_info);
	if (kr != KEXT_SUCCESS) {
		goto fail;
	}
	kr = kernelcache_find_text(&kc->kernel, &kc->text);
	if (kr != KEXT_SUCCESS) {
		goto fail;
	}
	kr = kernelcache_find_prelink_text(&kc->kernel, &kc->prelink_text);
	if (kr != KEXT_SUCCESS) {
		goto fail;
	}
	return KEXT_SUCCESS;
fail:
	kernelcache_deinit(kc);
	return kr;
}

void
kernelcache_deinit(struct kernelcache *kc) {
	if (kc->kernel.mh != NULL) {
		munmap(kc->kernel.mh, kc->kernel.size);
		kc->kernel.mh = NULL;
		kc->kernel.size = 0;
	}
	if (kc->prelink_info != NULL) {
		CFRelease(kc->prelink_info);
		kc->prelink_info = NULL;
	}
}

kext_result
kernelcache_parse_prelink_info(const struct macho *kernel, CFDictionaryRef *prelink_info) {
	const struct segment_command_64 *sc;
	macho_result mr = macho_find_segment_command_64(kernel, &sc, kPrelinkInfoSegment);
	if (mr != MACHO_SUCCESS) {
		missing_segment(kPrelinkInfoSegment);
		return KEXT_ERROR;
	}
	const void *prelink_xml = (const void *)((uintptr_t)kernel->mh + sc->fileoff);
	// TODO: IOCFUnserialize expects the buffer to be NULL-terminated. We don't do this
	// explicitly. However, there appears to be some zero padding after the text anyway, so it
	// works in practice.
	CFStringRef error = NULL;
	CFTypeRef info = IOCFUnserialize(prelink_xml, NULL, 0, &error);
	if (info == NULL) {
		char buf[512];
		CFStringGetCString(error, buf, sizeof(buf), kCFStringEncodingUTF8);
		error_internal("IOCFUnserialize: %s", buf);
		CFRelease(error);
		return KEXT_ERROR;
	}
	if (CFGetTypeID(info) != CFDictionaryGetTypeID()) {
		error_internal("%s not a dictionary type", kPrelinkInfoSegment);
		return KEXT_ERROR;
	}
	*prelink_info = info;
	return KEXT_SUCCESS;
}

bool
kernelcache_kext_for_each(const struct kernelcache *kc, kext_for_each_callback_fn callback,
		void *context) {
	bool success = false;
	CFArrayRef prelink_dicts = CFDictionaryGetValue(kc->prelink_info,
			kCFPrelinkInfoDictionaryKey);
	assert(prelink_dicts != NULL);
	assert(CFGetTypeID(prelink_dicts) == CFArrayGetTypeID());
	CFIndex count = CFArrayGetCount(prelink_dicts);
	for (CFIndex i = 0; i < count; i++) {
		CFDictionaryRef info = CFArrayGetValueAtIndex(prelink_dicts, i);
		assert(info != NULL);
		assert(CFGetTypeID(info) == CFDictionaryGetTypeID());
		CFStringRef cfbundleid = (CFStringRef)CFDictionaryGetValue(info,
				kCFBundleIdentifierKey);
		char buf[BUNDLE_ID_BUFFER_SIZE];
		const char *bundle_id = CFStringGetCStringOrConvert(cfbundleid, buf, sizeof(buf));
		kaddr_t base = 0;
		size_t size = 0;
		kernelcache_info_get_address_and_size(info, &base, &size);
		bool halt = callback(context, info, bundle_id, base, size);
		if (halt) {
			break;
		}
	}
	success = true;
	return success;
}

bool
kernelcache_for_each(const struct kernelcache *kc, kext_for_each_callback_fn callback,
		void *context) {
	// TODO: We don't have the info dictionary for the kernel itself. :(
	bool halt = callback(context, NULL, KERNEL_ID, kc->text->vmaddr, kc->text->vmsize);
	if (halt) {
		return true;
	}
	return kernelcache_kext_for_each(kc, callback, context);
}

/*
 * struct kernelcache_get_address_context
 *
 * Description:
 * 	kext_for_each_callback_fn context for kernelcache_get_address.
 */
struct kernelcache_get_address_context {
	const char *bundle_id;
	kaddr_t base;
	size_t size;
};

/*
 * kernelcache_get_address_callback
 *
 * Description:
 * 	kext_for_each_callback_fn for kernelcache_get_address.
 */
static bool
kernelcache_get_address_callback(void *context, CFDictionaryRef info,
		const char *bundle_id, kaddr_t base, size_t size) {
	struct kernelcache_get_address_context *c = context;
	if (strcmp(c->bundle_id, bundle_id) != 0) {
		return false;
	}
	c->base = base;
	c->size = size;
	return true;
}

kext_result kernelcache_get_address(const struct kernelcache *kc,
		const char *bundle_id, kaddr_t *base, size_t *size) {
	struct kernelcache_get_address_context context = { bundle_id };
	bool success = kernelcache_for_each(kc, kernelcache_get_address_callback, &context);
	if (!success) {
		return KEXT_ERROR;
	}
	if (context.base == 0) {
		return KEXT_NO_KEXT;
	}
	*base = context.base;
	*size = context.size;
	return KEXT_SUCCESS;
}

/*
 * struct kernelcache_find_containing_address_context
 *
 * Description:
 * 	kext_for_each_callback_fn context for kernelcache_find_containing_address.
 */
struct kernelcache_find_containing_address_context {
	const struct kernelcache *kc;
	kaddr_t kaddr;
	char **bundle_id;
	kaddr_t *base;
	kext_result status;
};

/*
 * kernelcache_find_containing_address_callback
 *
 * Description:
 * 	kext_for_each_callback_fn for kernelcache_find_containing_address.
 */
static bool
kernelcache_find_containing_address_callback(void *context, CFDictionaryRef info,
		const char *bundle_id, kaddr_t base, size_t size) {
	struct kernelcache_find_containing_address_context *c = context;
	kaddr_t address = c->kaddr;
	if (base <= address && address < base + size) {
		goto found;
	}
	struct macho kext;
	// We don't need to wrap this call in error_stop() because this function never pushes
	// errors onto the error stack.
	kext_result kr = kernelcache_kext_init_macho_at_address(c->kc, &kext, base);
	if (kr != KEXT_SUCCESS) {
		return false;
	}
	const struct segment_command_64 *sc = NULL;
	for (;;) {
		error_stop();
		macho_result mr = macho_find_load_command_64(&kext,
				(const struct load_command **)&sc, LC_SEGMENT_64);
		error_start();
		if (mr != MACHO_SUCCESS || sc == NULL) {
			return false;
		}
		if (sc->vmaddr <= base && base < sc->vmaddr + sc->vmsize) {
			goto found;
		}
	}
found:
	c->status = KEXT_SUCCESS;
	if (c->bundle_id != NULL) {
		*c->bundle_id = strdup(bundle_id);
		if (*c->bundle_id == NULL) {
			error_out_of_memory();
			c->status = KEXT_ERROR;
		}
	}
	if (c->base != NULL) {
		*c->base = base;
	}
	return true;
}

kext_result
kernelcache_find_containing_address(const struct kernelcache *kc, kaddr_t kaddr,
		char **bundle_id, kaddr_t *base) {
	struct kernelcache_find_containing_address_context context =
		{ kc, kaddr, bundle_id, base, KEXT_NO_KEXT };
	bool success = kernelcache_for_each(kc, kernelcache_find_containing_address_callback,
			&context);
	if (!success) {
		return KEXT_ERROR;
	}
	return context.status;
}

kext_result
kernelcache_kext_init_macho(const struct kernelcache *kc, struct macho *macho,
		const char *bundle_id) {
	kaddr_t base;
	size_t size;
	kext_result kr = kernelcache_get_address(kc, bundle_id, &base, &size);
	if (kr != KEXT_SUCCESS) {
		return kr;
	}
	return kernelcache_kext_init_macho_at_address(kc, macho, base);
}

kext_result
kernelcache_kext_init_macho_at_address(const struct kernelcache *kc, struct macho *macho,
		kaddr_t base) {
	if (base == kc->text->vmaddr) {
		*macho = kc->kernel;
		return KEXT_SUCCESS;
	}
	if (base < kc->prelink_text->vmaddr
	    || kc->prelink_text->vmaddr + kc->prelink_text->vmsize <= base) {
		// All kernel extension Mach-O headers should lie within the __PRELINK_TEXT
		// segment.
		return KEXT_NO_KEXT;
	}
	size_t kextoff = kc->prelink_text->fileoff + (base - kc->prelink_text->vmaddr);
	macho->mh = (void *)((uintptr_t)kc->kernel.mh + kextoff);
	macho->size = kc->kernel.size - kextoff;
	if (!macho_validate(macho->mh, macho->size)) {
		return KEXT_NO_KEXT;
	}
	return KEXT_SUCCESS;
}
