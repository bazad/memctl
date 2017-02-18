#ifndef MEMCTL__MACHO_H_
#define MEMCTL__MACHO_H_

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
 * 	inout	lc			The load command pointer. Set this to NULL to start
 * 					iterating. If lc is NULL on return, then all load commands
 * 					have been processed.
 *
 * TODO:
 * 	This should just return the new load command directly, this extra indirection is
 * 	unnecessary. The same goes for the similar functions below.
 */
void macho_next_load_command(const struct macho *macho, const struct load_command **lc);
void macho_next_load_command_32(const struct macho *macho, const struct load_command **lc);
void macho_next_load_command_64(const struct macho *macho, const struct load_command **lc);

/*
 * macho_find_load_command
 *
 * Description:
 * 	Iterate over load commands, skipping those that do not match the specified command type.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 	inout	lc			The load command pointer. Set this to NULL to start
 * 					iterating. If lc is NULL on return, then all load commands
 * 					have been processed.
 * 		cmd			The load command type to iterate over.
 *
 * Returns:
 * 	MACHO_SUCCESS or MACHO_NOT_FOUND.
 */
macho_result macho_find_load_command(const struct macho *macho, const struct load_command **lc,
		uint32_t cmd);
macho_result macho_find_load_command_32(const struct macho *macho, const struct load_command **lc,
		uint32_t cmd);
macho_result macho_find_load_command_64(const struct macho *macho, const struct load_command **lc,
		uint32_t cmd);

/*
 * macho_find_segment_command
 *
 * Description:
 * 	Find the segment command for the given Mach-O segment.
 *
 * Parameters:
 * 		macho			The macho struct.
 * 		segname			The name of the segment.
 *
 * Returns:
 * 	The segment command or NULL.
 */
const struct load_command *macho_find_segment_command(const struct macho *macho,
		const char *segname);
const struct segment_command *macho_find_segment_command_32(const struct macho *macho,
		const char *segname);
const struct segment_command_64 *macho_find_segment_command_64(const struct macho *macho,
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
const struct section *macho_find_section_32(const struct macho *macho,
		const struct segment_command *segment, const char *sectname);
const struct section_64 *macho_find_section_64(const struct macho *macho,
		const struct segment_command_64 *segment, const char *sectname);

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
void macho_segment_data_32(const struct macho *macho, const struct segment_command *segment,
		const void **data, uint32_t *addr, size_t *size);
void macho_segment_data_64(const struct macho *macho, const struct segment_command_64 *segment,
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
void macho_section_data_32(const struct macho *macho, const struct segment_command *segment,
		const struct section *section, const void **data, uint32_t *addr, size_t *size);
void macho_section_data_64(const struct macho *macho, const struct segment_command_64 *segment,
		const struct section_64 *section, const void **data, uint64_t *addr, size_t *size);

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
macho_result macho_find_base_32(struct macho *macho, uint32_t *base);
macho_result macho_find_base_64(struct macho *macho, uint64_t *base);

/*
 * macho_resolve_symbol
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_resolve_symbol(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, uint64_t *addr, size_t *size);
macho_result macho_resolve_symbol_32(const struct macho *macho,
		const struct symtab_command *symtab, const char *symbol, uint32_t *addr,
		size_t *size);
macho_result macho_resolve_symbol_64(const struct macho *macho,
		const struct symtab_command *symtab, const char *symbol, uint64_t *addr,
		size_t *size);

/*
 * macho_resolve_address
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_resolve_address(const struct macho *macho, const struct symtab_command *symtab,
		uint64_t addr, const char **name, size_t *size, size_t *offset);
macho_result macho_resolve_address_32(const struct macho *macho,
		const struct symtab_command *symtab, uint32_t addr, const char **name,
		size_t *size, size_t *offset);
macho_result macho_resolve_address_64(const struct macho *macho,
		const struct symtab_command *symtab, uint64_t addr, const char **name,
		size_t *size, size_t *offset);

/*
 * macho_search_data
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_search_data(const struct macho *macho, const void *data, size_t size,
		int minprot, uint64_t *addr);
macho_result macho_search_data_32(const struct macho *macho, const void *data, size_t size,
		int minprot, uint32_t *addr);
macho_result macho_search_data_64(const struct macho *macho, const void *data, size_t size,
		int minprot, uint64_t *addr);

#endif
