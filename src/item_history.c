#include "item_history.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QRQ_ITEM_HISTORY_HEADER "qrq-item-history-v1\n"

struct item_counter {
	char sent[QRQ_ITEM_HISTORY_ITEM_MAX + 1];
	size_t attempts;
	size_t errors;
};

struct item_schedule_entry {
	char sent[QRQ_ITEM_HISTORY_ITEM_MAX + 1];
	size_t last_sequence;
	unsigned char correct_streak;
	int last_correct;
};

static int valid_header(const char *line) {
	return strcmp(line, QRQ_ITEM_HISTORY_HEADER) == 0 ||
			strcmp(line, "qrq-item-history-v1\r\n") == 0;
}

static int valid_callsign(const char *callsign) {
	return callsign != NULL && callsign[0] != '\0' && strlen(callsign) < 32 &&
			strpbrk(callsign, ",\r\n") == NULL;
}

static int hex_digit(unsigned char value) {
	if (value >= '0' && value <= '9') {
		return value - '0';
	}
	if (value >= 'A' && value <= 'F') {
		return value - 'A' + 10;
	}
	if (value >= 'a' && value <= 'f') {
		return value - 'a' + 10;
	}
	return -1;
}

static int encode_item(const char *item, char *encoded, size_t capacity) {
	static const char digits[] = "0123456789ABCDEF";
	size_t length;
	size_t index;

	if (item == NULL || encoded == NULL || (length = strlen(item)) == 0 ||
			length > QRQ_ITEM_HISTORY_ITEM_MAX || length > (capacity - 1) / 2) {
		return -1;
	}
	for (index = 0; index < length; ++index) {
		unsigned char value = (unsigned char)item[index];
		if (value < 32 || value > 126) {
			return -1;
		}
		encoded[2 * index] = digits[value >> 4];
		encoded[2 * index + 1] = digits[value & 0x0fU];
	}
	encoded[2 * length] = '\0';
	return 0;
}

static int decode_item(const char *encoded, char *item, size_t capacity) {
	size_t length;
	size_t index;

	if (encoded == NULL || item == NULL || (length = strlen(encoded)) == 0 ||
			length % 2 != 0 || length / 2 > QRQ_ITEM_HISTORY_ITEM_MAX ||
			length / 2 >= capacity) {
		return -1;
	}
	for (index = 0; index < length / 2; ++index) {
		int high = hex_digit((unsigned char)encoded[2 * index]);
		int low = hex_digit((unsigned char)encoded[2 * index + 1]);
		unsigned char value;

		if (high < 0 || low < 0) {
			return -1;
		}
		value = (unsigned char)((unsigned int)high << 4 | (unsigned int)low);
		if (value < 32 || value > 126) {
			return -1;
		}
		item[index] = (char)value;
	}
	item[length / 2] = '\0';
	return 0;
}

static int parse_item_record(const char *line, char *recorded_call,
		char *sent, int *copied_correctly, uint64_t *response_ms) {
	char encoded[2 * QRQ_ITEM_HISTORY_ITEM_MAX + 1];
	unsigned int copied;
	unsigned long long response;
	size_t line_length;
	int consumed = 0;

	if (line == NULL || recorded_call == NULL || sent == NULL ||
			copied_correctly == NULL || response_ms == NULL ||
			(line_length = strlen(line)) == 0 || line[line_length - 1] != '\n' ||
			sscanf(line, "%31[^,],%56[^,],%u,%llu%n", recorded_call, encoded,
					&copied, &response, &consumed) != 4 ||
			(strcmp(line + consumed, "\n") != 0 &&
			 strcmp(line + consumed, "\r\n") != 0) || copied > 1 ||
			decode_item(encoded, sent, QRQ_ITEM_HISTORY_ITEM_MAX + 1) != 0) {
		return 0;
	}
	*copied_correctly = (int)copied;
	*response_ms = (uint64_t)response;
	return 1;
}

static int prepare_file_for_append(FILE *file) {
	char header[32];
	long length;
	int character;
	int last_character = '\n';

	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0) {
		return -1;
	}
	if (length == 0) {
		return fputs(QRQ_ITEM_HISTORY_HEADER, file) == EOF ? -1 : 0;
	}
	if (fseek(file, 0, SEEK_SET) != 0 ||
			fgets(header, sizeof(header), file) == NULL || !valid_header(header)) {
		return -1;
	}
	while ((character = fgetc(file)) != EOF) {
		last_character = character;
	}
	if (ferror(file) || last_character != '\n') {
		return -1;
	}
	clearerr(file);
	return fseek(file, 0, SEEK_END) == 0 ? 0 : -1;
}

int qrq_item_history_append(const char *path, const char *callsign,
		const char *sent, int copied_correctly, uint64_t response_ms) {
	char encoded[2 * QRQ_ITEM_HISTORY_ITEM_MAX + 1];
	FILE *file;
	int result = -1;

	if (path == NULL || !valid_callsign(callsign) ||
			(copied_correctly != 0 && copied_correctly != 1) ||
			encode_item(sent, encoded, sizeof(encoded)) != 0) {
		return -1;
	}
	file = fopen(path, "a+");
	if (file == NULL || prepare_file_for_append(file) != 0) {
		goto cleanup;
	}
	if (fprintf(file, "%s,%s,%d,%llu\n", callsign, encoded, copied_correctly,
				(unsigned long long)response_ms) < 0 || fflush(file) != 0) {
		goto cleanup;
	}
	result = 0;

cleanup:
	if (file != NULL && fclose(file) != 0) {
		result = -1;
	}
	return result;
}

static int counter_precedes(const struct item_counter *left,
		const struct item_counter *right) {
	if (left->errors != right->errors) {
		return left->errors > right->errors;
	}
	if (left->attempts != right->attempts) {
		return left->attempts > right->attempts;
	}
	return strcmp(left->sent, right->sent) < 0;
}

static void consider_counter(struct qrq_item_history_summary *summary,
		const struct item_counter *counter) {
	size_t position;

	if (counter->errors == 0) {
		return;
	}
	for (position = 0; position < summary->difficult_count; ++position) {
		struct item_counter existing = {
			.attempts = summary->difficult[position].attempts,
			.errors = summary->difficult[position].errors
		};

		memcpy(existing.sent, summary->difficult[position].sent,
				sizeof(existing.sent));
		if (counter_precedes(counter, &existing)) {
			break;
		}
	}
	if (position == QRQ_ITEM_HISTORY_TOP_COUNT) {
		return;
	}
	if (summary->difficult_count < QRQ_ITEM_HISTORY_TOP_COUNT) {
		summary->difficult_count++;
	}
	{
		size_t end = summary->difficult_count - 1;
		while (end > position) {
			summary->difficult[end] = summary->difficult[end - 1];
			end--;
		}
	}
	memcpy(summary->difficult[position].sent, counter->sent,
			sizeof(summary->difficult[position].sent));
	summary->difficult[position].attempts = counter->attempts;
	summary->difficult[position].errors = counter->errors;
}

static int add_counter(struct item_counter **counters, size_t *count,
		size_t *capacity, const char *sent, int copied_correctly) {
	struct item_counter *resized;
	size_t index;

	for (index = 0; index < *count; ++index) {
		if (strcmp((*counters)[index].sent, sent) == 0) {
			break;
		}
	}
	if (index == *count) {
		if (*count == *capacity) {
			size_t new_capacity = *capacity == 0 ? 16 : *capacity;
			if (new_capacity > SIZE_MAX / 2 ||
					new_capacity * 2 > SIZE_MAX / sizeof(**counters)) {
				return -1;
			}
			new_capacity *= 2;
			resized = realloc(*counters, new_capacity * sizeof(**counters));
			if (resized == NULL) {
				return -1;
			}
			*counters = resized;
			*capacity = new_capacity;
		}
		memset(&(*counters)[index], 0, sizeof((*counters)[index]));
		memcpy((*counters)[index].sent, sent, strlen(sent) + 1);
		(*count)++;
	}
	if ((*counters)[index].attempts == SIZE_MAX ||
			(!copied_correctly && (*counters)[index].errors == SIZE_MAX)) {
		return -1;
	}
	(*counters)[index].attempts++;
	if (!copied_correctly) {
		(*counters)[index].errors++;
	}
	return 0;
}

int qrq_item_history_summarize(const char *path, const char *callsign,
		struct qrq_item_history_summary *summary) {
	char line[160];
	char recorded_call[32];
	char sent[QRQ_ITEM_HISTORY_ITEM_MAX + 1];
	struct item_counter *counters = NULL;
	size_t count = 0;
	size_t capacity = 0;
	int copied_correctly;
	uint64_t response_ms;
	FILE *file;
	size_t index;
	int result = -1;

	if (path == NULL || !valid_callsign(callsign) || summary == NULL) {
		return -1;
	}
	memset(summary, 0, sizeof(*summary));
	file = fopen(path, "r");
	if (file == NULL) {
		return -1;
	}
	if (fgets(line, sizeof(line), file) == NULL || !valid_header(line)) {
		goto cleanup;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		if (!parse_item_record(line, recorded_call, sent, &copied_correctly,
					&response_ms)) {
			size_t line_length = strlen(line);

			if (line_length != 0 && line[line_length - 1] == '\n') {
				continue;
			}
			int character;
			while ((character = fgetc(file)) != EOF && character != '\n') {
			}
			continue;
		}
		if (strcmp(recorded_call, callsign) != 0) {
			continue;
		}
		if (summary->attempts == SIZE_MAX ||
				(copied_correctly && summary->correct == SIZE_MAX) ||
				summary->total_response_ms > UINT64_MAX - response_ms ||
				add_counter(&counters, &count, &capacity, sent, copied_correctly) != 0) {
			goto cleanup;
		}
		summary->attempts++;
		if (copied_correctly) {
			summary->correct++;
		}
		summary->total_response_ms += response_ms;
	}
	{
		int read_failed = ferror(file);
		int close_failed = fclose(file) != 0;

		file = NULL;
		if (read_failed || close_failed) {
			goto cleanup;
		}
	}
	for (index = 0; index < count; ++index) {
		consider_counter(summary, &counters[index]);
	}
	result = 0;

cleanup:
	if (file != NULL) {
		(void)fclose(file);
	}
	free(counters);
	return result;
}

static int schedule_entry_compare(const void *left, const void *right) {
	const struct item_schedule_entry *left_entry = left;
	const struct item_schedule_entry *right_entry = right;

	return strcmp(left_entry->sent, right_entry->sent);
}

static int schedule_entry_add(struct item_schedule_entry **entries,
		size_t *count, size_t *capacity, const char *sent, size_t sequence,
		int copied_correctly) {
	struct item_schedule_entry *resized;
	size_t index;

	for (index = 0; index < *count; ++index) {
		if (strcmp((*entries)[index].sent, sent) == 0) {
			break;
		}
	}
	if (index == *count) {
		if (*count == *capacity) {
			size_t new_capacity = *capacity == 0 ? 16 : *capacity;

			if (new_capacity > SIZE_MAX / 2 ||
					new_capacity * 2 > SIZE_MAX / sizeof(**entries)) {
				return -1;
			}
			new_capacity *= 2;
			resized = realloc(*entries, new_capacity * sizeof(**entries));
			if (resized == NULL) {
				return -1;
			}
			*entries = resized;
			*capacity = new_capacity;
		}
		memset(&(*entries)[index], 0, sizeof((*entries)[index]));
		memcpy((*entries)[index].sent, sent, strlen(sent) + 1);
		(*count)++;
	}
	(*entries)[index].last_sequence = sequence;
	(*entries)[index].last_correct = copied_correctly;
	if (copied_correctly) {
		if ((*entries)[index].correct_streak != UCHAR_MAX) {
			(*entries)[index].correct_streak++;
		}
	} else {
		(*entries)[index].correct_streak = 0;
	}
	return 0;
}

static size_t review_interval(unsigned char correct_streak) {
	static const size_t intervals[] = {1, 3, 7, 15, 31};
	size_t index;

	if (correct_streak == 0) {
		return 0;
	}
	index = correct_streak - 1;
	if (index >= sizeof(intervals) / sizeof(intervals[0])) {
		index = sizeof(intervals) / sizeof(intervals[0]) - 1;
	}
	return intervals[index];
}

int qrq_item_history_schedule(const char *path, const char *callsign,
		const char *const *items, size_t item_count, unsigned char *due) {
	char line[160];
	char recorded_call[32];
	char sent[QRQ_ITEM_HISTORY_ITEM_MAX + 1];
	struct item_schedule_entry *entries = NULL;
	size_t entry_count = 0;
	size_t capacity = 0;
	size_t sequence = 0;
	FILE *file = NULL;
	size_t index;
	int result = -1;

	if (path == NULL || !valid_callsign(callsign) ||
			(item_count != 0 && (items == NULL || due == NULL))) {
		return -1;
	}
	if (item_count != 0) {
		memset(due, 0, item_count);
	}
	file = fopen(path, "r");
	if (file == NULL) {
		return errno == ENOENT ? 0 : -1;
	}
	if (fgets(line, sizeof(line), file) == NULL || !valid_header(line)) {
		goto cleanup;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		int copied_correctly;
		uint64_t response_ms;

		if (!parse_item_record(line, recorded_call, sent, &copied_correctly,
					&response_ms)) {
			size_t line_length = strlen(line);

			if (line_length != 0 && line[line_length - 1] == '\n') {
				continue;
			}
			{
				int character;
				while ((character = fgetc(file)) != EOF && character != '\n') {
				}
			}
			continue;
		}
		(void)response_ms;
		if (strcmp(recorded_call, callsign) != 0) {
			continue;
		}
		if (sequence == SIZE_MAX) {
			goto cleanup;
		}
		sequence++;
		if (schedule_entry_add(&entries, &entry_count, &capacity, sent, sequence,
					copied_correctly) != 0) {
			goto cleanup;
		}
	}
	{
		int read_failed = ferror(file);
		int close_failed = fclose(file) != 0;

		file = NULL;
		if (read_failed || close_failed) {
			goto cleanup;
		}
	}
	if (entry_count > 1) {
		qsort(entries, entry_count, sizeof(*entries), schedule_entry_compare);
	}
	for (index = 0; index < item_count; ++index) {
		struct item_schedule_entry key;
		struct item_schedule_entry *entry;

		if (items[index] == NULL || strlen(items[index]) == 0 ||
				strlen(items[index]) > QRQ_ITEM_HISTORY_ITEM_MAX) {
			goto cleanup;
		}
		memset(&key, 0, sizeof(key));
		memcpy(key.sent, items[index], strlen(items[index]) + 1);
		if (entry_count == 0) {
			continue;
		}
		entry = bsearch(&key, entries, entry_count, sizeof(*entries),
				schedule_entry_compare);
		if (entry == NULL) {
			continue;
		}
		if (!entry->last_correct) {
			due[index] = 3;
		} else {
			size_t interval = review_interval(entry->correct_streak);
			size_t elapsed = sequence >= entry->last_sequence ?
					sequence - entry->last_sequence : 0;

			if (interval != 0 && elapsed >= interval) {
				size_t priority = elapsed / interval;
				due[index] = (unsigned char)(priority > 3 ? 3 : priority);
			}
		}
	}
	result = 0;

cleanup:
	if (file != NULL) {
		(void)fclose(file);
	}
	free(entries);
	return result;
}
