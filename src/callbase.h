#ifndef QRQ_CALLBASE_H
#define QRQ_CALLBASE_H

#include <stddef.h>

#define QRQ_CALLBASE_MAX_LENGTH 28

struct qrq_callbase {
	char **items;
	size_t count;
	size_t max_length;
};

int qrq_callbase_load(const char *path, size_t minimum_length,
		size_t maximum_length, struct qrq_callbase *callbase);
void qrq_callbase_free(struct qrq_callbase *callbase);

#endif
