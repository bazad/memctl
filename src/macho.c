#include "macho.h"

#include "memctl_error.h"

#include <assert.h>
#include <string.h>

bool
macho_is_32(const struct macho *macho) {
	return (macho->mh32->magic == MH_MAGIC);
}

bool
macho_is_64(const struct macho *macho) {
	return (macho->mh32->magic == MH_MAGIC_64);
}

size_t
macho_header_size(const struct macho *macho) {
	if (macho_is_32(macho)) {
		return sizeof(*macho->mh32);
	} else {
		return sizeof(*macho->mh64);
	}
}

/*
 * macho_get_nlist
 */
static const struct nlist *
macho_get_nlist_32(const struct macho *macho, const struct symtab_command *symtab) {
	assert(macho_is_32(macho));
	return (const struct nlist *)((uintptr_t)macho->mh + symtab->symoff);
}

/*
 * macho_get_nlist_64
 */
static const struct nlist_64 *
macho_get_nlist_64(const struct macho *macho, const struct symtab_command *symtab) {
	assert(macho_is_64(macho));
	return (const struct nlist_64 *)((uintptr_t)macho->mh + symtab->symoff);
}

/*
 * macho_get_section_32
 *
 * Description:
 * 	Find the given section in the 32-bit Mach-O image.
 */
static const struct section *
macho_get_section_32(const struct macho *macho, uint32_t sect) {
	assert(macho_is_32(macho));
	if (sect < 1) {
		return NULL;
	}
	const struct load_command *lc = NULL;
	uint32_t idx = 1;
	const struct section *sectcmd = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command_32(macho, &lc, LC_SEGMENT);
		if (mr != MACHO_SUCCESS || lc == NULL) {
			break;
		}
		const struct segment_command *sc = (const struct segment_command *)lc;
		if (sect < idx + sc->nsects) {
			sectcmd = (const struct section *) ((uintptr_t)sc + sizeof(*sc));
			sectcmd += sect - idx;
			break;
		}
		idx += sc->nsects;
	}
	return sectcmd;
}

/*
 * macho_get_section_64
 *
 * Description:
 * 	Find the given section in the 32-bit Mach-O image.
 */
static const struct section_64 *
macho_get_section_64(const struct macho *macho, uint32_t sect) {
	assert(macho_is_64(macho));
	if (sect < 1) {
		return NULL;
	}
	const struct load_command *lc = NULL;
	uint32_t idx = 1;
	const struct section_64 *sectcmd = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command_64(macho, &lc, LC_SEGMENT_64);
		if (mr != MACHO_SUCCESS || lc == NULL) {
			break;
		}
		const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
		if (sect < idx + sc->nsects) {
			sectcmd = (const struct section_64 *) ((uintptr_t)sc + sizeof(*sc));
			sectcmd += sect - idx;
			break;
		}
		idx += sc->nsects;
	}
	return sectcmd;
}

static size_t
guess_symbol_size_32(const struct macho *macho, const struct symtab_command *symtab,
		uint32_t idx, uint32_t next) {
	assert(macho_is_32(macho));
	const struct nlist *nl = macho_get_nlist_32(macho, symtab) + idx;
	size_t size = -1;
	if (next != -1) {
		size = next - nl->n_value;
	}
	const struct section *sect = macho_get_section_32(macho, nl->n_sect);
	if (sect != NULL
	    && sect->addr <= nl->n_value
	    && nl->n_value < sect->addr + sect->size) {
		size_t sect_size = sect->addr + sect->size - nl->n_value;
		if (sect_size < size) {
			size = sect_size;
		}
	}
	// TODO: Use segment size if section size failed.
	return (size == -1 ? 0 : size);
}

static size_t
guess_symbol_size_64(const struct macho *macho, const struct symtab_command *symtab,
		uint64_t idx, uint32_t next) {
	assert(macho_is_64(macho));
	const struct nlist_64 *nl = macho_get_nlist_64(macho, symtab) + idx;
	size_t size = -1;
	if (next != -1) {
		size = next - nl->n_value;
	}
	const struct section_64 *sect = macho_get_section_64(macho, nl->n_sect);
	if (sect != NULL
	    && sect->addr <= nl->n_value
	    && nl->n_value < sect->addr + sect->size) {
		size_t sect_size = sect->addr + sect->size - nl->n_value;
		if (sect_size < size) {
			size = sect_size;
		}
	}
	// TODO: Use segment size if section size failed.
	return (size == -1 ? 0 : size);
}

// TODO: Make this resilient to malformed images.
/*
 * macho_string_index
 *
 * Description:
 * 	Find the index of the string in the string table.
 */
static uint64_t
macho_string_index(const struct macho *macho, const struct symtab_command *symtab,
		const char *name) {
	uintptr_t base = (uintptr_t)macho->mh + symtab->stroff;
	const char *str = (const char *)(base + 4);
	const char *end = (const char *)(base + symtab->strsize);
	uint64_t strx;
	for (;; str++) {
		strx = (uintptr_t)str - base;
		const char *p = name;
		for (;;) {
			if (str >= end) {
				return 0;
			}
			if (*p != *str) {
				while (str < end && *str != 0) {
					str++;
				}
				break;
			}
			if (*p == 0) {
				return strx;
			}
			p++;
			str++;
		}
	}
}

macho_result
macho_validate_32(const struct mach_header *mh, size_t size) {
	if (mh->magic != MH_MAGIC) {
		error_macho("32-bit Mach-O invalid magic: %x", mh->magic);
		return MACHO_ERROR;
	}
	if (size < sizeof(*mh)) {
		error_macho("32-bit Mach-O too small");
		return MACHO_ERROR;
	}
	if (mh->sizeofcmds > size) {
		error_macho("Mach-O sizeofcmds greater than file size");
		return MACHO_ERROR;
	}
	// TODO: Validate commands.
	return MACHO_SUCCESS;
}

macho_result
macho_validate_64(const struct mach_header_64 *mh, size_t size) {
	if (mh->magic != MH_MAGIC_64) {
		error_macho("64-bit Mach-O invalid magic: %x", mh->magic);
		return MACHO_ERROR;
	}
	if (size < sizeof(*mh)) {
		error_macho("64-bit Mach-O too small");
		return MACHO_ERROR;
	}
	if (mh->sizeofcmds > size) {
		error_macho("Mach-O sizeofcmds greater than file size");
		return MACHO_ERROR;
	}
	// TODO: Validate commands.
	return MACHO_SUCCESS;
}

macho_result
macho_validate(const void *mh, size_t size) {
	const struct mach_header *mh32 = mh;
	if (size < sizeof(*mh32)) {
		error_macho("Mach-O too small");
		return MACHO_ERROR;
	}
	if (mh32->magic == MH_MAGIC) {
		return macho_validate_32(mh32, size);
	} else if (mh32->magic == MH_MAGIC_64) {
		return macho_validate_64((const struct mach_header_64 *)mh, size);
	} else {
		error_macho("Mach-O invalid magic: %x", mh32->magic);
		return MACHO_ERROR;
	}
}

macho_result
macho_next_load_command_32(const struct macho *macho, const struct load_command **lc) {
	assert(macho_is_32(macho));
	const struct load_command *lc0 = *lc;
	const struct mach_header *mh = macho->mh32;
	if (lc0 == NULL) {
		lc0 = (const struct load_command *)((uintptr_t)mh + sizeof(*mh));
	} else {
		lc0 = (const struct load_command *)((uintptr_t)lc0 + lc0->cmdsize);
	}
	uintptr_t end = (uintptr_t)mh + mh->sizeofcmds;
	if ((uintptr_t)lc0 >= end) {
		lc0 = NULL;
	}
	*lc = lc0;
	return MACHO_SUCCESS;
}

macho_result
macho_next_load_command_64(const struct macho *macho, const struct load_command **lc) {
	assert(macho_is_64(macho));
	const struct load_command *lc0 = *lc;
	const struct mach_header_64 *mh = macho->mh;
	if (lc0 == NULL) {
		lc0 = (const struct load_command *)((uintptr_t)mh + sizeof(*mh));
	} else {
		lc0 = (const struct load_command *)((uintptr_t)lc0 + lc0->cmdsize);
	}
	uintptr_t end = (uintptr_t)mh + mh->sizeofcmds;
	if ((uintptr_t)lc0 >= end) {
		lc0 = NULL;
	}
	*lc = lc0;
	return MACHO_SUCCESS;
}

macho_result
macho_next_load_command(const struct macho *macho, const struct load_command **lc) {
	if (macho_is_32(macho)) {
		return macho_next_load_command_32(macho, lc);
	} else {
		return macho_next_load_command_64(macho, lc);
	}
}

macho_result
macho_find_load_command_32(const struct macho *macho, const struct load_command **lc, uint32_t cmd) {
	assert(macho_is_32(macho));
	for (;;) {
		macho_result mr = macho_next_load_command_32(macho, lc);
		if (mr != MACHO_SUCCESS || *lc == NULL) {
			return mr;
		}
		if ((*lc)->cmd == cmd) {
			return MACHO_SUCCESS;
		}
	}
}

macho_result
macho_find_load_command_64(const struct macho *macho, const struct load_command **lc, uint32_t cmd) {
	assert(macho_is_64(macho));
	for (;;) {
		macho_result mr = macho_next_load_command_64(macho, lc);
		if (mr != MACHO_SUCCESS || *lc == NULL) {
			return mr;
		}
		if ((*lc)->cmd == cmd) {
			return MACHO_SUCCESS;
		}
	}
}

macho_result
macho_find_load_command(const struct macho *macho, const struct load_command **lc, uint32_t cmd) {
	if (macho_is_32(macho)) {
		return macho_find_load_command_32(macho, lc, cmd);
	} else {
		return macho_find_load_command_64(macho, lc, cmd);
	}
}

macho_result
macho_find_segment_command_32(const struct macho *macho, const struct segment_command **lc,
		const char *segname) {
	assert(macho_is_32(macho));
	const struct load_command *lc0 = NULL;
	for (;;) {
		macho_result mr = macho_next_load_command_32(macho, &lc0);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (lc0 == NULL) {
			return MACHO_NOT_FOUND;
		}
		if (lc0->cmd != LC_SEGMENT) {
			continue;
		}
		const struct segment_command *sc = (const struct segment_command *)lc0;
		if (strcmp(sc->segname, segname) != 0) {
			continue;
		}
		*lc = sc;
		return MACHO_SUCCESS;
	}
}

macho_result
macho_find_segment_command_64(const struct macho *macho, const struct segment_command_64 **lc,
		const char *segname) {
	assert(macho_is_64(macho));
	const struct load_command *lc0 = NULL;
	for (;;) {
		macho_result mr = macho_next_load_command_64(macho, &lc0);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (lc0 == NULL) {
			return MACHO_NOT_FOUND;
		}
		if (lc0->cmd != LC_SEGMENT_64) {
			continue;
		}
		const struct segment_command_64 *sc = (const struct segment_command_64 *)lc0;
		if (strcmp(sc->segname, segname) != 0) {
			continue;
		}
		*lc = sc;
		return MACHO_SUCCESS;
	}
}

macho_result
macho_find_segment_command(const struct macho *macho, const struct load_command **lc,
		const char *segname) {
	if (macho_is_32(macho)) {
		return macho_find_segment_command_32(macho, (const struct segment_command **)lc,
				segname);
	} else {
		return macho_find_segment_command_64(macho, (const struct segment_command_64 **)lc,
				segname);
	}
}

macho_result
macho_find_base_32(struct macho *macho, uint32_t *base) {
	assert(macho_is_32(macho));
	const struct segment_command *sc = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command_32(macho, (const struct load_command **)&sc,
				LC_SEGMENT);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (sc == NULL) {
			return MACHO_NOT_FOUND;
		}
		if (sc->fileoff != 0 || sc->filesize == 0) {
			continue;
		}
		*base = sc->vmaddr;
		return MACHO_SUCCESS;
	}
}

macho_result
macho_find_base_64(struct macho *macho, uint64_t *base) {
	assert(macho_is_64(macho));
	const struct segment_command_64 *sc = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command_64(macho, (const struct load_command **)&sc,
				LC_SEGMENT_64);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (sc == NULL) {
			return MACHO_NOT_FOUND;
		}
		if (sc->fileoff != 0 || sc->filesize == 0) {
			continue;
		}
		*base = sc->vmaddr;
		return MACHO_SUCCESS;
	}
}

macho_result
macho_find_base(struct macho *macho, uint64_t *base) {
	if (macho_is_32(macho)) {
		uint32_t base32;
		macho_result mr = macho_find_base_32(macho, &base32);
		if (mr == MACHO_SUCCESS) {
			*base = base32;
		}
		return mr;
	} else {
		return macho_find_base_64(macho, base);
	}
}

macho_result
macho_resolve_symbol_32(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, uint32_t *addr, size_t *size) {
	assert(macho_is_32(macho));
	uint32_t strx = macho_string_index(macho, symtab, symbol);
	if (strx == 0) {
		return MACHO_NOT_FOUND;
	}
	uint32_t addr0 = 0;
	const struct nlist *nl = macho_get_nlist_32(macho, symtab);
	uint32_t symidx = -1;
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		if (nl[i].n_un.n_strx == strx) {
			if ((nl[i].n_type & N_TYPE) == N_UNDF) {
				return MACHO_NOT_FOUND;
			}
			if ((nl[i].n_type & N_TYPE) != N_SECT) {
				error_macho("unexpected Mach-O symbol type %x for symbol %s",
						nl[i].n_type & N_TYPE, symbol);
				return MACHO_ERROR;
			}
			addr0 = nl[i].n_value;
			symidx = i;
			break;
		}
	}
	if (symidx == -1) {
		return MACHO_NOT_FOUND;
	}
	if (addr != NULL) {
		*addr = addr0;
	}
	if (size != NULL) {
		uint32_t next = -1;
		for (uint32_t i = 0; i < symtab->nsyms; i++) {
			if (nl[i].n_value > addr0 && nl[i].n_value < next) {
				next = nl[i].n_value;
			}
		}
		*size = guess_symbol_size_32(macho, symtab, symidx, next);
	}
	return MACHO_SUCCESS;
}

macho_result
macho_resolve_symbol_64(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, uint64_t *addr, size_t *size) {
	assert(macho_is_64(macho));
	uint64_t strx = macho_string_index(macho, symtab, symbol);
	if (strx == 0) {
		return MACHO_NOT_FOUND;
	}
	uint64_t addr0 = 0;
	const struct nlist_64 *nl = macho_get_nlist_64(macho, symtab);
	uint32_t symidx = -1;
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		if (nl[i].n_un.n_strx == strx) {
			if ((nl[i].n_type & N_TYPE) == N_UNDF) {
				return MACHO_NOT_FOUND;
			}
			if ((nl[i].n_type & N_TYPE) != N_SECT) {
				error_macho("unexpected Mach-O symbol type %x for symbol %s",
						nl[i].n_type & N_TYPE, symbol);
				return MACHO_ERROR;
			}
			addr0 = nl[i].n_value;
			symidx = i;
			break;
		}
	}
	if (symidx == -1) {
		return MACHO_NOT_FOUND;
	}
	if (addr != NULL) {
		*addr = addr0;
	}
	if (size != NULL) {
		uint64_t next = -1;
		for (uint32_t i = 0; i < symtab->nsyms; i++) {
			if (nl[i].n_value > addr0 && nl[i].n_value < next) {
				next = nl[i].n_value;
			}
		}
		*size = guess_symbol_size_64(macho, symtab, symidx, next);
	}
	return MACHO_SUCCESS;
}

// TODO: Make this resilient to malformed images.
macho_result
macho_resolve_symbol(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, uint64_t *addr, size_t *size) {
	if (macho_is_32(macho)) {
		uint32_t addr32;
		macho_result mr = macho_resolve_symbol_32(macho, symtab, symbol, &addr32, size);
		if (mr == MACHO_SUCCESS) {
			*addr = addr32;
		}
		return mr;
	} else {
		return macho_resolve_symbol_64(macho, symtab, symbol, addr, size);
	}
}

macho_result
macho_resolve_address_32(const struct macho *macho, const struct symtab_command *symtab,
		uint32_t addr, const char **name, size_t *size, size_t *offset) {
	assert(macho_is_32(macho));
	const struct nlist *nl = macho_get_nlist_32(macho, symtab);
	uint32_t symidx = -1;
	uint32_t next = -1;
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		if ((nl[i].n_type & N_TYPE) != N_SECT) {
			continue; // TODO: Handle other symbol types.
		}
		if ((symidx == -1 || nl[symidx].n_value < nl[i].n_value)
		    && nl[i].n_value <= addr) {
			symidx = i;
		} else if (addr < nl[i].n_value && nl[i].n_value <= next) {
			next = nl[i].n_value;
		}
	}
	if (symidx == -1) {
		return MACHO_NOT_FOUND;
	}
	if (nl[symidx].n_sect == NO_SECT) {
		error_macho("symbol index %d has no section", symidx);
		return MACHO_ERROR;
	}
	*name = (const char *)((uintptr_t)macho->mh + symtab->stroff + nl[symidx].n_un.n_strx);
	*size = guess_symbol_size_32(macho, symtab, symidx, next);
	*offset = addr - nl[symidx].n_value;
	return MACHO_SUCCESS;
}

macho_result
macho_resolve_address_64(const struct macho *macho, const struct symtab_command *symtab,
		uint64_t addr, const char **name, size_t *size, size_t *offset) {
	assert(macho_is_64(macho));
	const struct nlist_64 *nl = macho_get_nlist_64(macho, symtab);
	uint32_t symidx = -1;
	uint64_t next = -1;
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		if ((nl[i].n_type & N_TYPE) != N_SECT) {
			continue; // TODO: Handle other symbol types.
		}
		if ((symidx == -1 || nl[symidx].n_value < nl[i].n_value)
		    && nl[i].n_value <= addr) {
			symidx = i;
		} else if (addr < nl[i].n_value && nl[i].n_value <= next) {
			next = nl[i].n_value;
		}
	}
	if (symidx == -1) {
		return MACHO_NOT_FOUND;
	}
	if (nl[symidx].n_sect == NO_SECT) {
		error_macho("symbol index %d has no section", symidx);
		return MACHO_ERROR;
	}
	*name = (const char *)((uintptr_t)macho->mh + symtab->stroff + nl[symidx].n_un.n_strx);
	*size = guess_symbol_size_64(macho, symtab, symidx, next);
	*offset = addr - nl[symidx].n_value;
	return MACHO_SUCCESS;
}

macho_result
macho_resolve_address(const struct macho *macho, const struct symtab_command *symtab,
		uint64_t addr, const char **name, size_t *size, size_t *offset) {
	if (macho_is_32(macho)) {
		return macho_resolve_address_32(macho, symtab, addr, name, size, offset);
	} else {
		return macho_resolve_address_64(macho, symtab, addr, name, size, offset);
	}
}

// TODO: Make this resilient to malformed images.
macho_result
macho_search_data_32(const struct macho *macho, const void *data, size_t size,
		int minprot, uint32_t *addr) {
	assert(macho_is_32(macho));
	const struct load_command *lc = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command_32(macho, &lc, LC_SEGMENT);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (lc == NULL) {
			return MACHO_NOT_FOUND;
		}
		const struct segment_command *sc = (const struct segment_command *)lc;
		if ((sc->initprot & minprot) != minprot) {
			continue;
		}
		const void *base = (const void *)((uintptr_t)macho->mh + sc->fileoff);
		const void *found = memmem(base, sc->filesize, data, size);
		if (found == NULL) {
			continue;
		}
		size_t offset = (uintptr_t)found - (uintptr_t)base;
		*addr = sc->vmaddr + offset;
		return MACHO_SUCCESS;
	}
}

macho_result
macho_search_data_64(const struct macho *macho, const void *data, size_t size,
		int minprot, uint64_t *addr) {
	assert(macho_is_64(macho));
	const struct load_command *lc = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command_64(macho, &lc, LC_SEGMENT_64);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (lc == NULL) {
			return MACHO_NOT_FOUND;
		}
		const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
		if ((sc->initprot & minprot) != minprot) {
			continue;
		}
		const void *base = (const void *)((uintptr_t)macho->mh + sc->fileoff);
		const void *found = memmem(base, sc->filesize, data, size);
		if (found == NULL) {
			continue;
		}
		size_t offset = (uintptr_t)found - (uintptr_t)base;
		*addr = sc->vmaddr + offset;
		return MACHO_SUCCESS;
	}
}

macho_result
macho_search_data(const struct macho *macho, const void *data, size_t size, int minprot,
		uint64_t *addr) {
	if (macho_is_32(macho)) {
		uint32_t addr32;
		macho_result mr = macho_search_data_32(macho, data, size, minprot, &addr32);
		if (mr == MACHO_SUCCESS) {
			*addr = addr32;
		}
		return mr;
	} else {
		return macho_search_data_64(macho, data, size, minprot, addr);
	}
}
