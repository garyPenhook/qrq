#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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
