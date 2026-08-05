#include "config.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
	char copied[8];
	char literal_hash[] = "key=value#literal";
	char malformed[] = "broken";
	char invalid_key[] = "bad-key=value";
	char *buffer = NULL;
	char *key;
	char *value_text;
	double decimal;
	size_t capacity = 0;
	FILE *file = tmpfile();
	int integer;
	unsigned int value = 99;

	assert(file != NULL);
	fputs("  # comment\r\nkey = value with spaces  # note\r\nlast=42", file);
	rewind(file);
	assert(qrq_config_read_line(file, &buffer, &capacity) == 1);
	assert(qrq_config_split_line(buffer, &key, &value_text) == 0);
	assert(qrq_config_read_line(file, &buffer, &capacity) == 1);
	assert(qrq_config_split_line(buffer, &key, &value_text) == 1);
	assert(strcmp(key, "key") == 0 && strcmp(value_text, "value with spaces") == 0);
	assert(qrq_config_read_line(file, &buffer, &capacity) == 1);
	assert(strcmp(buffer, "last=42") == 0);
	assert(qrq_config_split_line(buffer, &key, &value_text) == 1);
	assert(strcmp(key, "last") == 0 && strcmp(value_text, "42") == 0);
	assert(qrq_config_read_line(file, &buffer, &capacity) == 0);
	assert(fclose(file) == 0);
	free(buffer);
	buffer = NULL;
	capacity = 0;

	file = tmpfile();
	assert(file != NULL);
	for (size_t index = 0; index < QRQ_CONFIG_MAX_LINE; index++) {
		assert(fputc('x', file) == 'x');
	}
	assert(fputc('\n', file) == '\n');
	assert(fputc('y', file) == 'y');
	rewind(file);
	assert(qrq_config_read_line(file, &buffer, &capacity) == 1);
	assert(strlen(buffer) == QRQ_CONFIG_MAX_LINE);
	assert(qrq_config_read_line(file, &buffer, &capacity) == 1);
	assert(strcmp(buffer, "y") == 0);
	assert(fclose(file) == 0);
	free(buffer);
	buffer = NULL;
	capacity = 0;

	file = tmpfile();
	assert(file != NULL);
	for (size_t index = 0; index <= QRQ_CONFIG_MAX_LINE; index++) {
		assert(fputc('x', file) == 'x');
	}
	rewind(file);
	assert(qrq_config_read_line(file, &buffer, &capacity) == -1);
	assert(fclose(file) == 0);
	free(buffer);

	assert(qrq_config_parse_int("10", 10, 20, &integer) == 0 && integer == 10);
	assert(qrq_config_parse_int("9", 10, 20, &integer) == -1);
	assert(qrq_config_parse_int("+10", 10, 20, &integer) == -1);
	assert(qrq_config_parse_int("10x", 10, 20, &integer) == -1);
	assert(qrq_config_parse_uint("0", &value) == 0 && value == 0);
	assert(qrq_config_parse_uint("123", &value) == 0 && value == 123);
	assert(qrq_config_parse_uint("", &value) == -1);
	assert(qrq_config_parse_uint("-1", &value) == -1);
	assert(qrq_config_parse_uint("12x", &value) == -1);
	assert(qrq_config_parse_uint("42949672960", &value) == -1);
	assert(qrq_config_parse_uint("1", NULL) == -1);
	assert(qrq_config_parse_uint(NULL, &value) == -1);
	assert(qrq_config_parse_double("2.5", 0.1, 10.0, &decimal) == 0 && decimal == 2.5);
	assert(qrq_config_parse_double("nan", 0.1, 10.0, &decimal) == -1);
	assert(qrq_config_parse_double("inf", 0.1, 10.0, &decimal) == -1);
	assert(qrq_config_parse_double("11", 0.1, 10.0, &decimal) == -1);
	assert(qrq_config_copy_string("ab/C", copied, sizeof(copied), 1) == 0);
	assert(strcmp(copied, "AB/C") == 0);
	assert(qrq_config_copy_string("toolong!", copied, sizeof(copied), 0) == -1);
	assert(qrq_config_split_line(malformed, &key, &value_text) == -1);
	assert(qrq_config_split_line(invalid_key, &key, &value_text) == -1);
	assert(qrq_config_split_line(literal_hash, &key, &value_text) == 1);
	assert(strcmp(value_text, "value#literal") == 0);
	return 0;
}
