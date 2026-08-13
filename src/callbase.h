#ifndef QRQ_CALLBASE_H
#define QRQ_CALLBASE_H

#include <stddef.h>

#define QRQ_CALLBASE_MAX_LENGTH 28
#define QRQ_SERIAL_DIGITS_MIN 3
#define QRQ_SERIAL_DIGITS_MAX 5

struct qrq_callbase {
	char **items;
	size_t count;
	size_t max_length;
};

struct qrq_callbase_filter {
	size_t minimum_length;
	size_t maximum_length;
	const char *prefixes;       /* comma-separated; empty means all */
	int digit_mode;             /* 0 any, 1 required, 2 excluded */
	int portable_mode;          /* 0 any, 1 required, 2 excluded */
	const char *allowed_chars;  /* empty means all */
};

int qrq_callbase_load(const char *path, const struct qrq_callbase_filter *filter,
		struct qrq_callbase *callbase);

/* Create zero-padded sequential serial exchanges (000..999 through
 * 00000..99999). The generated items replace a callbase for serial-copy
 * practice and are independent of the selected callsign database. */
int qrq_callbase_generate_serials(unsigned int digits,
		struct qrq_callbase *callbase);

/* Replace eligible base calls with portable /P, /M, and /MM variants. */
int qrq_callbase_generate_portable_variants(struct qrq_callbase *callbase);

/* Retain items containing at least one character in symbols. Returns one
 * without changing the callbase when no item would remain. */
int qrq_callbase_retain_symbols(struct qrq_callbase *callbase,
		const char *symbols);
void qrq_callbase_free(struct qrq_callbase *callbase);

#endif
