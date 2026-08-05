#include "history.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	const char *path = "test_history.csv";
	const struct qrq_history_entry entry = {100, "K1ABC", 50, 2, 123, 200, 1};
	const struct qrq_history_entry other = {101, "K1ABC", 50, 0, 200, 250, 1};
	const struct qrq_history_entry ineligible = {102, "K1ABC", 50, 50, 0, 20, 0};
	struct qrq_history_summary summary;
	char contents[128] = "";
	FILE *file;

	assert(qrq_history_append(path, &entry) == 0);
	assert(qrq_history_append(path, &other) == 0);
	assert(qrq_history_append(path, &entry) == 0);
	assert(qrq_history_append(path, &ineligible) == 0);
	assert(qrq_history_summarize(path, "K1ABC", &summary) == 0);
	assert(summary.sessions == 3 && summary.average_score == 148);
	assert(summary.average_accuracy == 97 && summary.best_score == 200);
	assert(summary.best_speed == 250 && summary.first_score == 123 && summary.last_score == 123);
	file = fopen(path, "r");
	assert(file != NULL);
	assert(fread(contents, 1, sizeof(contents) - 1, file) > 0);
	assert(fclose(file) == 0);
	assert(strstr(contents, "qrq-history-v1\n") == contents);
	remove(path);
	return 0;
}
