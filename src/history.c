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

int qrq_history_summarize(const char *path, const char *callsign,
		struct qrq_history_summary *summary) {
	char line[256];
	FILE *file;
	long long timestamp;
	char recorded_call[32];
	int calls;
	int errors;
	int score;
	int max_speed;
	int eligible;
	long long total_score = 0;
	long long total_accuracy = 0;

	if (path == NULL || callsign == NULL || summary == NULL) {
		return -1;
	}
	memset(summary, 0, sizeof(*summary));
	file = fopen(path, "r");
	if (file == NULL) {
		return -1;
	}
	if (fgets(line, sizeof(line), file) == NULL || strcmp(line, "qrq-history-v1\n") != 0) {
		fclose(file);
		return -1;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		if (sscanf(line, "%lld,%31[^,],%d,%d,%d,%d,%d", &timestamp,
				recorded_call, &calls, &errors, &score, &max_speed, &eligible) != 7 ||
				strcmp(recorded_call, callsign) != 0 || calls <= 0 || errors < 0 ||
				errors > calls || score < 0 || max_speed < 0) {
			continue;
		}
		if (summary->sessions == 0) {
			summary->first_score = score;
		}
		summary->last_score = score;
		if (score > summary->best_score) summary->best_score = score;
		if (max_speed > summary->best_speed) summary->best_speed = max_speed;
		total_score += score;
		total_accuracy += (long long)(calls - errors) * 100 / calls;
		summary->sessions++;
	}
	if (ferror(file) || fclose(file) != 0) {
		return -1;
	}
	if (summary->sessions != 0) {
		summary->average_score = (int)(total_score / (long long)summary->sessions);
		summary->average_accuracy = (int)(total_accuracy / (long long)summary->sessions);
	}
	return 0;
}
