#ifndef MEMCTL__SYMBOL_TABLE_H_
#define MEMCTL__SYMBOL_TABLE_H_

#include "memctl/macho.h"
#include "memctl/memctl_types.h"

/*
 * struct symbol_table
 *
 * Description:
 * 	A symbol table mapping symbols to addresses.
 */
struct symbol_table {
	// The number of symbols.
	size_t        count;
	// The symbol names. These are in no particular order.
	char **       symbol;
	// The address of each symbol, in the same order as the symbol array.
	kaddr_t *     address;
	// The symbols above in lexicographical sorted order.
	size_t *      sort_symbol;
	// The addresses above in sorted order.
	size_t *      sort_address;
	// The end address of the symbol table. This is the address of the first byte that is not
	// part of the last symbol.
	kaddr_t       end_address;
};

/*
 * symbol_table_init_with_macho
 *
 * Description:
 * 	Initialize a symbol table with the given Mach-O file.
 *
 * Parameters:
 * 	out	st			The symbol table to initialize.
 * 		macho			The Mach-O containing the symbols. All data is copied from
 * 					the Mach-O, so the Mach-O may be freed after this function
 * 					returns.
 *
 * Returns:
 * 	True if no errors were encountered.
 *
 * TODO:
 * 	We need to be able to truncate symbols on internal segment/section boundaries.
 */
bool symbol_table_init_with_macho(struct symbol_table *st, const struct macho *macho);

/*
 * symbol_table_deinit
 *
 * Description:
 * 	Free all the resources used by a symbol table.
 *
 * Parameters:
 * 		st			The symbol table.
 */
void symbol_table_deinit(struct symbol_table *st);

/*
 * symbol_table_add_symbol
 *
 * Description:
 * 	Add a symbol to the symbol table.
 *
 * Parameters:
 * 		st			The symbol table to which a new symbol will be added.
 * 		symbol			The symbol name to add. This string is copied internally,
 * 					so it may be freed after this function returns.
 * 		address			The address of the symbol.
 *
 * Returns:
 * 	True if the symbol was successfully added.
 */
bool symbol_table_add_symbol(struct symbol_table *st, const char *symbol, kaddr_t address);

/*
 * symbol_table_resolve_symbol
 *
 * Description:
 * 	Resolve a symbol name into an address and size for the symbol.
 *
 * Parameters:
 * 		st			The symbol table.
 * 		symbol			The symbol name to resolve.
 * 	out	address			On return, the address of the symbol. May be NULL.
 * 	out	size			On return, the size of the symbol. May be NULL.
 *
 * Returns:
 * 	True if the symbol was successfully found.
 */
bool symbol_table_resolve_symbol(const struct symbol_table *st, const char *symbol,
		kaddr_t *address, size_t *size);

/*
 * symbol_table_resolve_address
 *
 * Description:
 * 	Resolve an address into a symbol name, size, and offset.
 *
 * Parameters:
 * 		st			The symbol table.
 * 		address			The address to resolve.
 * 	out	symbol			On return, the symbol name. The returned string points to
 * 					memory allocated by the symbol table, and must not be
 * 					referenced after symbol_table_deinit is called. May be
 * 					NULL.
 * 	out	size			On return, the size of the symbol, which is computed as the
 * 					number of bytes between the start of this symbol and the
 * 					start of the next one. May be NULL.
 * 	out	offset			On return, the offset of address from the beginning of the
 * 					symbol. May be NULL.
 */
bool symbol_table_resolve_address(const struct symbol_table *st, kaddr_t address,
		const char **symbol, size_t *size, size_t *offset);

#endif
