#ifndef MEMCTL__ALGORITHM_H_
#define MEMCTL__ALGORITHM_H_

#include <stdlib.h>

/*
 * binary_search
 *
 * Description:
 * 	Search for an element in a sorted array matching a given key.
 *
 * Parameters:
 * 		array			The array to search.
 * 		width			The width of each element of the array in bytes.
 * 		count			The number of elements in the array.
 * 		compare			A comparison function. The first argument is the key to
 * 					search for (the same as passed to binary_search). The
 * 					second argument is a pointer to the current element in the
 * 					array. The returned integer should be negative, positive,
 * 					or zero if the key is found to be less than, greater than,
 * 					or equal to the element, respectively.
 * 		key			The value to search for. This value is not used by
 * 					binary_search, so its structure may differ from the
 * 					elements of the array and it may be used to carry
 * 					additional context for compare.
 * 	out	index			On return, the index of the match. If no element compares
 * 					equal to key, then this is the index at which the key can
 * 					be inserted into the array. May be NULL.
 *
 * Returns:
 * 	If the key compares equal to an element in the array, a pointer to one such element in the
 * 	array is returned. (There is no guarantee that this is the first matching element if the
 * 	array contains multiple matches.) Otherwise, NULL is returned.
 */
const void *binary_search(const void *array, size_t width, size_t count,
		int (*compare)(const void *key, const void *element),
		const void *key, size_t *index);

/*
 * sorting_permutation
 *
 * Description:
 * 	Get the sort order for an array of values. This is a new array order consisting of the
 * 	integers { 0, 1, ..., count-1 } sorted according to the values in the array. More
 * 	concretely, { array[order[0]], array[order[1]], ..., array[order[count-1]] } is the sorted
 * 	version of the original array.
 *
 * Parameters:
 * 		array			The array of values.
 * 		width			The width of each element of the array in bytes.
 * 		count			The number of elements in the array.
 * 		compare			A comparison function for values in the array. The two
 * 					arguments are pointers to elements in the array. The
 * 					returned integer should be negative, positive, or zero if
 * 					the first element is found to be less than, greater than,
 * 					or equal to the second element, respectively.
 * 	out	order			On return, the sorting order of the array, which is a
 * 					permutation of the indices 0, 1, ..., count-1 such that
 * 					reading the array in that order produces a sorted array.
 * 					The array must have at least count elements.
 */
void sorting_permutation(const void *array, size_t width, size_t count,
		int (*compare)(const void *, const void *), size_t *permutation);

#endif
