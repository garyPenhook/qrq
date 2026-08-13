#include "confusion.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QRQ_CONFUSION_HEADER "qrq-confusions-v1\n"

enum qrq_confusion_direction {
	QRQ_CONFUSION_DIAGONAL,
	QRQ_CONFUSION_OMIT,
	QRQ_CONFUSION_EXTRA
};

static const char supported_symbols[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/+.,= ?-";

static int valid_header(const char *line) {
	return strcmp(line, QRQ_CONFUSION_HEADER) == 0 ||
			strcmp(line, "qrq-confusions-v1\r\n") == 0;
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

	if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0) {
		return -1;
	}
	if (length == 0) {
		return fputs(QRQ_CONFUSION_HEADER, file) == EOF ? -1 : 0;
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

static int record_difference(FILE *file, const char *callsign,
		unsigned char expected, unsigned char received) {
	if (symbol_index(expected) < 0 || symbol_index(received) < 0) {
		return -1;
	}
	return fprintf(file, "%s,%u,%u\n", callsign, (unsigned int)expected,
			(unsigned int)received) < 0 ? -1 : 0;
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
	file = fopen(path, "a+");
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

int qrq_confusion_summarize(const char *path, const char *callsign,
		struct qrq_confusion_summary *summary) {
	size_t counts[sizeof(supported_symbols)][sizeof(supported_symbols)];
	char line[96];
	char recorded_call[32];
	unsigned int expected;
	unsigned int received;
	int consumed;
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
	if (fgets(line, sizeof(line), file) == NULL || !valid_header(line)) {
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
		consumed = 0;
		if (sscanf(line, "%31[^,],%u,%u%n", recorded_call, &expected, &received,
					&consumed) != 3 ||
				(strcmp(line + consumed, "\n") != 0 &&
				 strcmp(line + consumed, "\r\n") != 0) ||
				strcmp(recorded_call, callsign) != 0 || expected > UCHAR_MAX ||
				received > UCHAR_MAX ||
				(expected_symbol = symbol_index((unsigned char)expected)) < 0 ||
				(received_symbol = symbol_index((unsigned char)received)) < 0 ||
				expected == received) {
			continue;
		}
		if (counts[expected_symbol][received_symbol] == SIZE_MAX ||
				summary->errors == SIZE_MAX) {
			(void)fclose(file);
			return -1;
		}
		counts[expected_symbol][received_symbol]++;
		summary->errors++;
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

int qrq_confusion_focus_symbols(const char *path, const char *callsign,
		char *symbols, size_t capacity) {
	struct qrq_confusion_summary summary;
	size_t index;
	size_t length = 0;

	if (symbols == NULL || capacity == 0 ||
			qrq_confusion_summarize(path, callsign, &summary) != 0) {
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
