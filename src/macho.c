#include "macho.h"

#include "memctl_error.h"

#include <string.h>

// TODO: Make this resilient to malformed images.
/*
 * macho_get_nlist
 */
static const struct macho_nlist *
macho_get_nlist(const struct macho *macho, const struct symtab_command *symtab) {
	return (const struct macho_nlist *)((uintptr_t)macho->mh + symtab->symoff);
}

// TODO: Make this resilient to malformed images.
/*
 * macho_get_section
 *
 * Description:
 * 	Find the given section in the Mach-O image.
 */
static const struct macho_section *
macho_get_section(const struct macho *macho, uint32_t sect) {
	if (sect < 1) {
		return NULL;
	}
	const struct load_command *lc = NULL;
	uint32_t idx = 1;
	const struct macho_section *sectcmd = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command(macho, &lc, MACHO_LC_SEGMENT);
		if (mr != MACHO_SUCCESS || lc == NULL) {
			break;
		}
		const struct macho_segment_command *sc = (const struct macho_segment_command *)lc;
		if (sect < idx + sc->nsects) {
			sectcmd = (const struct macho_section *) ((uintptr_t)sc + sizeof(*sc));
			sectcmd += sect - idx;
			break;
		}
		idx += sc->nsects;
	}
	return sectcmd;
}

// TODO: Make this resilient to malformed images.
/*
 * guess_symbol_size
 *
 * Description:
 * 	Take a best guess of the symbol's size given the address of the next symbol.
 */
static size_t
guess_symbol_size(const struct macho *macho, const struct symtab_command *symtab,
		uint32_t idx, macho_word next) {
	const struct macho_nlist *nl = macho_get_nlist(macho, symtab) + idx;
	size_t size = -1;
	if (next != -1) {
		size = next - nl->n_value;
	}
	const struct macho_section *sect = macho_get_section(macho, nl->n_sect);
	if (sect != NULL
	    && sect->addr <= nl->n_value
	    && nl->n_value < sect->addr + sect->size) {
		size_t sect_size = sect->addr + sect->size - nl->n_value;
		if (sect_size < size) {
			size = sect_size;
		}
	}
	// TODO: Use segment size if section size failed.
	return (size == -1 ? sizeof(macho_word) : size);
}

// TODO: Make this resilient to malformed images.
/*
 * macho_string_index
 *
 * Description:
 * 	Find the index of the string in the string table.
 */
static macho_word
macho_string_index(const struct macho *macho, const struct symtab_command *symtab,
		const char *name) {
	uintptr_t base = (uintptr_t)macho->mh + symtab->stroff;
	const char *str = (const char *)(base + 4);
	const char *end = (const char *)(base + symtab->strsize);
	macho_word strx;
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

// TODO: Make this resilient to malformed images.
macho_result
macho_validate(const void *macho, size_t size) {
	const struct macho_header *mh = macho;
	if (size < sizeof(*mh)) {
		error_macho("Mach-O too small");
		return MACHO_ERROR;
	}
	if (mh->magic != MACHO_MAGIC) {
		error_macho("Mach-O invalid magic");
		return MACHO_ERROR;
	}
	if (mh->sizeofcmds > size) {
		error_macho("Mach-O sizeofcmds greater than file size");
		return MACHO_ERROR;
	}
	// TODO: Validate commands.
	return MACHO_SUCCESS;
}

// TODO: Make this resilient to malformed images.
macho_result
macho_next_load_command(const struct macho *macho, const struct load_command **lc) {
	const struct load_command *lc0 = *lc;
	const struct macho_header *mh = macho->mh;
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

// TODO: Make this resilient to malformed images.
macho_result
macho_find_load_command(const struct macho *macho, const struct load_command **lc, uint32_t cmd) {
	for (;;) {
		macho_result mr = macho_next_load_command(macho, lc);
		if (mr != MACHO_SUCCESS || *lc == NULL) {
			return mr;
		}
		if ((*lc)->cmd == cmd) {
			return MACHO_SUCCESS;
		}
	}
}

macho_result
macho_find_base(struct macho *macho, macho_word *base) {
	const struct macho_segment_command *sc = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command(macho, (const struct load_command **)&sc,
				MACHO_LC_SEGMENT);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (sc == NULL) {
			return MACHO_NOT_FOUND;
		}
		if (sc->fileoff != 0) {
			continue;
		}
		*base = sc->vmaddr;
		return MACHO_SUCCESS;
	}
}

// TODO: Make this resilient to malformed images.
macho_result
macho_resolve_symbol(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, macho_word *addr, size_t *size) {
	macho_word strx = macho_string_index(macho, symtab, symbol);
	if (strx == 0) {
		return MACHO_NOT_FOUND;
	}
	macho_word addr0 = 0;
	const struct macho_nlist *nl = macho_get_nlist(macho, symtab);
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
		macho_word next = -1;
		for (uint32_t i = 0; i < symtab->nsyms; i++) {
			if (nl[i].n_value > addr0 && nl[i].n_value < next) {
				next = nl[i].n_value;
			}
		}
		*size = guess_symbol_size(macho, symtab, symidx, next);
	}
	return MACHO_SUCCESS;
}

// TODO: Make this resilient to malformed images.
macho_result
macho_resolve_address(const struct macho *macho, const struct symtab_command *symtab,
		macho_word addr, const char **name, size_t *size, size_t *offset) {
	const struct macho_nlist *nl = macho_get_nlist(macho, symtab);
	uint32_t symidx = -1;
	macho_word next = -1;
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
	*size = guess_symbol_size(macho, symtab, symidx, next);
	*offset = addr - nl[symidx].n_value;
	return MACHO_SUCCESS;
}

// TODO: Make this resilient to malformed images.
macho_result
macho_search_data(const struct macho *macho, const void *data, size_t size,
		int minprot, macho_word *addr) {
	const struct load_command *lc = NULL;
	for (;;) {
		macho_result mr = macho_find_load_command(macho, &lc, MACHO_LC_SEGMENT);
		if (mr != MACHO_SUCCESS) {
			return mr;
		}
		if (lc == NULL) {
			return MACHO_NOT_FOUND;
		}
		const struct macho_segment_command *sc = (const struct macho_segment_command *)lc;
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
