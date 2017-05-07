#ifndef MEMCTL__MACHO_H_
#define MEMCTL__MACHO_H_

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * struct macho
 *
 * Description:
 * 	A container for a pointer to a Mach-O file and its size.
 */
struct macho {
	union {
		void *mh;
		struct mach_header *mh32;
		struct mach_header_64 *mh64;
	};
	size_t size;
};

/*
 * enum macho_result
 *
 * Description:
 * 	Mach-O processing status codes.
 */
typedef enum macho_result {
	MACHO_SUCCESS,
	MACHO_ERROR,
	MACHO_NOT_FOUND,
} macho_result;

/*
 * macho_validate
 *
 * Description:
 * 	Validate that the given file is a Mach-O file.
 *
 * Parameters:
 * 		macho			A pointer to the file data.
 * 		size			The size of the data.
 *
 * Returns:
 * 	MACHO_SUCCESS on success, and MACHO_ERROR if validation failed.
 */
macho_result macho_validate(const void *mh, size_t size);
macho_result macho_validate_32(const struct mach_header *mh, size_t size);
macho_result macho_validate_64(const struct mach_header_64 *mh, size_t size);

/*
 * macho_is_32
 *
 * Description:
 * 	Returns true if the Mach-O file is a 32-bit Mach-O.
 */
bool macho_is_32(const struct macho *macho);

/*
 * macho_is_64
 *
 * Description:
 * 	Returns true if the Mach-O file is a 64-bit Mach-O.
 */
bool macho_is_64(const struct macho *macho);

/*
 * macho_header_size
 *
 * Description:
 * 	Returns the size of the Mach-O's mach header.
 */
size_t macho_header_size(const struct macho *macho);

/*
 * macho_next_load_command
 *
 * Description:
 * 	Iterate over load commands.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		lc			The load command pointer. Set this to NULL to start
 * 					at the first load command.
 *
 * Returns:
 * 	The next load command, or NULL if all load commands have been processed.
 */
const struct load_command *macho_next_load_command(const struct macho *macho,
		const struct load_command *lc);

/*
 * macho_find_load_command
 *
 * Description:
 * 	Iterate over load commands, skipping those that do not match the specified command type.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		lc			The current load command pointer. Set this to NULL to
 * 					return the first load command.
 * 		cmd			The load command type to iterate over.
 *
 * Returns:
 * 	The next load command, or NULL if all load commands of the given type have been processed.
 */
const struct load_command *macho_find_load_command(const struct macho *macho,
		const struct load_command *lc, uint32_t cmd);

/*
 * macho_next_segment
 *
 * Description:
 * 	Iterate over the segments of a Mach-O file.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		sc			The current segment command pointer. Set this to NULL to
 * 					return the first segment command.
 *
 * Returns:
 * 	The next segment command, or NULL if there are no more segment commands after sc.
 */
const struct load_command *macho_next_segment(const struct macho *macho,
		const struct load_command *sc);

/*
 * macho_find_segment
 *
 * Description:
 * 	Find the segment command for the given Mach-O segment name.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		segname			The name of the segment.
 *
 * Returns:
 * 	The segment command or NULL.
 */
const struct load_command *macho_find_segment(const struct macho *macho,
		const char *segname);

/*
 * macho_find_section
 *
 * Description:
 * 	Find the named section of the given segment of the Mach-O.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		segment			The segment command.
 * 		sectname		The section name.
 *
 * Returns:
 * 	The section or NULL.
 */
const void *macho_find_section(const struct macho *macho,
		const struct load_command *segment, const char *sectname);

/*
 * macho_segment_data
 *
 * Description:
 * 	Return the data contents of the given segment, including the virtual memory address and
 * 	size.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		segment			The segment command.
 * 	out	data			On return, a pointer to the contents of the segment.
 * 	out	addr			On return, the runtime address of the segment contents.
 * 	out	size			On return, the size of the segment contents.
 */
void macho_segment_data(const struct macho *macho, const struct load_command *segment,
		const void **data, uint64_t *addr, size_t *size);

/*
 * macho_section_data
 *
 * Description:
 * 	Return the data contents of the given section, including the virtual memory address and
 * 	size.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		segment			The segment command.
 * 		section			The section.
 * 	out	data			On return, a pointer to the contents of the section.
 * 	out	addr			On return, the runtime address of the section contents.
 * 	out	size			On return, the size of the section contents.
 */
void macho_section_data(const struct macho *macho, const struct load_command *segment,
		const void *section, const void **data, uint64_t *addr, size_t *size);

/*
 * macho_find_base
 *
 * Description:
 * 	Find the address at which the Mach-O file will map the Mach-O header into memory.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 	out	base			The static base address.
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_find_base(struct macho *macho, uint64_t *base);

/*
 * macho_resolve_symbol
 *
 * Description:
 * 	Resolve a symbol in a Mach-O file.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		symtab			The Mach-O symtab command.
 * 		symbol			The symbol to resolve.
 * 	out	addr			The address of the symbol.
 * 	out	size			A guess of the size of the symbol. This will only ever be
 * 					an overestimate.
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_resolve_symbol(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, uint64_t *addr, size_t *size);

/*
 * macho_resolve_address
 *
 * Description:
 * 	Resolve an address into a symbol.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		symtab			The Mach-O symtab command.
 * 		addr			The address to resolve.
 * 	out	name			The symbol name.
 * 	out	size			A guess of the size of the symbol. See
 * 					macho_resolve_symbol.
 * 	out	offset			The offset of addr into the symbol.
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_resolve_address(const struct macho *macho, const struct symtab_command *symtab,
		uint64_t addr, const char **name, size_t *size, size_t *offset);

/*
 * macho_search_data
 *
 * Description:
 * 	Search the data of the Mach-O file for a given byte sequence.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		data			The data to search for.
 * 		size			The number of bytes in data.
 * 		minprot			The minimum memory protections of the region.
 * 	out	addr			The virtual address of the data in the Mach-O.
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_search_data(const struct macho *macho, const void *data, size_t size,
		int minprot, uint64_t *addr);

#endif
