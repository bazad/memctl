#include "algorithm.h"

#include <unistd.h>

const void *
binary_search(const void *array, size_t width, size_t count,
		int (*compare)(const void *key, const void *element),
		const void *key, size_t *index) {
	ssize_t left  = 0;
	ssize_t right = count - 1;
	for (;;) {
		if (right < left) {
			if (index != NULL) {
				*index = left;
			}
			return NULL;
		}
		ssize_t mid = (left + right) / 2;
		void *element = (void *)((uintptr_t)array + mid * width);
		int cmp = compare(key, element);
		if (cmp < 0) {
			right = mid - 1;
		} else if (cmp > 0) {
			left = mid + 1;
		} else {
			// We assume that there's only one value that's equal.
			if (index != NULL) {
				*index = mid;
			}
			return element;
		}
	}
}

/*
 * struct compare_sorting_permutation_context
 *
 * Description:
 * 	Context for compare_sorting_permutation.
 */
struct compare_sorting_permutation_context {
	const void *array;
	size_t width;
	int (*compare)(const void *, const void *);
};

/*
 * compare_sorting_permutation
 *
 * Description:
 * 	A qsort_r comparator for sorting_permutation.
 */
static int
compare_sorting_permutation(void *context0, const void *a0, const void *b0) {
	struct compare_sorting_permutation_context *context = context0;
	size_t a_idx = *(size_t *)a0;
	size_t b_idx = *(size_t *)b0;
	const void *a = (const void *)((uintptr_t)context->array + a_idx * context->width);
	const void *b = (const void *)((uintptr_t)context->array + b_idx * context->width);
	return context->compare(a, b);
}

void sorting_permutation(const void *array, size_t width, size_t count,
		int (*compare)(const void *, const void *), size_t *permutation) {
	// Create an array of indices [0, 1, ..., count-1].
	for (size_t i = 0; i < count; i++) {
		permutation[i] = i;
	}
	// Sort the permutation array using the original array as keys.
	struct compare_sorting_permutation_context context = { array, width, compare };
	qsort_r(permutation, count, sizeof(*permutation), &context, compare_sorting_permutation);
}
