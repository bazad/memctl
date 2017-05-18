#include "memctl/macho.h"

#include "memctl/memctl_error.h"

#include <assert.h>
#include <string.h>

#define MACHO_STRUCT_FIELD(macho, struct_type, object, field)		\
	(macho_is_64(macho) ? ((struct_type##_64 *)object)->field	\
	                    : ((struct_type *)object)->field)

#define MACHO_STRUCT_SIZE(macho, struct_type)				\
	(macho_is_64(macho) ? sizeof(struct_type##_64) : sizeof(struct_type))

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
	return MACHO_STRUCT_SIZE(macho, struct mach_header);
}

/*
 * macho_get_nlist
 */
static const void *
macho_get_nlist(const struct macho *macho, const struct symtab_command *symtab, uint32_t idx) {
	return (const void *)((uintptr_t)macho->mh + symtab->symoff
			+ idx * MACHO_STRUCT_SIZE(macho, struct nlist));
}

/*
 * macho_get_section_by_index
 *
 * Description:
 * 	Find the given section by index.
 */
static const void *
macho_get_section_by_index(const struct macho *macho, uint32_t sect) {
	if (sect < 1) {
		return NULL;
	}
	const struct load_command *lc = NULL;
	uint32_t idx = 1;
	uintptr_t sectcmd = 0;
	for (;;) {
		lc = macho_next_segment(macho, lc);
		if (lc == NULL) {
			break;
		}
		uint32_t nsects = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, nsects);
		if (sect < idx + nsects) {
			size_t lc_size = MACHO_STRUCT_SIZE(macho, struct segment_command);
			sectcmd = (uintptr_t)lc + lc_size;
			sectcmd += (sect - idx) * MACHO_STRUCT_SIZE(macho, struct section);
			break;
		}
		idx += nsects;
	}
	return (const void *)sectcmd;
}

static size_t
guess_symbol_size(const struct macho *macho, const struct symtab_command *symtab,
		uint32_t idx, uint64_t next) {
	const void *nl = macho_get_nlist(macho, symtab, idx);
	size_t size = -1;
	uint64_t n_value = MACHO_STRUCT_FIELD(macho, struct nlist, nl, n_value);
	uint32_t n_sect  = MACHO_STRUCT_FIELD(macho, struct nlist, nl, n_sect);
	if (next != -1) {
		size = next - n_value;
	}
	const void *sect = macho_get_section_by_index(macho, n_sect);
	if (sect != NULL) {
		uint64_t sect_addr = MACHO_STRUCT_FIELD(macho, struct section, sect, addr);
		size_t   sect_size = MACHO_STRUCT_FIELD(macho, struct section, sect, size);
		if (sect_addr <= n_value && n_value < sect_addr + sect_size) {
			size_t sect_limited_size = sect_addr + sect_size - n_value;
			if (sect_limited_size < size) {
				size = sect_limited_size;
			}
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

const struct load_command *
macho_next_load_command(const struct macho *macho, const struct load_command *lc) {
	if (lc == NULL) {
		lc = (const struct load_command *)((uintptr_t)macho->mh + macho_header_size(macho));
	} else {
		lc = (const struct load_command *)((uintptr_t)lc + lc->cmdsize);
	}
	size_t sizeofcmds = MACHO_STRUCT_FIELD(macho, struct mach_header, macho->mh, sizeofcmds);
	// TODO size_t sizeofcmds = (macho_is_32(macho) ? macho->mh32->sizeofcmds : macho->mh64->sizeofcmds);
	if ((uintptr_t)lc >= (uintptr_t)macho->mh + sizeofcmds) {
		lc = NULL;
	}
	return lc;
}

const struct load_command *
macho_find_load_command(const struct macho *macho, const struct load_command *lc, uint32_t cmd) {
	for (;;) {
		lc = macho_next_load_command(macho, lc);
		if (lc == NULL) {
			return NULL;
		}
		if (lc->cmd == cmd) {
			return lc;
		}
	}
}

const struct load_command *
macho_next_segment(const struct macho *macho, const struct load_command *sc) {
	const uint32_t cmd = (macho_is_64(macho) ? LC_SEGMENT_64 : LC_SEGMENT);
	return macho_find_load_command(macho, sc, cmd);
}

const struct load_command *
macho_find_segment(const struct macho *macho, const char *segname) {
	const struct load_command *lc = NULL;
	for (;;) {
		lc = macho_next_segment(macho, lc);
		if (lc == NULL) {
			return NULL;
		}
		const char *lc_segname = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, segname);
		if (strcmp(lc_segname, segname) != 0) {
			continue;
		}
		return lc;
	}
}

const void *
macho_find_section(const struct macho *macho, const struct load_command *segment,
		const char *sectname) {
	const size_t segment_size = MACHO_STRUCT_SIZE(macho, struct segment_command);
	const size_t section_size = MACHO_STRUCT_SIZE(macho, struct section);
	uintptr_t sect = (uintptr_t)segment + segment_size;
	size_t nsects = MACHO_STRUCT_FIELD(macho, struct segment_command, segment, nsects);
	uintptr_t end  = sect + nsects * section_size;
	for (; sect < end; sect += section_size) {
		const char *name = MACHO_STRUCT_FIELD(macho, struct section, sect, sectname);
		if (strcmp(name, sectname) == 0) {
			return (const void *)sect;
		}
	}
	return NULL;
}

void
macho_segment_data(const struct macho *macho, const struct load_command *segment,
		const void **data, uint64_t *addr, size_t *size) {
	size_t fileoff  = MACHO_STRUCT_FIELD(macho, struct segment_command, segment, fileoff);
	uint64_t vmaddr = MACHO_STRUCT_FIELD(macho, struct segment_command, segment, vmaddr);
	size_t vmsize   = MACHO_STRUCT_FIELD(macho, struct segment_command, segment, vmsize);
	*data = (const void *)((uintptr_t)macho->mh + fileoff);
	*addr = vmaddr;
	*size = vmsize;
}

void
macho_section_data(const struct macho *macho, const struct load_command *segment,
		const void *section, const void **data, uint64_t *addr, size_t *size) {
	uint64_t section_addr = MACHO_STRUCT_FIELD(macho, struct section, section, addr);
	size_t   section_size = MACHO_STRUCT_FIELD(macho, struct section, section, size);
	uint64_t segment_addr = MACHO_STRUCT_FIELD(macho, struct segment_command, segment, vmaddr);
	size_t   fileoff = MACHO_STRUCT_FIELD(macho, struct segment_command, segment, fileoff);
	uint64_t vmoff = section_addr - segment_addr;
	*data = (const void *)((uintptr_t)macho->mh + fileoff + vmoff);
	*addr = section_addr;
	*size = section_size;
}

macho_result
macho_find_base(struct macho *macho, uint64_t *base) {
	const struct load_command *lc = NULL;
	for (;;) {
		lc = macho_next_segment(macho, lc);
		if (lc == NULL) {
			return MACHO_NOT_FOUND;
		}
		size_t fileoff  = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, fileoff);
		size_t filesize = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, filesize);
		uint64_t vmaddr = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, vmaddr);
		if (fileoff != 0 || filesize == 0) {
			continue;
		}
		*base = vmaddr;
		return MACHO_SUCCESS;
	}
}

// TODO: Make this resilient to malformed images.
macho_result
macho_resolve_symbol(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, uint64_t *addr, size_t *size) {
	uint64_t strx = macho_string_index(macho, symtab, symbol);
	if (strx == 0) {
		return MACHO_NOT_FOUND;
	}
	uint64_t addr0 = 0;
	uint32_t symidx = -1;
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		const void *nl_i = macho_get_nlist(macho, symtab, i);
		uint64_t n_strx = MACHO_STRUCT_FIELD(macho, struct nlist, nl_i, n_un.n_strx);
		if (n_strx == strx) {
			uint8_t n_type = MACHO_STRUCT_FIELD(macho, struct nlist, nl_i, n_type);
			if ((n_type & N_TYPE) == N_UNDF) {
				return MACHO_NOT_FOUND;
			}
			if ((n_type & N_TYPE) != N_SECT) {
				error_macho("unexpected Mach-O symbol type %x for symbol %s",
						n_type & N_TYPE, symbol);
				return MACHO_ERROR;
			}
			addr0 = MACHO_STRUCT_FIELD(macho, struct nlist, nl_i, n_value);
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
			const void *nl_i = macho_get_nlist(macho, symtab, i);
			uint64_t n_value = MACHO_STRUCT_FIELD(macho, struct nlist, nl_i, n_value);
			if (n_value > addr0 && n_value < next) {
				next = n_value;
			}
		}
		*size = guess_symbol_size(macho, symtab, symidx, next);
	}
	return MACHO_SUCCESS;
}

macho_result
macho_resolve_address(const struct macho *macho, const struct symtab_command *symtab,
		uint64_t addr, const char **name, size_t *size, size_t *offset) {
	const void *sym = NULL;
	uint32_t symidx;
	uint64_t sym_addr;
	uint64_t next_addr = -1;
	for (uint32_t i = 0; i < symtab->nsyms; i++) {
		const void *nl_i = macho_get_nlist(macho, symtab, i);
		uint8_t n_type = MACHO_STRUCT_FIELD(macho, struct nlist, nl_i, n_type);
		if ((n_type & N_TYPE) != N_SECT) {
			continue; // TODO: Handle other symbol types.
		}
		uint64_t n_value = MACHO_STRUCT_FIELD(macho, struct nlist, nl_i, n_value);
		if ((sym == NULL || sym_addr < n_value) && n_value <= addr) {
			sym = nl_i;
			symidx = i;
			sym_addr = n_value;
		} else if (addr < n_value && n_value <= next_addr) {
			next_addr = n_value;
		}
	}
	if (sym == NULL) {
		return MACHO_NOT_FOUND;
	}
	uint32_t sym_sect = MACHO_STRUCT_FIELD(macho, struct nlist, sym, n_sect);
	if (sym_sect == NO_SECT) {
		error_macho("symbol index %d has no section", symidx);
		return MACHO_ERROR;
	}
	uint64_t sym_strx = MACHO_STRUCT_FIELD(macho, struct nlist, sym, n_un.n_strx);
	*name = (const char *)((uintptr_t)macho->mh + symtab->stroff + sym_strx);
	*size = guess_symbol_size(macho, symtab, symidx, next_addr);
	*offset = addr - sym_addr;
	return MACHO_SUCCESS;
}

// TODO: Make this resilient to malformed images.
macho_result
macho_search_data(const struct macho *macho, const void *data, size_t size, int minprot,
		uint64_t *addr) {
	const struct load_command *lc = NULL;
	for (;;) {
		lc = macho_next_segment(macho, lc);
		if (lc == NULL) {
			return MACHO_NOT_FOUND;
		}
		int initprot = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, initprot);
		if ((initprot & minprot) != minprot) {
			continue;
		}
		size_t fileoff  = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, fileoff);
		size_t filesize = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, filesize);
		const void *base = (const void *)((uintptr_t)macho->mh + fileoff);
		const void *found = memmem(base, filesize, data, size);
		if (found == NULL) {
			continue;
		}
		size_t offset = (uintptr_t)found - (uintptr_t)base;
		uint64_t vmaddr = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, vmaddr);
		*addr = vmaddr + offset;
		return MACHO_SUCCESS;
	}
}

const struct load_command *
macho_segment_containing_address(const struct macho *macho, uint64_t addr) {
	const struct load_command *lc = NULL;
	for (;;) {
		lc = macho_next_segment(macho, lc);
		if (lc == NULL) {
			return NULL;
		}
		uint64_t vmaddr = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, vmaddr);
		size_t   vmsize = MACHO_STRUCT_FIELD(macho, struct segment_command, lc, vmsize);
		if (vmaddr <= addr && addr < vmaddr + vmsize) {
			return lc;
		}
	}
}
