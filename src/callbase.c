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

int qrq_callbase_load(const char *path, size_t minimum_length,
		size_t maximum_length, struct qrq_callbase *callbase) {
	char line[QRQ_CALLBASE_MAX_LENGTH + 3];
	FILE *file;
	size_t line_length;
	size_t selected = 0;
	size_t max_length = 0;
	size_t index = 0;
	int read_status = -1;

	if (path == NULL || callbase == NULL || minimum_length == 0 ||
			maximum_length < minimum_length || maximum_length > QRQ_CALLBASE_MAX_LENGTH) {
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
		if (line_length >= minimum_length && line_length <= maximum_length) {
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
		if (line_length < minimum_length || line_length > maximum_length) {
			continue;
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
