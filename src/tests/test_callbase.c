#include "callbase.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	const char *path = "test_callbase.qcb";
	struct qrq_callbase callbase = {0};
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	fputs("k1abc\nDE\r\nWORD\n", file);
	assert(fclose(file) == 0);
	assert(qrq_callbase_load(path, 1, QRQ_CALLBASE_MAX_LENGTH, &callbase) == 0);
	assert(callbase.count == 3 && callbase.max_length == 5);
	assert(strcmp(callbase.items[0], "K1ABC") == 0);
	assert(strcmp(callbase.items[1], "DE") == 0);
	qrq_callbase_free(&callbase);

	assert(qrq_callbase_load(path, 3, 4, &callbase) == 0);
	assert(callbase.count == 1 && strcmp(callbase.items[0], "WORD") == 0);
	qrq_callbase_free(&callbase);

	file = fopen(path, "w");
	assert(file != NULL);
	fputs("ABCDEFGHIJKLMNOPQRSTUVWXYZ123\n", file);
	assert(fclose(file) == 0);
	errno = 0;
	assert(qrq_callbase_load(path, 1, QRQ_CALLBASE_MAX_LENGTH, &callbase) == -1);
	assert(errno == EOVERFLOW);
	remove(path);
	return 0;
}
