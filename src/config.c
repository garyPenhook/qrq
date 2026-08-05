#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int add_size(size_t left, size_t right, size_t *result) {
	if (result == NULL || left > SIZE_MAX - right) {
		errno = EOVERFLOW;
		return -1;
	}
	*result = left + right;
	return 0;
}

int qrq_config_read_line(FILE *file, char **buffer, size_t *capacity) {
	char *resized;
	size_t length = 0;
	int character;

	if (file == NULL || buffer == NULL || capacity == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (*buffer == NULL || *capacity == 0) {
		*capacity = 256;
		*buffer = malloc(*capacity);
		if (*buffer == NULL) {
			return -1;
		}
	}
	while ((character = fgetc(file)) != EOF && character != '\n') {
		if (length == QRQ_CONFIG_MAX_LINE) {
			errno = EOVERFLOW;
			return -1;
		}
		if (length + 1 >= *capacity) {
			size_t new_capacity = *capacity * 2;
			if (new_capacity > QRQ_CONFIG_MAX_LINE + 1) {
				new_capacity = QRQ_CONFIG_MAX_LINE + 1;
			}
			resized = realloc(*buffer, new_capacity);
			if (resized == NULL) {
				return -1;
			}
			*buffer = resized;
			*capacity = new_capacity;
		}
		(*buffer)[length++] = (char)character;
	}
	if (character == EOF && length == 0) {
		return ferror(file) ? -1 : 0;
	}
	if (length != 0 && (*buffer)[length - 1] == '\r') {
		length--;
	}
	(*buffer)[length] = '\0';
	return 1;
}

int qrq_config_split_line(char *line, char **key, char **value) {
	char *comment;
	char *end;
	char *equals;
	char *start;

	if (line == NULL || key == NULL || value == NULL) {
		return -1;
	}
	start = line;
	while (isspace((unsigned char)*start)) {
		start++;
	}
	if (*start == '\0' || *start == '#') {
		return 0;
	}
	equals = strchr(start, '=');
	if (equals == NULL) {
		return -1;
	}
	end = equals;
	while (end > start && isspace((unsigned char)end[-1])) {
		end--;
	}
	if (end == start) {
		return -1;
	}
	*end = '\0';
	for (char *cursor = start; *cursor != '\0'; cursor++) {
		if (!isalnum((unsigned char)*cursor) && *cursor != '_') {
			return -1;
		}
	}
	*key = start;
	start = equals + 1;
	while (isspace((unsigned char)*start)) {
		start++;
	}
	comment = start;
	while ((comment = strchr(comment, '#')) != NULL) {
		if (comment == start || isspace((unsigned char)comment[-1])) {
			*comment = '\0';
			break;
		}
		comment++;
	}
	end = start + strlen(start);
	while (end > start && isspace((unsigned char)end[-1])) {
		end--;
	}
	*end = '\0';
	*value = start;
	return 1;
}

int qrq_config_parse_int(const char *text, int minimum, int maximum, int *value) {
	char *end;
	long parsed;

	if (text == NULL || value == NULL || minimum > maximum ||
			!isdigit((unsigned char)*text)) {
		return -1;
	}
	errno = 0;
	parsed = strtol(text, &end, 10);
	if (errno != 0 || *end != '\0' || parsed < minimum || parsed > maximum) {
		return -1;
	}
	*value = (int)parsed;
	return 0;
}

int qrq_config_parse_uint(const char *text, unsigned int *value) {
	char *end;
	unsigned long parsed;

	if (text == NULL || value == NULL ||
			!isdigit((unsigned char)*text)) {
		return -1;
	}
	errno = 0;
	parsed = strtoul(text, &end, 10);
	if (errno != 0 || parsed > UINT_MAX) {
		return -1;
	}
	if (*end != '\0') {
		return -1;
	}
	*value = (unsigned int)parsed;
	return 0;
}

int qrq_config_parse_double(const char *text, double minimum, double maximum,
		double *value) {
	char *end;
	double parsed;

	if (text == NULL || value == NULL || minimum > maximum ||
			(!isdigit((unsigned char)*text) && *text != '.')) {
		return -1;
	}
	errno = 0;
	parsed = strtod(text, &end);
	if (errno != 0 || *end != '\0' || !isfinite(parsed) ||
			parsed < minimum || parsed > maximum) {
		return -1;
	}
	*value = parsed;
	return 0;
}

int qrq_config_copy_string(const char *text, char *destination, size_t capacity,
		int uppercase) {
	size_t length;
	size_t i;

	if (text == NULL || destination == NULL || capacity == 0) {
		return -1;
	}
	length = strlen(text);
	if (length >= capacity) {
		return -1;
	}
	for (i = 0; i < length; i++) {
		destination[i] = uppercase ?
				(char)toupper((unsigned char)text[i]) : text[i];
	}
	destination[length] = '\0';
	return 0;
}

int qrq_config_set_value(char **text, size_t *length, const char *key,
		const char *value) {
	char *updated;
	size_t key_length;
	size_t value_length;
	size_t line_start;
	size_t line_end;
	size_t content_end;
	size_t cursor;
	size_t value_start = 0;
	size_t value_end = 0;
	size_t prefix_length;
	size_t suffix_length;
	size_t updated_length;
	size_t separator_length;
	size_t newline_length;
	int found = 0;
	int use_crlf;

	if (text == NULL || *text == NULL || length == NULL || key == NULL ||
			value == NULL || key[0] == '\0' || strlen(*text) != *length) {
		errno = EINVAL;
		return -1;
	}
	for (const unsigned char *key_cursor = (const unsigned char *)key;
			*key_cursor != '\0'; key_cursor++) {
		if (!isalnum(*key_cursor) && *key_cursor != '_') {
			errno = EINVAL;
			return -1;
		}
	}
	key_length = strlen(key);
	value_length = strlen(value);
	use_crlf = strstr(*text, "\r\n") != NULL;

	for (line_start = 0; line_start < *length; line_start = line_end) {
		line_end = line_start;
		while (line_end < *length && (*text)[line_end] != '\n') {
			line_end++;
		}
		if (line_end < *length) {
			line_end++;
		}
		content_end = line_end;
		if (content_end > line_start && (*text)[content_end - 1] == '\n') {
			content_end--;
		}
		if (content_end > line_start && (*text)[content_end - 1] == '\r') {
			content_end--;
		}
		cursor = line_start;
		while (cursor < content_end && isspace((unsigned char)(*text)[cursor])) {
			cursor++;
		}
		if (key_length > content_end - cursor ||
				memcmp(*text + cursor, key, key_length) != 0) {
			continue;
		}
		cursor += key_length;
		while (cursor < content_end && isspace((unsigned char)(*text)[cursor])) {
			cursor++;
		}
		if (cursor == content_end || (*text)[cursor] != '=') {
			continue;
		}
		cursor++;
		while (cursor < content_end && isspace((unsigned char)(*text)[cursor])) {
			cursor++;
		}
		value_start = cursor;
		value_end = content_end;
		while (cursor < content_end) {
			if ((*text)[cursor] == '#' &&
					(cursor == value_start ||
					 isspace((unsigned char)(*text)[cursor - 1]))) {
				value_end = cursor;
				break;
			}
			cursor++;
		}
		while (value_end > value_start &&
				isspace((unsigned char)(*text)[value_end - 1])) {
			value_end--;
		}
		/* Configuration loading is last-value-wins, so update the last
		 * matching assignment when a hand-edited file contains duplicates. */
		found = 1;
	}

	if (found) {
		prefix_length = value_start;
		suffix_length = *length - value_end;
		if (add_size(prefix_length, value_length, &updated_length) != 0 ||
				add_size(updated_length, suffix_length, &updated_length) != 0 ||
				updated_length == SIZE_MAX) {
			return -1;
		}
		updated = malloc(updated_length + 1);
		if (updated == NULL) {
			return -1;
		}
		memcpy(updated, *text, prefix_length);
		memcpy(updated + prefix_length, value, value_length);
		memcpy(updated + prefix_length + value_length, *text + value_end,
				suffix_length + 1);
	} else {
		newline_length = use_crlf ? 2 : 1;
		separator_length = *length != 0 && (*text)[*length - 1] != '\n' ?
				newline_length : 0;
		updated_length = *length;
		if (add_size(updated_length, separator_length, &updated_length) != 0 ||
				add_size(updated_length, key_length, &updated_length) != 0 ||
				add_size(updated_length, 1, &updated_length) != 0 ||
				add_size(updated_length, value_length, &updated_length) != 0 ||
				add_size(updated_length, newline_length, &updated_length) != 0 ||
				updated_length == SIZE_MAX) {
			return -1;
		}
		updated = malloc(updated_length + 1);
		if (updated == NULL) {
			return -1;
		}
		cursor = 0;
		memcpy(updated, *text, *length);
		cursor += *length;
		if (separator_length != 0) {
			if (use_crlf) updated[cursor++] = '\r';
			updated[cursor++] = '\n';
		}
		memcpy(updated + cursor, key, key_length);
		cursor += key_length;
		updated[cursor++] = '=';
		memcpy(updated + cursor, value, value_length);
		cursor += value_length;
		if (use_crlf) updated[cursor++] = '\r';
		updated[cursor++] = '\n';
		updated[cursor] = '\0';
	}

	free(*text);
	*text = updated;
	*length = updated_length;
	return 0;
}
