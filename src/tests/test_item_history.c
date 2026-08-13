#include "item_history.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
	const char *path = "test_item_history.csv";
	struct qrq_item_history_summary summary;
	FILE *file;

	remove(path);
	assert(qrq_item_history_append(path, "K1ABC", "A", 1, 100) == 0);
	assert(qrq_item_history_append(path, "K1ABC", "A", 0, 200) == 0);
	assert(qrq_item_history_append(path, "K1ABC", "B", 0, 50) == 0);
	assert(qrq_item_history_append(path, "W1AW", "A", 0, 999) == 0);
	assert(qrq_item_history_summarize(path, "K1ABC", &summary) == 0);
	assert(summary.attempts == 3 && summary.correct == 1 &&
			summary.total_response_ms == 350);
	assert(summary.difficult_count == 2);
	assert(summary.difficult[0].sent[0] == 'A' && summary.difficult[0].sent[1] == '\0');
	assert(summary.difficult[0].attempts == 2 && summary.difficult[0].errors == 1);
	assert(summary.difficult[1].sent[0] == 'B' && summary.difficult[1].sent[1] == '\0');
	{
		const char *items[] = {"A", "B", "C"};
		unsigned char due[] = {9, 9, 9};

		assert(qrq_item_history_schedule(path, "K1ABC", items, 3, due) == 0);
		assert(due[0] == 3 && due[1] == 3 && due[2] == 0);
		assert(qrq_item_history_append(path, "K1ABC", "A", 1, 10) == 0);
		assert(qrq_item_history_append(path, "K1ABC", "C", 1, 10) == 0);
		assert(qrq_item_history_schedule(path, "K1ABC", items, 3, due) == 0);
		assert(due[0] == 1 && due[1] == 3 && due[2] == 0);
	}
	assert(qrq_item_history_append(path, "K1,ABC", "A", 1, 0) == -1);
	assert(qrq_item_history_append(path, "K1ABC", "", 1, 0) == -1);
	assert(qrq_item_history_append(path, "K1ABC", "A", 2, 0) == -1);
	remove(path);
	{
		const char *items[] = {"A"};
		unsigned char due[] = {9};

		assert(qrq_item_history_schedule(path, "K1ABC", items, 1, due) == 0);
		assert(due[0] == 0);
	}

	file = fopen(path, "wb");
	assert(file != NULL);
	assert(fputs("qrq-item-history-v1\n"
			"K1ABC,41,1,100,trailing\n"
			"K1ABC,41,0,200\n", file) >= 0);
	assert(fclose(file) == 0);
	assert(qrq_item_history_summarize(path, "K1ABC", &summary) == 0);
	assert(summary.attempts == 1 && summary.correct == 0 &&
			summary.total_response_ms == 200);
	remove(path);
	return 0;
}
