#include "memctl/symbol_table.h"

#include "memctl/memctl_error.h"
#include "memctl/utility.h"

#include "algorithm.h"

#include <assert.h>
#include <string.h>

/*
 * sort_order
 *
 * Description:
 * 	Get a newly allocated sorting permutation for an array.
 */
static size_t *
sort_order(const void *array, size_t width, size_t count,
		int (*compare)(const void *, const void *)) {
	size_t *order = malloc(count * sizeof(*order));
	if (order != NULL) {
		sorting_permutation(array, width, count, compare, order);
	}
	return order;
}

/*
 * count_symbol
 *
 * Description:
 * 	A macho_for_each_symbol_fn callback for count_symbols.
 */
static bool
count_symbol(void *context0, const char *symbol, uint64_t address) {
	size_t *count = context0;
	++*count;
	return false;
}

/*
 * count_symbols
 *
 * Description:
 * 	Return the number of symbols in the Mach-O symtab.
 */
static size_t
count_symbols(const struct macho *macho, const struct symtab_command *symtab) {
	size_t count = 0;
	macho_for_each_symbol(macho, symtab, count_symbol, &count);
	return count;
}

/*
 * struct collect_symbol_context
 *
 * Description:
 * 	Context for collect_symbol and symbol_table_init_with_macho.
 */
struct collect_symbol_context {
	struct symbol_table *st;
	size_t capacity;
	bool out_of_memory;
};

/*
 * collect_symbol
 *
 * Description:
 * 	A macho_for_each_symbol_fn for symbol_table_init_with_macho.
 */
static bool
collect_symbol(void *context0, const char *symbol, uint64_t address) {
	struct collect_symbol_context *context = context0;
	struct symbol_table *st = context->st;
	assert(st->count < context->capacity);
	// Duplicate the symbol string.
	char *new_symbol = strdup(symbol);
	if (new_symbol == NULL) {
		context->out_of_memory = true;
		return true;
	}
	// Insert this new symbol into the arrays.
	st->symbol[st->count]  = new_symbol;
	st->address[st->count] = address;
	st->count++;
	return (st->count == context->capacity);
}

/*
 * compare_symbols
 *
 * Description:
 * 	Compare symbols in the symbol array for symbol_table_init_with_macho.
 */
static int
compare_symbols(const void *a0, const void *b0) {
	const char *const *a = a0;
	const char *const *b = b0;
	return strcmp(*a, *b);
}

/*
 * compare_addresses
 *
 * Description:
 * 	Compare addresses in the address array for symbol_table_init_with_macho.
 */
static int
compare_addresses(const void *a0, const void *b0) {
	kaddr_t a = *(const kaddr_t *)a0;
	kaddr_t b = *(const kaddr_t *)b0;
	if (a < b) {
		return -1;
	} else if (a > b) {
		return 1;
	}
	return 0;
}

/*
 * collect_segments
 *
 * Description:
 * 	Collect segment information into the symbol table.
 */
static bool
collect_segments(struct symbol_table *st, const struct macho *macho) {
	// Count the number of segments.
	const struct load_command *sc = NULL;
	size_t count = 0;
	for (;; count++) {
		sc = macho_next_segment(macho, sc);
		if (sc == NULL) {
			break;
		}
	}
	// Allocate the arrays.
	st->segment = malloc(2 * count * sizeof(*st->segment));
	if (st->segment == NULL) {
		return false;
	}
	// Collect the segment information.
	sc = NULL;
	for (size_t i = 0;; i++) {
		sc = macho_next_segment(macho, sc);
		if (sc == NULL) {
			assert(i == count);
			st->segment_count = i;
			break;
		}
		assert(i < count);
		uint64_t address;
		size_t size;
		macho_segment_data(macho, sc, NULL, &address, &size);
		st->segment[2 * i]     = address;
		st->segment[2 * i + 1] = address + size;
	}
	return true;
}

bool
symbol_table_init_with_macho(struct symbol_table *st, const struct macho *macho) {
	// Default-initialize.
	st->count         = 0;
	st->symbol        = NULL;
	st->address       = NULL;
	st->sort_symbol   = NULL;
	st->sort_address  = NULL;
	st->segment       = NULL;
	st->segment_count = 0;
	// Get the segments.
	if (!collect_segments(st, macho)) {
		goto out_of_memory;
	}
	// Get the symbol table.
	const struct symtab_command *symtab = (const struct symtab_command *)
		macho_find_load_command(macho, NULL, LC_SYMTAB);
	if (symtab == NULL) {
		return true;
	}
	// Create arrays of the requisite capacity.
	size_t count = count_symbols(macho, symtab);
	st->symbol  = malloc(count * sizeof(*st->symbol));
	st->address = malloc(count * sizeof(*st->address));
	if (st->symbol == NULL || st->address == NULL) {
		goto out_of_memory;
	}
	// Collect the symbols and addresses.
	struct collect_symbol_context context = { st, count, false };
	macho_for_each_symbol(macho, symtab, collect_symbol, &context);
	if (context.out_of_memory) {
		goto out_of_memory;
	}
	assert(st->count == count);
	// Get the lexicographical sort order of the symbols and the numerical sort order of the
	// addresses.
	st->sort_symbol  = sort_order(st->symbol,  sizeof(*st->symbol),  count, compare_symbols);
	st->sort_address = sort_order(st->address, sizeof(*st->address), count, compare_addresses);
	if (st->sort_symbol == NULL || st->sort_address == NULL) {
		goto out_of_memory;
	}
	// All done.
	return true;
out_of_memory:
	error_out_of_memory();
	symbol_table_deinit(st);
	return false;
}

void
symbol_table_deinit(struct symbol_table *st) {
	if (st->symbol != NULL) {
		for (size_t i = 0; i < st->count; i++) {
			free(st->symbol[i]);
		}
		free(st->symbol);
		st->symbol = NULL;
	}
	st->count = 0;
	if (st->address != NULL) {
		free(st->address);
		st->address = NULL;
	}
	if (st->sort_symbol != NULL) {
		free(st->sort_symbol);
		st->sort_symbol = NULL;
	}
	if (st->sort_address != NULL) {
		free(st->sort_address);
		st->sort_address = NULL;
	}
	if (st->segment != NULL) {
		free(st->segment);
		st->segment = NULL;
	}
	st->segment_count = 0;
}

// A sentinel value indicating that the index was not found.
#define NOT_FOUND ((size_t)(-1))

/*
 * struct compare_sort_symbol_key
 *
 * Description:
 * 	A key and context for compare_sort_symbol.
 */
struct compare_sort_symbol_key {
	const char *symbol;
	char **symbols;
};

/*
 * compare_sort_symbol
 *
 * Description:
 * 	Compare a key to an element in the sort_symbol array.
 */
static int
compare_sort_symbol(const void *key0, const void *element) {
	const struct compare_sort_symbol_key *key = key0;
	size_t symbol_index = *(const size_t *)element;
	return compare_symbols(&key->symbol, &key->symbols[symbol_index]);
}

/*
 * find_index_of_symbol
 *
 * Description:
 * 	Find the index in the symbol/address tables corresponding to the given symbol. If index is
 * 	not NULL, then the index in the sort_symbol table (at which the symbol could be inserted)
 * 	is also returned.
 */
static size_t
find_index_of_symbol(const struct symbol_table *st, const char *symbol,
		size_t *sort_symbol_index) {
	struct compare_sort_symbol_key key = { symbol, st->symbol };
	const size_t *symbol_index = binary_search(st->sort_symbol, sizeof(*st->sort_symbol),
			st->count, compare_sort_symbol, &key, sort_symbol_index);
	return (symbol_index == NULL ? NOT_FOUND : *symbol_index);
}

/*
 * struct compare_sort_address_key
 *
 * Description:
 * 	A key and context for compare_sort_address.
 */
struct compare_sort_address_key {
	kaddr_t address;
	const kaddr_t *addresses;
};

/*
 * compare_sort_address
 *
 * Description:
 * 	Compare a key to an element in the sort_address array.
 */
static int
compare_sort_address(const void *key0, const void *element) {
	const struct compare_sort_address_key *key = key0;
	size_t address_index = *(const size_t *)element;
	return compare_addresses(&key->address, &key->addresses[address_index]);
}

/*
 * find_index_of_address
 *
 * Description:
 * 	Find the index in the symbol/address tables corresponding to the given address. If index is
 * 	not NULL, then the index in the sort_address table (at which the address could be inserted)
 * 	is also returned.
 */
static size_t
find_index_of_address(const struct symbol_table *st, kaddr_t address, size_t *sort_address_index) {
	struct compare_sort_address_key key = { address, st->address };
	const size_t *address_index = binary_search(st->sort_address, sizeof(*st->sort_address),
			st->count, compare_sort_address, &key, sort_address_index);
	return (address_index == NULL ? NOT_FOUND : *address_index);
}

/*
 * grow_arrays
 *
 * Description:
 * 	Grow all the arrays.
 */
static bool
grow_arrays(struct symbol_table *st, size_t count) {
	char **symbol = realloc(st->symbol, count * sizeof(*st->symbol));
	if (symbol == NULL) {
		return false;
	}
	st->symbol = symbol;
	kaddr_t *address = realloc(st->address, count * sizeof(*st->address));
	if (address == NULL) {
		return false;
	}
	st->address = address;
	size_t *sort_symbol = realloc(st->sort_symbol, count * sizeof(*st->sort_symbol));
	if (sort_symbol == NULL) {
		return false;
	}
	st->sort_symbol = sort_symbol;
	size_t *sort_address = realloc(st->sort_address, count * sizeof(*st->sort_address));
	if (sort_address == NULL) {
		return false;
	}
	st->sort_address = sort_address;
	return true;
}

bool
symbol_table_add_symbol(struct symbol_table *st, const char *symbol, kaddr_t address) {
	// Get the index of the symbol, in case it is already present.
	size_t sort_symbol_index;
	size_t symbol_index = find_index_of_symbol(st, symbol, &sort_symbol_index);
	if (symbol_index != NOT_FOUND) {
		error_internal("symbol '%s' already present in symbol table", symbol);
		return false;
	}
	// Grow all the arrays.
	size_t count = st->count;
	if (!grow_arrays(st, count + 1)) {
		goto out_of_memory;
	}
	// Duplicate the symbol string.
	char *new_symbol = strdup(symbol);
	if (new_symbol == NULL) {
		goto out_of_memory;
	}
	// Append the symbol to the end of the symbol and address arrays.
	st->symbol[count]  = new_symbol;
	st->address[count] = address;
	// Insert the new index in the proper sorted position in the sort_symbol array.
	memmove(&st->sort_symbol[sort_symbol_index + 1], &st->sort_symbol[sort_symbol_index],
			(count - sort_symbol_index) * sizeof(*st->sort_symbol));
	st->sort_symbol[sort_symbol_index] = count;
	// Insert the new index in the proper sorted position in the sort_address array.
	size_t sort_address_index;
	find_index_of_address(st, address, &sort_address_index);
	memmove(&st->sort_address[sort_address_index + 1], &st->sort_address[sort_address_index],
			(count - sort_address_index) * sizeof(*st->sort_address));
	st->sort_address[sort_address_index] = count;
	// All done. Increment count (after all of the find_index_of_* calls, which use count).
	st->count = count + 1;
	return true;
out_of_memory:
	error_out_of_memory();
	return false;
}

/*
 * find_segment_containing_address
 *
 * Description:
 * 	Find the bounds of the section containing the given address.
 */
static bool
find_segment_containing_address(const struct symbol_table *st, kaddr_t address, kaddr_t *end) {
	for (size_t i = 0; i < st->segment_count; i++) {
		if (st->segment[2 * i] <= address && address < st->segment[2 * i + 1]) {
			*end = st->segment[2 * i + 1];
			return true;
		}
	}
	return false;
}

/*
 * find_symbol_end_address
 *
 * Description:
 * 	Find the address of the end of the symbol. This is usually the address of the next symbol,
 * 	skipping any symbols that share this address.
 */
static kaddr_t
find_symbol_end_address(const struct symbol_table *st, kaddr_t address, kaddr_t segment_end) {
	// Find the insertion index in the sort_address array of `address + 1`. This will skip over
	// any symbols that share the same address, and either put sort_address_index right on a
	// next symbol, or put us off the end of the array.
	size_t sort_address_index;
	find_index_of_address(st, address + 1, &sort_address_index);
	assert(sort_address_index <= st->count);
	if (sort_address_index == st->count) {
		// This is the last address. The end of the symbol is the end of the containing
		// segment.
		return segment_end;
	}
	assert(st->sort_address[sort_address_index] < st->count);
	kaddr_t next = st->address[st->sort_address[sort_address_index]];
	assert(next > address);
	return min(next, segment_end);
}

bool
symbol_table_resolve_symbol(const struct symbol_table *st, const char *symbol,
		kaddr_t *address, size_t *size) {
	size_t index = find_index_of_symbol(st, symbol, NULL);
	if (index == NOT_FOUND) {
		return false;
	}
	kaddr_t start = st->address[index];
	if (address != NULL) {
		*address = start;
	}
	if (size != NULL) {
		kaddr_t segment_end;
		bool found = find_segment_containing_address(st, start, &segment_end);
		assert(found);
		kaddr_t end = find_symbol_end_address(st, start, segment_end);
		*size = end - start;
	}
	return true;
}

bool
symbol_table_resolve_address(const struct symbol_table *st, kaddr_t address,
		const char **symbol, size_t *size, size_t *offset) {
	kaddr_t segment_end;
	bool found = find_segment_containing_address(st, address, &segment_end);
	if (!found) {
		// The address is not contained in any segment.
		return false;
	}
	// Get the index of the symbol containing the address.
	size_t sort_address_index;
	size_t index = find_index_of_address(st, address, &sort_address_index);
	if (index == NOT_FOUND) {
		// No symbol is at that exact address, but we have an insertion index, which means
		// the address before it is the symbol containing this address.
		if (sort_address_index == 0) {
			// We'd have to insert at index 0, meaning this address comes before all
			// symbols. No match.
			return false;
		}
		index = st->sort_address[sort_address_index - 1];
	}
	assert(index < st->count);
	// Extract the symbol name, size, and the offset of the address from the start.
	if (symbol != NULL) {
		*symbol = st->symbol[index];
	}
	kaddr_t start = st->address[index];
	if (size != NULL) {
		kaddr_t end = find_symbol_end_address(st, address, segment_end);
		assert(start <= address && address < end && end <= segment_end);
		*size = end - start;
	}
	if (offset != NULL) {
		*offset = address - start;
	}
	return true;
}
