#include "practice.h"

#include <assert.h>
#include <limits.h>

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
	assert(qrq_practice_answer_batch_size(0, 1) == 0);
	assert(qrq_practice_answer_batch_size(1, 5) == 1);
	assert(qrq_practice_answer_batch_size(3, 2) == 2);
	assert(qrq_practice_answer_batch_size(8, 0) == 1);
	assert(qrq_practice_answer_batch_size(8, 99) == QRQ_PRACTICE_MAX_ANSWER_BATCH);
	{
		unsigned char due[] = {0, 2, 0};
		assert(qrq_practice_choose_scheduled(3, used, mistakes, 0, due, 1, 0) == 0);
		assert(qrq_practice_choose_scheduled(3, used, mistakes, 0, due, 1, 1) == 1);
		assert(qrq_practice_choose_scheduled(3, used, mistakes, 0, due, 1, 8) == 1);
		assert(qrq_practice_choose_scheduled(3, used, mistakes, 0, due, 1, 10) == 2);
	}
	used[1] = 1;
	assert(qrq_practice_choose(3, used, mistakes, 1, 1) == 2);
	assert(qrq_practice_record_result(3, used, mistakes, 1, 0, 1) == 0);
	assert(used[1] == 0 && mistakes[1] == 3);
	assert(qrq_practice_choose(3, used, mistakes, 1, 1) == 1);
	assert(qrq_practice_record_result(3, used, mistakes, 1, 1, 1) == 0);
	assert(used[1] == 1 && mistakes[1] == 3);
	used[1] = 0;
	mistakes[1] = UCHAR_MAX;
	assert(qrq_practice_record_result(3, used, mistakes, 1, 0, 0) == 0);
	assert(used[1] == 1 && mistakes[1] == UCHAR_MAX);
	assert(qrq_practice_record_result(3, used, mistakes, 3, 0, 1) == -1);
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
	assert(qrq_practice_session_eligible(1, 50, 50, 90, 0) == 1);
	assert(qrq_practice_session_eligible(1, 49, 50, 98, 0) == 0);
	assert(qrq_practice_session_eligible(0, 50, 50, 100, 0) == 0);
	assert(qrq_practice_session_eligible(1, 50, 50, 89, 90) == 0);
	assert(qrq_practice_session_eligible(1, 50, 50, 90, 90) == 1);
	assert(qrq_practice_session_eligible(1, 0, 0, 100, 0) == 0);
	assert(qrq_practice_session_eligible(1, 50, 50, 101, 0) == 0);
	return 0;
}
