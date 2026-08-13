#include "callbase.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	const char *path = "test_callbase.qcb";
	struct qrq_callbase callbase = {0};
	struct qrq_callbase_filter filter = {
		.minimum_length = 1,
		.maximum_length = QRQ_CALLBASE_MAX_LENGTH,
	};
	FILE *file = fopen(path, "w");

	assert(file != NULL);
	fputs("k1abc\nDE\r\nWORD", file);
	assert(fclose(file) == 0);
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(callbase.count == 3 && callbase.max_length == 5);
	assert(strcmp(callbase.items[0], "K1ABC") == 0);
	assert(strcmp(callbase.items[1], "DE") == 0);
	assert(qrq_callbase_retain_symbols(&callbase, "1O") == 0);
	assert(callbase.count == 2 && callbase.max_length == 5);
	assert(strcmp(callbase.items[0], "K1ABC") == 0);
	assert(strcmp(callbase.items[1], "WORD") == 0);
	assert(qrq_callbase_retain_symbols(&callbase, "Z") == 1);
	assert(callbase.count == 2);
	qrq_callbase_free(&callbase);
	errno = 0;
	assert(qrq_callbase_generate_portable_variants(NULL) == -1 && errno == EINVAL);
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(qrq_callbase_generate_portable_variants(&callbase) == 0);
	assert(callbase.count == 9 && callbase.max_length == 8);
	assert(strcmp(callbase.items[0], "K1ABC/P") == 0);
	assert(strcmp(callbase.items[1], "K1ABC/M") == 0);
	assert(strcmp(callbase.items[2], "K1ABC/MM") == 0);
	assert(strcmp(callbase.items[3], "DE/P") == 0);
	qrq_callbase_free(&callbase);
	errno = 0;
	assert(qrq_callbase_generate_items(NULL, &callbase) == -1 && errno == EINVAL);
	errno = 0;
	assert(qrq_callbase_generate_items("", &callbase) == -1 && errno == ENODATA);
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(qrq_callbase_generate_items(" cq test , 5nn , ?, /P ",
			&callbase) == 0);
	assert(callbase.count == 4 && callbase.max_length == 7);
	assert(strcmp(callbase.items[0], "CQ TEST") == 0);
	assert(strcmp(callbase.items[1], "5NN") == 0);
	assert(strcmp(callbase.items[2], "?") == 0);
	assert(strcmp(callbase.items[3], "/P") == 0);
	qrq_callbase_free(&callbase);
	assert(qrq_callbase_generate_items("CQ,   ", &callbase) == 0);
	assert(callbase.count == 1 && strcmp(callbase.items[0], "CQ") == 0);
	qrq_callbase_free(&callbase);
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	errno = 0;
	assert(qrq_callbase_generate_items("CQ,,TU", &callbase) == -1 && errno == EINVAL);
	assert(callbase.count == 3 && strcmp(callbase.items[0], "K1ABC") == 0);
	qrq_callbase_free(&callbase);
	errno = 0;
	assert(qrq_callbase_generate_items("CQ,@", &callbase) == -1 && errno == EINVAL);

	errno = 0;
	assert(qrq_callbase_generate_serials(2, &callbase) == -1 && errno == EINVAL);
	assert(qrq_callbase_generate_serials(3, &callbase) == 0);
	assert(callbase.count == 1000 && callbase.max_length == 3);
	assert(strcmp(callbase.items[0], "000") == 0);
	assert(strcmp(callbase.items[42], "042") == 0);
	assert(strcmp(callbase.items[999], "999") == 0);
	qrq_callbase_free(&callbase);
	assert(qrq_callbase_generate_serials(QRQ_SERIAL_DIGITS_MAX, &callbase) == 0);
	assert(callbase.count == 100000 &&
			callbase.max_length == QRQ_SERIAL_DIGITS_MAX);
	assert(strcmp(callbase.items[0], "00000") == 0);
	assert(strcmp(callbase.items[99999], "99999") == 0);
	qrq_callbase_free(&callbase);

	filter.minimum_length = 3;
	filter.maximum_length = 4;
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(callbase.count == 1 && strcmp(callbase.items[0], "WORD") == 0);
	qrq_callbase_free(&callbase);

	filter.minimum_length = 1;
	filter.maximum_length = QRQ_CALLBASE_MAX_LENGTH;
	filter.prefixes = "K1,DE";
	filter.digit_mode = 0;
	filter.portable_mode = 0;
	filter.allowed_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/";
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(callbase.count == 2);
	qrq_callbase_free(&callbase);

	filter.prefixes = "";
	filter.digit_mode = 2;
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(callbase.count == 2);
	qrq_callbase_free(&callbase);

	file = fopen(path, "w");
	assert(file != NULL);
	fputs("K1ABC/P\nDE\n", file);
	assert(fclose(file) == 0);
	filter.digit_mode = 0;
	filter.portable_mode = 1;
	filter.allowed_chars = "";
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(callbase.count == 1 && strcmp(callbase.items[0], "K1ABC/P") == 0);
	qrq_callbase_free(&callbase);

	file = fopen(path, "w");
	assert(file != NULL);
	fputs("ABCDEFGHIJKLMNOPQRSTUVWXYZ123\n", file);
	assert(fclose(file) == 0);
	errno = 0;
	filter.minimum_length = 1;
	filter.maximum_length = QRQ_CALLBASE_MAX_LENGTH;
	assert(qrq_callbase_load(path, &filter, &callbase) == -1);
	assert(errno == EOVERFLOW);

	file = fopen(path, "w");
	assert(file != NULL);
	fputs("!!Order!! Call Name\nK1ABC\n", file);
	assert(fclose(file) == 0);
	filter.portable_mode = 0;
	assert(qrq_callbase_load(path, &filter, &callbase) == 0);
	assert(callbase.count == 1 && strcmp(callbase.items[0], "K1ABC") == 0);
	qrq_callbase_free(&callbase);
	remove(path);
	return 0;
}
