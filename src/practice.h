#ifndef QRQ_PRACTICE_H
#define QRQ_PRACTICE_H

#include <stddef.h>
#include <stdint.h>

#define QRQ_PRACTICE_NO_ITEM ((size_t)-1)
#define QRQ_REVIEW_INTERVAL 3
#define QRQ_PRACTICE_MAX_ANSWER_BATCH 5

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

/* Add a bounded selection boost for items marked due by persistent review. */
size_t qrq_practice_choose_scheduled(size_t count, const unsigned char *used,
		const unsigned char *mistakes, int adaptive, const unsigned char *due,
		int spaced_repetition, uint32_t random_value);

/* Clamp a configured delayed-answer batch to the remaining items. A batch of
 * one retains the ordinary send-then-answer workflow. */
size_t qrq_practice_answer_batch_size(size_t remaining, int configured_batch);

/* Record one result. Correct items are retired from the session pool; an
 * adaptively trained missed item remains available with increased weight. */
int qrq_practice_record_result(size_t count, unsigned char *used,
		unsigned char *mistakes, size_t item, int copied_correctly, int adaptive);

int qrq_review_queue_push(struct qrq_review_queue *queue, size_t item);
int qrq_review_queue_take(struct qrq_review_queue *queue, size_t *item);
void qrq_review_queue_clear(struct qrq_review_queue *queue);

/* Return a whole-number accuracy percentage, rounded down. */
int qrq_practice_accuracy(size_t attempts, size_t errors);

/* A comparable session must use ordinary scoring, finish every requested
 * item, and meet the optional accuracy target. */
int qrq_practice_session_eligible(int attempt_valid, size_t completed,
		size_t requested, int accuracy, int accuracy_target);

#endif
