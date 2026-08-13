#include "practice.h"

#include <limits.h>
#include <stdlib.h>

static size_t selection_weight(const unsigned char *mistakes, int adaptive,
		const unsigned char *due, int spaced_repetition, size_t index) {
	size_t weight = 1;

	if (adaptive && mistakes != NULL) {
		weight += mistakes[index] > 15 ? 15 : mistakes[index];
	}
	if (spaced_repetition && due != NULL) {
		weight += (due[index] > 3 ? 3 : due[index]) * 4;
	}
	return weight;
}

size_t qrq_practice_choose_scheduled(size_t count, const unsigned char *used,
		const unsigned char *mistakes, int adaptive, const unsigned char *due,
		int spaced_repetition, uint32_t random_value) {
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
		weight = selection_weight(mistakes, adaptive, due, spaced_repetition, i);
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
		weight = selection_weight(mistakes, adaptive, due, spaced_repetition, i);
		if (selected_weight < weight) {
			return i;
		}
		selected_weight -= weight;
	}
	return QRQ_PRACTICE_NO_ITEM;
}

size_t qrq_practice_choose(size_t count, const unsigned char *used,
		const unsigned char *mistakes, int adaptive, uint32_t random_value) {
	return qrq_practice_choose_scheduled(count, used, mistakes, adaptive, NULL,
			0, random_value);
}

size_t qrq_practice_answer_batch_size(size_t remaining, int configured_batch) {
	size_t batch = configured_batch < 1 ? 1U : (size_t)configured_batch;

	if (batch > QRQ_PRACTICE_MAX_ANSWER_BATCH) {
		batch = QRQ_PRACTICE_MAX_ANSWER_BATCH;
	}
	return batch < remaining ? batch : remaining;
}

int qrq_practice_record_result(size_t count, unsigned char *used,
		unsigned char *mistakes, size_t item, int copied_correctly, int adaptive) {
	if (used == NULL || mistakes == NULL || item >= count) {
		return -1;
	}
	if (copied_correctly) {
		used[item] = 1;
		return 0;
	}
	if (mistakes[item] != UCHAR_MAX) {
		mistakes[item]++;
	}
	used[item] = adaptive ? 0 : 1;
	return 0;
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

int qrq_practice_session_eligible(int attempt_valid, size_t completed,
		size_t requested, int accuracy, int accuracy_target) {
	if (!attempt_valid || requested == 0 || completed != requested ||
			accuracy < 0 || accuracy > 100 || accuracy_target < 0 ||
			accuracy_target > 100) {
		return 0;
	}
	return accuracy_target == 0 || accuracy >= accuracy_target;
}

int qrq_practice_sustained_goal_active(int speed_target, int duration_seconds) {
	return speed_target > 0 && duration_seconds > 0 &&
			duration_seconds <= QRQ_PRACTICE_SUSTAINED_GOAL_MAX_SECONDS;
}

int qrq_practice_sustained_goal_expired(int speed_target, int duration_seconds,
		uint64_t elapsed_milliseconds) {
	uint64_t duration_milliseconds;

	if (!qrq_practice_sustained_goal_active(speed_target, duration_seconds)) {
		return 0;
	}
	duration_milliseconds = (uint64_t)duration_seconds * UINT64_C(1000);
	return elapsed_milliseconds >= duration_milliseconds;
}

int qrq_practice_sustained_goal_met(int speed_target, int duration_seconds,
		uint64_t elapsed_milliseconds, int speed_violated) {
	return !speed_violated && qrq_practice_sustained_goal_expired(speed_target,
			duration_seconds, elapsed_milliseconds);
}
