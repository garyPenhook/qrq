#include "confusion.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
	char backup[64];
	time_t now;

	remove(path);
	remove("test_confusion.csv.bak");
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
	assert(strcmp(header, "qrq-confusions-v2\n") == 0);
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
	assert(qrq_confusion_append(path, "K1ABC", "A", "C") == 0);
	file = fopen(path, "r");
	assert(file != NULL);
	assert(fgets(header, sizeof(header), file) != NULL);
	assert(strcmp(header, "qrq-confusions-v2\n") == 0);
	assert(fclose(file) == 0);
	remove(path);

	now = time(NULL);
	assert(now != (time_t)-1);
	file = fopen(path, "wb");
	assert(file != NULL);
	assert(fputs("qrq-confusions-v2\n", file) >= 0);
	assert(fprintf(file, "K1ABC,65,66,%lld\n", (long long)now - 1LL) > 0);
	assert(fprintf(file, "K1ABC,66,67,%lld\n",
				(long long)now - 4LL * 86400LL) > 0);
	assert(fprintf(file, "K1ABC,65,68,%lld\n",
				(long long)now - 8LL * 86400LL) > 0);
	assert(fputs("K1ABC,65,69\n", file) >= 0);
	assert(fclose(file) == 0);
	assert(qrq_confusion_summarize_recent(path, "K1ABC", 7U, now, &summary) == 0);
	assert(summary.errors == 6 && summary.pair_count == 2);
	assert(summary.pairs[0].expected == 'A' && summary.pairs[0].received == 'B' &&
			summary.pairs[0].count == 4);
	assert(summary.pairs[1].expected == 'B' && summary.pairs[1].received == 'C' &&
			summary.pairs[1].count == 2);
	{
		char symbols[8];

		assert(qrq_confusion_focus_symbols_recent(path, "K1ABC", 7U, now,
				symbols, sizeof(symbols)) == 0);
		assert(strcmp(symbols, "ABC") == 0);
	}
	{
		char too_small[1];

		errno = 0;
		assert(qrq_confusion_reset(path, too_small, sizeof(too_small)) == -1);
		assert(errno == ENAMETOOLONG);
		assert(qrq_confusion_summarize(path, "K1ABC", &summary) == 0);
		assert(summary.errors == 4);
	}
	assert(qrq_confusion_reset(path, backup, sizeof(backup)) == 0);
	assert(strcmp(backup, "test_confusion.csv.bak") == 0);
	assert(qrq_confusion_summarize(path, "K1ABC", &summary) == 0);
	assert(summary.errors == 0 && summary.pair_count == 0);
	assert(qrq_confusion_summarize(backup, "K1ABC", &summary) == 0);
	assert(summary.errors == 4);
	errno = 0;
	assert(qrq_confusion_reset(path, backup, sizeof(backup)) == -1);
	assert(errno == EEXIST);
	remove(path);
	remove("test_confusion.csv.bak");
	assert(qrq_confusion_reset(path, backup, sizeof(backup)) == 0);
	assert(backup[0] == '\0');
	remove(path);
	return 0;
}
