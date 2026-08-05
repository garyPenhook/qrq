#include "practice.h"

#include <limits.h>
#include <stdlib.h>

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

int qrq_review_queue_push(struct qrq_review_queue *queue, size_t item) {
	size_t *items;
	size_t capacity;

	if (queue == NULL) {
		return -1;
	}
	if (queue->count == queue->capacity) {
		if (queue->capacity > SIZE_MAX / 2 / sizeof(*queue->items)) {
			return -1;
		}
		capacity = queue->capacity == 0 ? 8 : queue->capacity * 2;
		items = realloc(queue->items, capacity * sizeof(*queue->items));
		if (items == NULL) {
			return -1;
		}
		queue->items = items;
		queue->capacity = capacity;
	}
	queue->items[queue->count++] = item;
	return 0;
}

int qrq_review_queue_take(struct qrq_review_queue *queue, size_t *item) {
	if (queue == NULL || item == NULL || queue->next >= queue->count) {
		return 0;
	}
	*item = queue->items[queue->next++];
	return 1;
}

void qrq_review_queue_clear(struct qrq_review_queue *queue) {
	if (queue == NULL) {
		return;
	}
	free(queue->items);
	queue->items = NULL;
	queue->count = 0;
	queue->next = 0;
	queue->capacity = 0;
}

int qrq_practice_accuracy(size_t attempts, size_t errors) {
	if (attempts == 0 || errors > attempts) {
		return 0;
	}
	return (int)(((long double)(attempts - errors) * 100.0L) /
			(long double)attempts);
}
