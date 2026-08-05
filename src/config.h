#ifndef QRQ_CONFIG_H
#define QRQ_CONFIG_H

#include <stddef.h>
#include <stdio.h>

#define QRQ_CONFIG_MAX_LINE 16384

/* Read one complete line without its LF or optional CR. The buffer is owned by
 * the caller and may be reused across calls. Returns 1, 0 at EOF, or -1. */
int qrq_config_read_line(FILE *file, char **buffer, size_t *capacity);

/* Split a mutable line into trimmed key and value fields. Inline comments
 * begin with '#' at the start of a value or after whitespace. Returns 1 for an
 * entry, 0 for a blank/comment line, or -1 for malformed input. */
int qrq_config_split_line(char *line, char **key, char **value);

int qrq_config_parse_int(const char *text, int minimum, int maximum, int *value);

/* Parse a complete unsigned decimal value. Whitespace/comments are removed by
 * qrq_config_split_line(). */
int qrq_config_parse_uint(const char *text, unsigned int *value);

int qrq_config_parse_double(const char *text, double minimum, double maximum,
		double *value);
int qrq_config_copy_string(const char *text, char *destination, size_t capacity,
		int uppercase);

#endif
