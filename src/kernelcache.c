#include "kernelcache.h"

#include "memctl_common.h"
#include "memctl_error.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFUnserialize.h>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>//TODO

#define kPrelinkInfoSegment                "__PRELINK_INFO"

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

kernelcache_result
kernelcache_init_file(struct kernelcache *kc, const char *file) {
	const void *data;
	size_t size;
	if (!mmap_file(file, &data, &size)) {
		return KERNELCACHE_ERROR;
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
static kernelcache_result
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
	return KERNELCACHE_ERROR;
}

kernelcache_result
kernelcache_init(struct kernelcache *kc, const void *data, size_t size) {
	const struct mach_header *mh = data;
	if (mh->magic == MH_MAGIC || mh->magic == MH_MAGIC_64) {
		return kernelcache_init_uncompressed(kc, data, size);
	}
	return kernelcache_init_decompress(kc, data, size);
}

/*
 * kernelcache_parse_prelink_info
 *
 * Description:
 * 	Try to find and parse the __PRELINK_INFO segment.
 */
static bool
kernelcache_parse_prelink_info(const struct kernelcache *kc, CFDictionaryRef *prelink_info) {
	const struct segment_command_64 *sc;
	macho_result mr = macho_find_segment_command_64(&kc->kernel, &sc, kPrelinkInfoSegment);
	if (mr != MACHO_SUCCESS) {
		if (mr == MACHO_NOT_FOUND) {
			error_kernelcache("kernelcache parsing without __PRELINK_INFO is not "
			                  "supported");
		} else {
			error_kernelcache("error while locating __PRELINK_INFO");
		}
		return false;
	}
	const void *prelink_xml = (const void *)((uintptr_t)kc->kernel.mh + sc->fileoff);
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
		return false;
	}
	if (CFGetTypeID(info) != CFDictionaryGetTypeID()) {
		error_internal("__PRELINK_INFO not a dictionary type");
		return false;
	}
	*prelink_info = info;
	return true;
}

/*
 * kernelcache_init_kexts
 *
 * Description:
 * 	Initialize kext information for the kernelcache.
 */
static kernelcache_result
kernelcache_init_kexts(struct kernelcache *kc) {
	CFDictionaryRef prelink_info;
	if (!kernelcache_parse_prelink_info(kc, &prelink_info)) {
		return KERNELCACHE_ERROR;
	}
	return KERNELCACHE_SUCCESS;
}

kernelcache_result
kernelcache_init_uncompressed(struct kernelcache *kc, const void *data, size_t size) {
	kc->kernel.mh   = (void *)data;
	kc->kernel.size = size;
	kc->nkexts      = 0;
	kc->kexts       = NULL;
	macho_result mr = macho_validate(kc->kernel.mh, kc->kernel.size);
	if (mr != MACHO_SUCCESS) {
		error_kernelcache("not a valid kernelcache");
		goto fail;
	}
	kernelcache_result kr = kernelcache_init_kexts(kc);
	if (kr != KERNELCACHE_SUCCESS) {
		goto fail;
	}
	return KERNELCACHE_SUCCESS;
fail:
	kernelcache_deinit(kc);
	return KERNELCACHE_ERROR;
}

void
kernelcache_deinit(struct kernelcache *kc) {
	if (kc->kernel.mh != NULL) {
		munmap(kc->kernel.mh, kc->kernel.size);
		kc->kernel.mh = NULL;
		kc->kernel.size = 0;
	}
	if (kc->nkexts > 0) {
		free(kc->kexts);
		kc->nkexts = 0;
		kc->kexts = NULL;
	}
}
