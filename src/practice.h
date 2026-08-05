#ifndef QRQ_PRACTICE_H
#define QRQ_PRACTICE_H

#include <stddef.h>
#include <stdint.h>

#define QRQ_PRACTICE_NO_ITEM ((size_t)-1)

struct qrq_review_queue {
	size_t *items;
	size_t count;
	size_t next;
	size_t capacity;
};

/* Select an unused callbase item. In adaptive mode, each prior mistake adds
 * one selection weight (capped to keep the calculation bounded). */
size_t qrq_practice_choose(size_t count, const unsigned char *used,
		const unsigned char *mistakes, int adaptive, uint32_t random_value);

int qrq_review_queue_push(struct qrq_review_queue *queue, size_t item);
int qrq_review_queue_take(struct qrq_review_queue *queue, size_t *item);
void qrq_review_queue_clear(struct qrq_review_queue *queue);

/* Return a whole-number accuracy percentage, rounded down. */
int qrq_practice_accuracy(size_t attempts, size_t errors);

#endif
