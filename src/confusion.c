#include "confusion.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define QRQ_CONFUSION_HEADER_V1 "qrq-confusions-v1\n"
#define QRQ_CONFUSION_HEADER_V2 "qrq-confusions-v2\n"
#define QRQ_CONFUSION_BACKUP_SUFFIX ".bak"

enum qrq_confusion_direction {
	QRQ_CONFUSION_DIAGONAL,
	QRQ_CONFUSION_OMIT,
	QRQ_CONFUSION_EXTRA
};

static const char supported_symbols[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/+.,= ?-";

static int header_version(const char *line) {
	if (strcmp(line, QRQ_CONFUSION_HEADER_V1) == 0 ||
			strcmp(line, "qrq-confusions-v1\r\n") == 0) {
		return 1;
	}
	if (strcmp(line, QRQ_CONFUSION_HEADER_V2) == 0 ||
			strcmp(line, "qrq-confusions-v2\r\n") == 0) {
		return 2;
	}
	return 0;
}

static int symbol_index(unsigned char symbol) {
	const char *position;

	if (symbol == '\0') {
		return 0;
	}
	if (symbol >= 'a' && symbol <= 'z') {
		symbol = (unsigned char)toupper(symbol);
	}
	position = strchr(supported_symbols, symbol);
	return position == NULL ? -1 : (int)(position - supported_symbols) + 1;
}

static unsigned char normalized_symbol(unsigned char symbol) {
	if (symbol >= 'a' && symbol <= 'z') {
		return (unsigned char)toupper(symbol);
	}
	return symbol;
}

static int valid_callsign(const char *callsign) {
	return callsign != NULL && callsign[0] != '\0' && strlen(callsign) < 32 &&
			strpbrk(callsign, ",\r\n") == NULL;
}

static int prepare_file_for_append(FILE *file) {
	char header[32];
	long length;
	int character;
	int last_character = '\n';
	int version;

	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0) {
		return -1;
	}
	if (length == 0) {
		return fputs(QRQ_CONFUSION_HEADER_V2, file) == EOF ? -1 : 0;
	}
	if (fseek(file, 0, SEEK_SET) != 0 ||
			fgets(header, sizeof(header), file) == NULL ||
			(version = header_version(header)) == 0) {
		return -1;
	}
	while ((character = fgetc(file)) != EOF) {
		last_character = character;
	}
	if (ferror(file) || last_character != '\n') {
		return -1;
	}
	clearerr(file);
	if (version == 1) {
		/* The two headers are deliberately equal in length.  Old rows are
		 * still accepted, while every new row gains a timestamp. */
		if (fseek(file, 0, SEEK_SET) != 0 ||
				fputs(QRQ_CONFUSION_HEADER_V2, file) == EOF || fflush(file) != 0) {
			return -1;
		}
	}
	return fseek(file, 0, SEEK_END) == 0 ? 0 : -1;
}

static int record_difference(FILE *file, const char *callsign,
		unsigned char expected, unsigned char received) {
	time_t recorded = time(NULL);
	long long timestamp;

	if (symbol_index(expected) < 0 || symbol_index(received) < 0) {
		return -1;
	}
	if (recorded == (time_t)-1) {
		return -1;
	}
	timestamp = (long long)recorded;
	return fprintf(file, "%s,%u,%u,%lld\n", callsign, (unsigned int)expected,
			(unsigned int)received, timestamp) < 0 ? -1 : 0;
}

int qrq_confusion_append(const char *path, const char *callsign,
		const char *sent, const char *received) {
	const size_t sent_length = sent == NULL ? 0 : strlen(sent);
	const size_t received_length = received == NULL ? 0 : strlen(received);
	const size_t columns = received_length + 1;
	unsigned char *directions = NULL;
	size_t *previous = NULL;
	size_t *current = NULL;
	FILE *file = NULL;
	size_t i;
	size_t j;
	int result = -1;

	if (path == NULL || sent == NULL || received == NULL || !valid_callsign(callsign) ||
			sent_length == SIZE_MAX || received_length == SIZE_MAX ||
			columns == 0 || sent_length + 1 > SIZE_MAX / columns) {
		return -1;
	}
	for (i = 0; i < sent_length; ++i) {
		if (symbol_index((unsigned char)sent[i]) < 0) {
			return -1;
		}
	}
	for (j = 0; j < received_length; ++j) {
		if (symbol_index((unsigned char)received[j]) < 0) {
			return -1;
		}
	}
	if (strcmp(sent, received) == 0) {
		return 0;
	}
	if (columns > SIZE_MAX / sizeof(*previous) ||
			sent_length + 1 > SIZE_MAX / columns) {
		return -1;
	}
	directions = malloc((sent_length + 1) * columns);
	previous = malloc(columns * sizeof(*previous));
	current = malloc(columns * sizeof(*current));
	if (directions == NULL || previous == NULL || current == NULL) {
		goto cleanup;
	}
	for (j = 0; j < columns; ++j) {
		previous[j] = j;
		directions[j] = QRQ_CONFUSION_EXTRA;
	}
	for (i = 1; i <= sent_length; ++i) {
		current[0] = i;
		directions[i * columns] = QRQ_CONFUSION_OMIT;
		for (j = 1; j < columns; ++j) {
			size_t diagonal = previous[j - 1] +
					(normalized_symbol((unsigned char)sent[i - 1]) ==
					 normalized_symbol((unsigned char)received[j - 1]) ? 0U : 1U);
			size_t omission = previous[j] + 1U;
			size_t extra = current[j - 1] + 1U;

			if (diagonal <= omission && diagonal <= extra) {
				current[j] = diagonal;
				directions[i * columns + j] = QRQ_CONFUSION_DIAGONAL;
			} else if (omission <= extra) {
				current[j] = omission;
				directions[i * columns + j] = QRQ_CONFUSION_OMIT;
			} else {
				current[j] = extra;
				directions[i * columns + j] = QRQ_CONFUSION_EXTRA;
			}
		}
		{
			size_t *temporary = previous;
			previous = current;
			current = temporary;
		}
	}
	file = fopen(path, "r+");
	if (file == NULL && errno == ENOENT) {
		file = fopen(path, "w+");
	}
	if (file == NULL || prepare_file_for_append(file) != 0) {
		goto cleanup;
	}
	i = sent_length;
	j = received_length;
	while (i != 0 || j != 0) {
		unsigned char expected = '\0';
		unsigned char actual = '\0';
		enum qrq_confusion_direction direction = directions[i * columns + j];

		if (direction == QRQ_CONFUSION_DIAGONAL) {
			expected = normalized_symbol((unsigned char)sent[--i]);
			actual = normalized_symbol((unsigned char)received[--j]);
		} else if (direction == QRQ_CONFUSION_OMIT) {
			expected = normalized_symbol((unsigned char)sent[--i]);
		} else {
			actual = normalized_symbol((unsigned char)received[--j]);
		}
		if (expected != actual &&
				record_difference(file, callsign, expected, actual) != 0) {
			goto cleanup;
		}
	}
	if (fflush(file) != 0) {
		goto cleanup;
	}
	result = 0;

cleanup:
	if (file != NULL && fclose(file) != 0) {
		result = -1;
	}
	free(current);
	free(previous);
	free(directions);
	return result;
}

static int pair_precedes(const struct qrq_confusion_pair *left,
		const struct qrq_confusion_pair *right) {
	if (left->count != right->count) {
		return left->count > right->count;
	}
	if (left->expected != right->expected) {
		return left->expected < right->expected;
	}
	return left->received < right->received;
}

static void consider_pair(struct qrq_confusion_summary *summary,
		struct qrq_confusion_pair pair) {
	size_t position;

	for (position = 0; position < summary->pair_count; ++position) {
		if (pair_precedes(&pair, &summary->pairs[position])) {
			break;
		}
	}
	if (position == QRQ_CONFUSION_TOP_COUNT) {
		return;
	}
	if (summary->pair_count < QRQ_CONFUSION_TOP_COUNT) {
		summary->pair_count++;
	}
	{
		size_t end = summary->pair_count - 1;
		while (end > position) {
			summary->pairs[end] = summary->pairs[end - 1];
			end--;
		}
	}
	summary->pairs[position] = pair;
}

static int parse_record(const char *line, char recorded_call[32],
		unsigned int *expected, unsigned int *received, long long *timestamp,
		int *has_timestamp) {
	int consumed = 0;
	long long parsed_timestamp;

	if (sscanf(line, "%31[^,],%u,%u,%lld%n", recorded_call, expected,
				received, &parsed_timestamp, &consumed) == 4 &&
			(strcmp(line + consumed, "\n") == 0 ||
			 strcmp(line + consumed, "\r\n") == 0)) {
		*timestamp = parsed_timestamp;
		*has_timestamp = 1;
		return 0;
	}
	consumed = 0;
	if (sscanf(line, "%31[^,],%u,%u%n", recorded_call, expected, received,
				&consumed) == 3 &&
			(strcmp(line + consumed, "\n") == 0 ||
			 strcmp(line + consumed, "\r\n") == 0)) {
		*timestamp = 0;
		*has_timestamp = 0;
		return 0;
	}
	return -1;
}

static size_t recency_weight(unsigned int days, time_t now,
		long long timestamp, int has_timestamp) {
	long long now_value;
	long long window;
	long long age;

	if (days == 0) {
		return 1;
	}
	if (!has_timestamp || now == (time_t)-1 || timestamp < 0 ||
			days > (unsigned int)(LLONG_MAX / 86400LL)) {
		return 0;
	}
	now_value = (long long)now;
	window = (long long)days * 86400LL;
	if (timestamp >= now_value) {
		return 4;
	}
	age = now_value - timestamp;
	if (age > window) {
		return 0;
	}
	/* Four bands keep the summary integer and stable: the newest quarter of
	 * the window gets 4x influence, then 3x, 2x, and 1x at the boundary. */
	{
		size_t weight = 1U + (size_t)((window - age) * 4LL / window);
		return weight > 4U ? 4U : weight;
	}
}

int qrq_confusion_summarize_recent(const char *path, const char *callsign,
		unsigned int days, time_t now, struct qrq_confusion_summary *summary) {
	size_t counts[sizeof(supported_symbols)][sizeof(supported_symbols)];
	char line[96];
	char recorded_call[32];
	unsigned int expected;
	unsigned int received;
	long long timestamp;
	size_t weight;
	int has_timestamp;
	FILE *file;
	size_t i;
	size_t j;
	int read_failed;
	int close_failed;

	if (path == NULL || !valid_callsign(callsign) || summary == NULL) {
		return -1;
	}
	memset(summary, 0, sizeof(*summary));
	memset(counts, 0, sizeof(counts));
	file = fopen(path, "r");
	if (file == NULL) {
		return -1;
	}
	if (fgets(line, sizeof(line), file) == NULL || header_version(line) == 0) {
		(void)fclose(file);
		return -1;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		size_t line_length = strlen(line);
		int expected_symbol;
		int received_symbol;

		if (line_length == 0 || line[line_length - 1] != '\n') {
			int character;
			while ((character = fgetc(file)) != EOF && character != '\n') {
			}
			continue;
		}
		if (parse_record(line, recorded_call, &expected, &received, &timestamp,
					&has_timestamp) != 0 ||
				strcmp(recorded_call, callsign) != 0 || expected > UCHAR_MAX ||
				received > UCHAR_MAX ||
				(expected_symbol = symbol_index((unsigned char)expected)) < 0 ||
				(received_symbol = symbol_index((unsigned char)received)) < 0 ||
				expected == received) {
			continue;
		}
		weight = recency_weight(days, now, timestamp, has_timestamp);
		if (weight == 0) {
			continue;
		}
		if (counts[expected_symbol][received_symbol] > SIZE_MAX - weight ||
				summary->errors > SIZE_MAX - weight) {
			(void)fclose(file);
			return -1;
		}
		counts[expected_symbol][received_symbol] += weight;
		summary->errors += weight;
	}
	read_failed = ferror(file);
	close_failed = fclose(file) != 0;
	if (read_failed || close_failed) {
		return -1;
	}
	for (i = 0; i < sizeof(supported_symbols); ++i) {
		for (j = 0; j < sizeof(supported_symbols); ++j) {
			if (counts[i][j] != 0) {
				struct qrq_confusion_pair pair = {
					.expected = i == 0 ? '\0' : (unsigned char)supported_symbols[i - 1],
					.received = j == 0 ? '\0' : (unsigned char)supported_symbols[j - 1],
					.count = counts[i][j]
				};
				consider_pair(summary, pair);
			}
		}
	}
	return 0;
}

int qrq_confusion_summarize(const char *path, const char *callsign,
		struct qrq_confusion_summary *summary) {
	return qrq_confusion_summarize_recent(path, callsign, 0, time(NULL), summary);
}

int qrq_confusion_focus_symbols_recent(const char *path, const char *callsign,
		unsigned int days, time_t now, char *symbols, size_t capacity) {
	struct qrq_confusion_summary summary;
	size_t index;
	size_t length = 0;

	if (symbols == NULL || capacity == 0 ||
			qrq_confusion_summarize_recent(path, callsign, days, now, &summary) != 0) {
		return -1;
	}
	symbols[0] = '\0';
	for (index = 0; index < summary.pair_count; ++index) {
		unsigned char candidates[2] = {summary.pairs[index].expected,
			summary.pairs[index].received};
		size_t candidate_index;

		for (candidate_index = 0; candidate_index < 2; ++candidate_index) {
			unsigned char candidate = candidates[candidate_index];

			if (candidate != '\0' && strchr(symbols, candidate) == NULL) {
				if (length + 1 >= capacity) {
					return -1;
				}
				symbols[length++] = (char)candidate;
				symbols[length] = '\0';
			}
		}
	}
	return 0;
}

int qrq_confusion_focus_symbols(const char *path, const char *callsign,
		char *symbols, size_t capacity) {
	return qrq_confusion_focus_symbols_recent(path, callsign, 0, time(NULL),
			symbols, capacity);
}

int qrq_confusion_reset(const char *path, char *backup, size_t capacity) {
	FILE *file;
	char *backup_path;
	size_t path_length;
	int had_history = 0;
	int result = -1;

	if (path == NULL || path[0] == '\0' || backup == NULL || capacity == 0) {
		return -1;
	}
	backup[0] = '\0';
	file = fopen(path, "rb");
	if (file != NULL) {
		had_history = 1;
		if (fclose(file) != 0) {
			return -1;
		}
	} else if (errno != ENOENT) {
		return -1;
	}
	if (!had_history) {
		file = fopen(path, "a+");
		if (file == NULL) {
			return -1;
		}
		if (prepare_file_for_append(file) != 0) {
			(void)fclose(file);
			file = NULL;
			return -1;
		}
		if (fclose(file) != 0) {
			file = NULL;
			return -1;
		}
		file = NULL;
		return 0;
	}
	path_length = strlen(path);
	if (path_length > SIZE_MAX - sizeof(QRQ_CONFUSION_BACKUP_SUFFIX)) {
		return -1;
	}
	if (path_length + sizeof(QRQ_CONFUSION_BACKUP_SUFFIX) > capacity) {
		errno = ENAMETOOLONG;
		return -1;
	}
	backup_path = malloc(path_length + sizeof(QRQ_CONFUSION_BACKUP_SUFFIX));
	if (backup_path == NULL) {
		return -1;
	}
	(void)snprintf(backup_path, path_length + sizeof(QRQ_CONFUSION_BACKUP_SUFFIX),
			"%s%s", path, QRQ_CONFUSION_BACKUP_SUFFIX);
	file = fopen(backup_path, "rb");
	if (file != NULL) {
		(void)fclose(file);
		file = NULL;
		errno = EEXIST;
		goto cleanup;
	}
	if (errno != ENOENT || rename(path, backup_path) != 0) {
		goto cleanup;
	}
	file = fopen(path, "w");
	if (file == NULL) {
		goto restore;
	}
	if (fputs(QRQ_CONFUSION_HEADER_V2, file) == EOF || fflush(file) != 0 ||
			fclose(file) != 0) {
		file = NULL;
		goto restore;
	}
	file = NULL;
	memcpy(backup, backup_path, strlen(backup_path) + 1);
	result = 0;
	goto cleanup;

restore:
	if (file != NULL) {
		(void)fclose(file);
		file = NULL;
	}
	{
		(void)remove(path);
		(void)rename(backup_path, path);
	}

cleanup:
	if (file != NULL) {
		(void)fclose(file);
	}
	free(backup_path);
	return result;
}
