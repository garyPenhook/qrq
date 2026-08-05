#include "history.h"

#include <stdio.h>
#include <string.h>

int qrq_history_append(const char *path, const struct qrq_history_entry *entry) {
	FILE *file;
	long length;
	int result = -1;

	if (path == NULL || entry == NULL || entry->callsign == NULL || entry->calls < 0 ||
			entry->errors < 0 || entry->errors > entry->calls ||
			strpbrk(entry->callsign, ",\r\n") != NULL) {
		return -1;
	}
	file = fopen(path, "a+");
	if (file == NULL) {
		return -1;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0) {
		goto cleanup;
	}
	if (length == 0 && fputs("qrq-history-v1\n", file) == EOF) {
		goto cleanup;
	}
	if (fprintf(file, "%lld,%s,%d,%d,%d,%d,%d\n", (long long)entry->timestamp,
			entry->callsign, entry->calls, entry->errors, entry->score,
			entry->max_speed, entry->eligible != 0) < 0 || fflush(file) != 0) {
		goto cleanup;
	}
	result = 0;

cleanup:
	if (fclose(file) != 0) {
		result = -1;
	}
	return result;
}
