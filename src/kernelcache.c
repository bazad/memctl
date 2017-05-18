#include "memctl/kernelcache.h"

#include "memctl/memctl_error.h"

#include "memctl_common.h"

#include <compression.h>
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
 * kernelcache_decompress_complzss
 *
 * Description:
 * 	Try to decompress an LZSS-compressed kernelcache.
 *
 * Notes:
 * 	This function is a giant hack and definitely is not robust or secure.
 */
static bool
kernelcache_decompress_complzss(const void *data, size_t size,
		const void **decompressed, size_t *decompressed_size) {
	// Find the complzss header.
	struct compression_header {
		uint8_t compression_type[8];
		uint32_t reserved1;
		uint32_t uncompressed_size;
		uint32_t compressed_size;
		uint32_t reserved2;
	} *h;
	const uint8_t complzss[8] = "complzss";
	h = memmem(data, 0x200, complzss, sizeof(complzss));
	if (h == NULL) {
		return false;
	}
	// Find the start of the compressed data.
	const uint8_t *end = (const uint8_t *)data + size;
	const uint8_t *p   = (const uint8_t *)(h + 1);
	for (;;) {
		if (p >= end) {
			error_internal("could not find compressed data");
			goto fail;
		}
		if (*p != 0) {
			break;
		}
		p++;
	}
	// Allocate memory to decompress the kernelcache.
	size_t dsize = ntohl(h->uncompressed_size);
	size_t csize = ntohl(h->compressed_size);
	void *ddata = mmap(NULL, dsize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
	if (ddata == MAP_FAILED) {
		error_out_of_memory();
		goto fail;
	}
	// Decompress the kernelcache.
	if (!decompress_complzss(p, csize, ddata, dsize)) {
		error_internal("decompression failed");
		munmap(ddata, dsize);
		goto fail;
	}
	*decompressed      = ddata;
	*decompressed_size = dsize;
fail:
	return true;
}

/*
 * kernelcache_decompress_lzfse
 *
 * Description:
 * 	Try to decompress an lzfse-compressed kernelcache.
 */
static bool
kernelcache_init_decompress_lzfse(const void *data, size_t size,
		const void **decompressed, size_t *decompressed_size) {
	const uint8_t lzfse_sig[4] = "bvx2";
	const void *lzfse = memmem(data, 0x200, lzfse_sig, sizeof(lzfse_sig));
	if (lzfse == NULL) {
		return false;
	}
	// Try to decompress the data.
	size_t dalloc = 4 * size;
	void *ddata;
	size_t dsize;
	size_t csize = size - ((uintptr_t)lzfse - (uintptr_t)data);
	uint8_t dbuf[compression_decode_scratch_buffer_size(COMPRESSION_LZFSE)];
	for (;;) {
		ddata = mmap(NULL, dalloc, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0,
				0);
		if (ddata == MAP_FAILED) {
			error_out_of_memory();
			goto fail;
		}
		dsize = compression_decode_buffer(ddata, dalloc, lzfse, csize, dbuf,
				COMPRESSION_LZFSE);
		if (dsize < dalloc) {
			break;
		}
		munmap(ddata, dalloc);
		dalloc *= 2;
		memctl_warning("decompress_lzfse: reallocating decompression buffer");
	}
	*decompressed      = ddata;
	*decompressed_size = dsize;
fail:
	return true;
}

/*
 * kernelcache_decompress
 *
 * Description:
 * 	Try to decompress the kernelcache data using all known compression schemes.
 *
 * Notes:
 * 	This is quite ad-hoc. In the future this should be implemented by parsing the IM4P file.
 */
static bool
kernelcache_decompress(const void *data, size_t size,
		const void **decompressed, size_t *decompressed_size) {
	// Sanity checks.
	const uint8_t im4p[4] = "IM4P";
	if (size < 0x1000 || memmem(data, 0x80, im4p, sizeof(im4p)) == NULL) {
		return false;
	}
	// Try to decompress the kernelcache data.
	return kernelcache_decompress_complzss(data, size, decompressed, decompressed_size)
		|| kernelcache_init_decompress_lzfse(data, size, decompressed, decompressed_size);
}

kext_result
kernelcache_init(struct kernelcache *kc, const void *data, size_t size) {
	const void *decompressed = NULL;
	size_t decompressed_size;
	bool is_compressed = kernelcache_decompress(data, size, &decompressed, &decompressed_size);
	if (is_compressed) {
		munmap((void *)data, size);
		if (decompressed == NULL) {
			// This is a compressed kernelcache but there was an error during the
			// decompression.
			error_kernelcache("could not decompress kernelcache");
			return KEXT_ERROR;
		}
		data = decompressed;
		size = decompressed_size;
	}
	if (size < 0x1000) {
		error_kernelcache("kernelcache too small");
		munmap((void *)data, size);
		return KEXT_ERROR;
	}
	return kernelcache_init_uncompressed(kc, data, size);
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
 */
static kext_result
kernelcache_find_text(const struct macho *kernel, const struct segment_command_64 **text) {
	const struct segment_command_64 *sc = (const struct segment_command_64 *)
		macho_find_segment(kernel, SEG_TEXT);
	if (sc == NULL) {
		missing_segment(SEG_TEXT);
		return KEXT_ERROR;
	}
	if (sc->fileoff != 0) {
		error_kernelcache("%s segment does not include Mach-O header", SEG_TEXT);
		return KEXT_ERROR;
	}
	*text = sc;
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
	const struct segment_command_64 *sc = (const struct segment_command_64 *)
		macho_find_segment(kernel, kPrelinkTextSegment);
	if (sc == NULL) {
		missing_segment(kPrelinkTextSegment);
		return KEXT_ERROR;
	}
	assert(sc->vmsize == sc->filesize);
	*prelink_text = sc;
	return KEXT_SUCCESS;
}

/*
 * kernelcache_extract_fat
 *
 * Description:
 * 	Extract the appropriate architecture from a FAT Mach-O.
 *
 * TODO:
 * 	Some of this functionality should be provided by macho.h.
 */
static bool
kernelcache_extract_fat(const void *data, size_t size, void **macho_data, size_t *macho_size) {
	const struct fat_header *fh = (const struct fat_header *)data;
	bool swap;
	if (fh->magic == FAT_MAGIC) {
		swap = false;
	} else if (fh->magic == FAT_CIGAM) {
		swap = true;
	} else {
		return false;
	}
	uint32_t nfat_arch = fh->nfat_arch;
	if (swap) {
		nfat_arch = ntohl(nfat_arch);
	}
	if (nfat_arch != 1) {
		goto fail;
	}
	const struct fat_arch *fa = (const struct fat_arch *)(fh + 1);
	uint32_t arch_offset = fa->offset;
	uint32_t arch_size   = fa->size;
	if (swap) {
		arch_offset = ntohl(arch_offset);
		arch_size   = ntohl(arch_size);
	}
	*macho_data = (void *)((uintptr_t)data + arch_offset);
	*macho_size = arch_size;
fail:
	return true;
}

/*
 * kernelcache_get_kernel
 *
 * Description:
 * 	Initialize the kernel from the kernelcache data, unwrapping a FAT binary if necessary.
 */
static bool
kernelcache_get_kernel(struct macho *kernel, const void *data, size_t size) {
	bool is_fat = kernelcache_extract_fat(data, size, &kernel->mh, &kernel->size);
	if (is_fat) {
		if (kernel->mh == NULL) {
			// This is a FAT binary, but there was an error extracting the appropriate
			// slice.
			error_kernelcache("could not extract kernelcache from FAT binary");
			return false;
		}
	} else {
		// Assume the data is a Mach-O file.
		macho_result mr = macho_validate(data, size);
		if (mr != MACHO_SUCCESS) {
			error_kernelcache("not a valid kernelcache");
			return false;
		}
		kernel->mh   = (void *)data;
		kernel->size = size;
	}
	return true;
}

kext_result
kernelcache_init_uncompressed(struct kernelcache *kc, const void *data, size_t size) {
	kext_result kr = KEXT_ERROR;
	kc->data         = data;
	kc->size         = size;
	kc->kernel.mh    = NULL;
	kc->prelink_info = NULL;
	bool success = kernelcache_get_kernel(&kc->kernel, data, size);
	if (!success) {
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
	if (kc->data != NULL) {
		munmap((void *)kc->data, kc->size);
		kc->data = NULL;
		kc->size = 0;
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
	const struct segment_command_64 *sc = (const struct segment_command_64 *)
		macho_find_segment(kernel, kPrelinkInfoSegment);
	if (sc == NULL) {
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

void
kernelcache_kext_for_each(const struct kernelcache *kc, kext_for_each_callback_fn callback,
		void *context) {
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
}

void
kernelcache_for_each(const struct kernelcache *kc, kext_for_each_callback_fn callback,
		void *context) {
	// TODO: We don't have the info dictionary for the kernel itself. :(
	bool halt = callback(context, NULL, KERNEL_ID, kc->text->vmaddr, kc->text->vmsize);
	if (!halt) {
		kernelcache_kext_for_each(kc, callback, context);
	}
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
	kernelcache_for_each(kc, kernelcache_get_address_callback, &context);
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
	const struct load_command *lc = macho_segment_containing_address(&kext, address);
	if (lc == NULL) {
		return false;
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
	// It's important to process the kernel itself last, since its segments contain those of
	// all the prelinked kexts.
	kernelcache_kext_for_each(kc, kernelcache_find_containing_address_callback, &context);
	if (context.status == KEXT_NO_KEXT) {
		kernelcache_find_containing_address_callback(&context, NULL, KERNEL_ID,
				kc->text->vmaddr, kc->text->vmsize);
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
	kr = kernelcache_kext_init_macho_at_address(kc, macho, base);
	if (kr != KEXT_SUCCESS) {
		error_kernelcache("prelink info error: could not initialize kext %s at base "
		                  "address %llx", bundle_id, (unsigned long long)base);
		return KEXT_ERROR;
	}
	return KEXT_SUCCESS;
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
	error_stop();
	macho_result mr = macho_validate(macho->mh, macho->size);
	error_start();
	if (mr != MACHO_SUCCESS) {
		assert(mr == MACHO_ERROR);
		return KEXT_NO_KEXT;
	}
	return KEXT_SUCCESS;
}
