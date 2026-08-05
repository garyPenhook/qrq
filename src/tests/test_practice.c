#include "practice.h"

#include <assert.h>

int main(void) {
	unsigned char used[] = {0, 0, 0};
	unsigned char mistakes[] = {0, 2, 0};
	struct qrq_review_queue queue = {0};
	size_t item;

	assert(qrq_practice_choose(0, used, mistakes, 0, 0) == QRQ_PRACTICE_NO_ITEM);
	assert(qrq_practice_choose(3, used, mistakes, 0, 0) == 0);
	assert(qrq_practice_choose(3, used, mistakes, 0, 1) == 1);
	assert(qrq_practice_choose(3, used, mistakes, 0, 2) == 2);
	assert(qrq_practice_choose(3, used, mistakes, 1, 0) == 0);
	assert(qrq_practice_choose(3, used, mistakes, 1, 1) == 1);
	assert(qrq_practice_choose(3, used, mistakes, 1, 2) == 1);
	assert(qrq_practice_choose(3, used, mistakes, 1, 3) == 1);
	assert(qrq_practice_choose(3, used, mistakes, 1, 4) == 2);
	used[1] = 1;
	assert(qrq_practice_choose(3, used, mistakes, 1, 1) == 2);
	used[0] = used[2] = 1;
	assert(qrq_practice_choose(3, used, mistakes, 1, 0) == QRQ_PRACTICE_NO_ITEM);
	assert(qrq_review_queue_take(&queue, &item) == 0);
	assert(qrq_review_queue_push(&queue, 2) == 0);
	assert(qrq_review_queue_push(&queue, 1) == 0);
	assert(qrq_review_queue_take(&queue, &item) == 1 && item == 2);
	assert(qrq_review_queue_take(&queue, &item) == 1 && item == 1);
	assert(qrq_review_queue_take(&queue, &item) == 0);
	qrq_review_queue_clear(&queue);
	assert(qrq_practice_accuracy(0, 0) == 0);
	assert(qrq_practice_accuracy(10, 0) == 100);
	assert(qrq_practice_accuracy(10, 1) == 90);
	assert(qrq_practice_accuracy(3, 1) == 66);
	assert(qrq_practice_accuracy(3, 4) == 0);
	return 0;
}
