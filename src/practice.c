#include "practice.h"

#include <limits.h>

size_t qrq_practice_choose(size_t count, const unsigned char *used,
		const unsigned char *mistakes, int adaptive, uint32_t random_value) {
	size_t i;
	size_t total_weight = 0;
	size_t selected_weight;

	if (count == 0 || used == NULL) {
		return QRQ_PRACTICE_NO_ITEM;
	}
	for (i = 0; i < count; i++) {
		size_t weight;
		if (used[i] != 0) {
			continue;
		}
		weight = 1;
		if (adaptive && mistakes != NULL) {
			weight += mistakes[i] > 15 ? 15 : mistakes[i];
		}
		if (total_weight > SIZE_MAX - weight) {
			return QRQ_PRACTICE_NO_ITEM;
		}
		total_weight += weight;
	}
	if (total_weight == 0) {
		return QRQ_PRACTICE_NO_ITEM;
	}
	selected_weight = (size_t)random_value % total_weight;
	for (i = 0; i < count; i++) {
		size_t weight;
		if (used[i] != 0) {
			continue;
		}
		weight = 1;
		if (adaptive && mistakes != NULL) {
			weight += mistakes[i] > 15 ? 15 : mistakes[i];
		}
		if (selected_weight < weight) {
			return i;
		}
		selected_weight -= weight;
	}
	return QRQ_PRACTICE_NO_ITEM;
}
