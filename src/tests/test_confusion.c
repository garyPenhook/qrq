#include "confusion.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int contains_pair(const struct qrq_confusion_summary *summary,
		unsigned char expected, unsigned char received, size_t count) {
	size_t index;

	for (index = 0; index < summary->pair_count; ++index) {
		if (summary->pairs[index].expected == expected &&
				summary->pairs[index].received == received &&
				summary->pairs[index].count == count) {
			return 1;
		}
	}
	return 0;
}

int main(void) {
	const char *path = "test_confusion.csv";
	struct qrq_confusion_summary summary;
	FILE *file;
	char header[32];

	remove(path);
	assert(qrq_confusion_append(path, "K1ABC", "K1ABC", "K1ABC") == 0);
	assert(qrq_confusion_append(path, "K1ABC", "K1ABC", "K1AXC") == 0);
	assert(qrq_confusion_append(path, "K1ABC", "K1ABC", "K1AXC") == 0);
	assert(qrq_confusion_append(path, "K1ABC", "K1ABC", "K1AC") == 0);
	assert(qrq_confusion_append(path, "W1AW", "A", "B") == 0);

	assert(qrq_confusion_summarize(path, "K1ABC", &summary) == 0);
	assert(summary.errors == 3);
	assert(summary.pair_count == 2);
	assert(summary.pairs[0].expected == 'B' && summary.pairs[0].received == 'X' &&
			summary.pairs[0].count == 2);
	assert(summary.pairs[1].expected == 'B' && summary.pairs[1].received == '\0' &&
			summary.pairs[1].count == 1);
	{
		char symbols[8];

		assert(qrq_confusion_focus_symbols(path, "K1ABC", symbols,
				sizeof(symbols)) == 0);
		assert(strcmp(symbols, "BX") == 0);
	}
	assert(qrq_confusion_append(path, "K1ABC", "ABC", "ABXC") == 0);
	assert(qrq_confusion_summarize(path, "K1ABC", &summary) == 0);
	assert(summary.errors == 4);
	assert(contains_pair(&summary, '\0', 'X', 1));

	file = fopen(path, "r");
	assert(file != NULL);
	assert(fgets(header, sizeof(header), file) != NULL);
	assert(fclose(file) == 0);
	assert(qrq_confusion_append(path, "K1,ABC", "A", "B") == -1);
	remove(path);

	file = fopen(path, "wb");
	assert(file != NULL);
	assert(fputs("qrq-confusions-v1\nK1ABC,65,66,trailing\nK1ABC,65,66\n", file) >= 0);
	assert(fclose(file) == 0);
	assert(qrq_confusion_summarize(path, "K1ABC", &summary) == 0);
	assert(summary.errors == 1 && summary.pair_count == 1);
	assert(summary.pairs[0].expected == 'A' && summary.pairs[0].received == 'B');
	remove(path);
	return 0;
}
