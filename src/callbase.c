#include "callbase.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int normalize_line(char *line, size_t *length, int at_end_of_file) {
	size_t line_length = strlen(line);
	if (line_length == 0) {
		return -1;
	}
	if (line[line_length - 1] == '\n') {
		line[--line_length] = '\0';
	} else if (!at_end_of_file) {
		return -1;
	}
	if (line_length != 0 && line[line_length - 1] == '\r') {
		line[--line_length] = '\0';
	}
	*length = line_length;
	return 0;
}

static int has_prefix(const char *line, const char *prefixes) {
	const char *prefix = prefixes;

	if (prefixes == NULL || *prefixes == '\0') {
		return 1;
	}
	while (*prefix != '\0') {
		const char *end = strchr(prefix, ',');
		size_t length = end == NULL ? strlen(prefix) : (size_t)(end - prefix);
		if (length != 0 && strncmp(line, prefix, length) == 0) {
			return 1;
		}
		if (end == NULL) {
			break;
		}
		prefix = end + 1;
	}
	return 0;
}

static int matches_filter(const char *line, size_t length,
		const struct qrq_callbase_filter *filter) {
	size_t i;
	int has_digit = 0;
	int is_portable = 0;

	if (length < filter->minimum_length || length > filter->maximum_length ||
			!has_prefix(line, filter->prefixes)) {
		return 0;
	}
	for (i = 0; i < length; i++) {
		if (isdigit((unsigned char)line[i])) {
			has_digit = 1;
		}
		if (line[i] == '/' && i + 1 < length) {
			is_portable = 1;
		}
		if (filter->allowed_chars != NULL && *filter->allowed_chars != '\0' &&
				strchr(filter->allowed_chars, line[i]) == NULL) {
			return 0;
		}
	}
	if ((filter->digit_mode == 1 && !has_digit) ||
			(filter->digit_mode == 2 && has_digit) ||
			(filter->portable_mode == 1 && !is_portable) ||
			(filter->portable_mode == 2 && is_portable)) {
		return 0;
	}
	return 1;
}

static void uppercase_line(char *line, size_t length) {
	size_t i;
	for (i = 0; i < length; i++) {
		line[i] = (char)toupper((unsigned char)line[i]);
	}
}

static int has_supported_characters(const char *line, size_t length) {
	static const char supported[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/+.,= ?";
	size_t i;

	for (i = 0; i < length; i++) {
		if (strchr(supported, line[i]) == NULL) {
			return 0;
		}
	}
	return 1;
}

void qrq_callbase_free(struct qrq_callbase *callbase) {
	size_t i;
	if (callbase == NULL) {
		return;
	}
	for (i = 0; i < callbase->count; i++) {
		free(callbase->items[i]);
	}
	free(callbase->items);
	callbase->items = NULL;
	callbase->count = 0;
	callbase->max_length = 0;
}

int qrq_callbase_generate_serials(unsigned int digits,
		struct qrq_callbase *callbase) {
	char serial[QRQ_SERIAL_DIGITS_MAX + 1];
	size_t count = 1;
	size_t index;

	if (callbase == NULL || digits < QRQ_SERIAL_DIGITS_MIN ||
			digits > QRQ_SERIAL_DIGITS_MAX) {
		errno = EINVAL;
		return -1;
	}
	callbase->items = NULL;
	callbase->count = 0;
	callbase->max_length = 0;
	for (index = 0; index < digits; ++index) {
		count *= 10;
	}
	callbase->items = calloc(count, sizeof(*callbase->items));
	if (callbase->items == NULL) {
		return -1;
	}
	callbase->count = count;
	callbase->max_length = digits;
	for (index = 0; index < count; ++index) {
		int written = snprintf(serial, sizeof(serial), "%0*u", (int)digits,
				(unsigned int)index);

		if (written != (int)digits) {
			errno = EOVERFLOW;
			qrq_callbase_free(callbase);
			return -1;
		}
		callbase->items[index] = malloc((size_t)written + 1);
		if (callbase->items[index] == NULL) {
			qrq_callbase_free(callbase);
			return -1;
		}
		memcpy(callbase->items[index], serial, (size_t)written + 1);
	}
	return 0;
}

int qrq_callbase_retain_symbols(struct qrq_callbase *callbase,
		const char *symbols) {
	size_t kept = 0;
	size_t index;
	size_t max_length = 0;

	if (callbase == NULL || symbols == NULL || symbols[0] == '\0' ||
			callbase->items == NULL || callbase->count == 0) {
		errno = EINVAL;
		return -1;
	}
	for (index = 0; index < callbase->count; ++index) {
		if (strpbrk(callbase->items[index], symbols) != NULL) {
			kept++;
		}
	}
	if (kept == 0) {
		return 1;
	}
	kept = 0;
	for (index = 0; index < callbase->count; ++index) {
		char *item = callbase->items[index];

		if (strpbrk(item, symbols) != NULL) {
			size_t length = strlen(item);

			callbase->items[kept++] = item;
			if (length > max_length) {
				max_length = length;
			}
		} else {
			free(item);
		}
	}
	for (index = kept; index < callbase->count; ++index) {
		callbase->items[index] = NULL;
	}
	callbase->count = kept;
	callbase->max_length = max_length;
	return 0;
}

int qrq_callbase_load(const char *path, const struct qrq_callbase_filter *filter,
		struct qrq_callbase *callbase) {
	char line[QRQ_CALLBASE_MAX_LENGTH + 3];
	FILE *file;
	size_t line_length;
	size_t selected = 0;
	size_t max_length = 0;
	size_t index = 0;
	int read_status = -1;

	if (path == NULL || filter == NULL || callbase == NULL ||
			filter->minimum_length == 0 ||
			filter->maximum_length < filter->minimum_length ||
			filter->maximum_length > QRQ_CALLBASE_MAX_LENGTH ||
			filter->digit_mode < 0 || filter->digit_mode > 2 ||
			filter->portable_mode < 0 || filter->portable_mode > 2) {
		errno = EINVAL;
		return -1;
	}
	callbase->items = NULL;
	callbase->count = 0;
	callbase->max_length = 0;
	file = fopen(path, "r");
	if (file == NULL) {
		return -1;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		if (normalize_line(line, &line_length, feof(file)) != 0) {
			errno = EOVERFLOW;
			goto cleanup;
		}
		if (line_length > QRQ_CALLBASE_MAX_LENGTH) {
			errno = EOVERFLOW;
			goto cleanup;
		}
		uppercase_line(line, line_length);
		if (has_supported_characters(line, line_length) &&
				matches_filter(line, line_length, filter)) {
			selected++;
			if (line_length > max_length) {
				max_length = line_length;
			}
		}
	}
	if (ferror(file) || selected == 0) {
		errno = selected == 0 ? ENODATA : EIO;
		goto cleanup;
	}
	callbase->items = calloc(selected, sizeof(*callbase->items));
	if (callbase->items == NULL) {
		goto cleanup;
	}
	callbase->count = selected;
	callbase->max_length = max_length;
	if (fseek(file, 0, SEEK_SET) != 0) {
		goto cleanup;
	}
	while (fgets(line, sizeof(line), file) != NULL) {
		size_t i;
		if (normalize_line(line, &line_length, feof(file)) != 0) {
			errno = EOVERFLOW;
			goto cleanup;
		}
		if (line_length > QRQ_CALLBASE_MAX_LENGTH) {
			errno = EOVERFLOW;
			goto cleanup;
		}
		uppercase_line(line, line_length);
		if (!has_supported_characters(line, line_length) ||
				!matches_filter(line, line_length, filter)) {
			continue;
		}
		/* The database may have been replaced between the counting and
		 * loading passes. Never trust the earlier count as an array bound. */
		if (index >= selected) {
			errno = EOVERFLOW;
			goto cleanup;
		}
		callbase->items[index] = malloc(line_length + 1);
		if (callbase->items[index] == NULL) {
			goto cleanup;
		}
		for (i = 0; i < line_length; i++) {
			callbase->items[index][i] = (char)toupper((unsigned char)line[i]);
		}
		callbase->items[index][line_length] = '\0';
		index++;
	}
	if (ferror(file) || index != selected) {
		errno = EIO;
		goto cleanup;
	}
	read_status = 0;

cleanup:
	if (fclose(file) != 0 && read_status == 0) {
		read_status = -1;
	}
	if (read_status != 0) {
		qrq_callbase_free(callbase);
	}
	return read_status;
}
