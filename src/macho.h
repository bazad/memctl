#ifndef MEMCTL__MACHO_H_
#define MEMCTL__MACHO_H_

#include "memctl_types.h"

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef MACHO_BITS
#define MACHO_BITS 64
#endif

#if MACHO_BITS == 32
# define macho_header		mach_header
# define MACHO_MAGIC		MH_MAGIC
# define MACHO_CIGAM		MH_CIGAM
# define macho_word		uint32_t
# define macho_segment_command	segment_command
# define macho_section		section
# define MACHO_LC_SEGMENT	LC_SEGMENT
# define macho_nlist		nlist
#elif MACHO_BITS == 64
# define macho_header		mach_header_64
# define MACHO_MAGIC		MH_MAGIC_64
# define MACHO_CIGAM		MH_CIGAM_64
# define macho_word		uint64_t
# define macho_segment_command	segment_command_64
# define macho_section		section_64
# define MACHO_LC_SEGMENT	LC_SEGMENT_64
# define macho_nlist		nlist_64
#else
# error Unsupported Mach-O format.
#endif

/*
 * struct macho
 *
 * Description:
 * 	A container for a pointer to a Mach-O file and its size.
 */
struct macho {
	struct macho_header *mh;
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
macho_result macho_validate(const void *macho, size_t size);

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
 * Returns:
 * 	MACHO_SUCCESS or MACHO_ERROR.
 */
macho_result macho_next_load_command(const struct macho *macho, const struct load_command **lc);

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
 * 	MACHO_SUCCESS or MACHO_ERROR.
 */
macho_result macho_find_load_command(const struct macho *macho, const struct load_command **lc,
		uint32_t cmd);

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
macho_result macho_find_base(struct macho *macho, macho_word *base);

/*
 * macho_resolve_symbol
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_resolve_symbol(const struct macho *macho, const struct symtab_command *symtab,
		const char *symbol, macho_word *addr, size_t *size);

/*
 * macho_resolve_address
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_resolve_address(const struct macho *macho, const struct symtab_command *symtab,
		macho_word addr, const char **name, size_t *size, size_t *offset);

/*
 * macho_search_data
 *
 * Returns:
 * 	A macho_result status code.
 */
macho_result macho_search_data(const struct macho *macho, const void *data, size_t size,
		int minprot, macho_word *addr);

#endif
