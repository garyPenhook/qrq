#include "history.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	const char *path = "test_history.csv";
	const struct qrq_history_entry entry = {100, "K1ABC", 50, 2, 123, 200, 1};
	char contents[128] = "";
	FILE *file;

	assert(qrq_history_append(path, &entry) == 0);
	assert(qrq_history_append(path, &entry) == 0);
	file = fopen(path, "r");
	assert(file != NULL);
	assert(fread(contents, 1, sizeof(contents) - 1, file) > 0);
	assert(fclose(file) == 0);
	assert(strcmp(contents, "qrq-history-v1\n100,K1ABC,50,2,123,200,1\n100,K1ABC,50,2,123,200,1\n") == 0);
	remove(path);
	return 0;
}
