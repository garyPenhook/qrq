#include "history.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define QRQ_HISTORY_HEADER "qrq-history-v1\n"

static int valid_header(const char *line) {
	return strcmp(line, QRQ_HISTORY_HEADER) == 0 ||
			strcmp(line, "qrq-history-v1\r\n") == 0;
}

int qrq_history_append(const char *path, const struct qrq_history_entry *entry) {
	char header[32];
	FILE *file;
	long length;
	int character;
	int last_character = '\n';
	int result = -1;

	if (path == NULL || entry == NULL || entry->callsign == NULL ||
			entry->callsign[0] == '\0' || entry->calls < 0 ||
			entry->errors < 0 || entry->errors > entry->calls ||
			entry->score < 0 || entry->max_speed < 0 ||
			strlen(entry->callsign) >= 32 ||
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
	if (length == 0) {
		if (fputs(QRQ_HISTORY_HEADER, file) == EOF) {
			goto cleanup;
		}
	} else {
		if (fseek(file, 0, SEEK_SET) != 0) {
			goto cleanup;
		}
		if (fgets(header, sizeof(header), file) == NULL || !valid_header(header)) {
			goto cleanup;
		}
		while ((character = fgetc(file)) != EOF) {
			last_character = character;
		}
		if (ferror(file) || last_character != '\n') {
			goto cleanup;
		}
		clearerr(file);
		if (fseek(file, 0, SEEK_END) != 0) {
			goto cleanup;
		}
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
	int close_failed;
	int read_failed;
	int consumed;
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
	if (fgets(line, sizeof(line), file) == NULL || !valid_header(line)) {
		(void)fclose(file);
		return -1;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		size_t line_length = strlen(line);

		if (line_length == 0 || line[line_length - 1] != '\n') {
			/* Drain an overlong or incomplete record so no continuation
			 * fragment can be mistaken for a separate valid session. */
			int character;
			while ((character = fgetc(file)) != EOF && character != '\n') {
			}
			continue;
		}
		consumed = 0;
		if (sscanf(line, "%lld,%31[^,],%d,%d,%d,%d,%d%n", &timestamp,
				recorded_call, &calls, &errors, &score, &max_speed, &eligible,
				&consumed) != 7 ||
				(strcmp(line + consumed, "\n") != 0 &&
				 strcmp(line + consumed, "\r\n") != 0) ||
				strcmp(recorded_call, callsign) != 0 || calls <= 0 || errors < 0 ||
				errors > calls || score < 0 || max_speed < 0 || eligible != 1) {
			continue;
		}
		if (total_score > LLONG_MAX - score || total_accuracy > LLONG_MAX - 100 ||
				summary->sessions == SIZE_MAX) {
			(void)fclose(file);
			return -1;
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
	read_failed = ferror(file);
	close_failed = fclose(file) != 0;
	if (read_failed || close_failed) {
		return -1;
	}
	if (summary->sessions != 0) {
		summary->average_score = (int)(total_score / (long long)summary->sessions);
		summary->average_accuracy = (int)(total_accuracy / (long long)summary->sessions);
	}
	return 0;
}
