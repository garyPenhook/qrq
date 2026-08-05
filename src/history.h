#ifndef QRQ_HISTORY_H
#define QRQ_HISTORY_H

#include <time.h>

struct qrq_history_entry {
	time_t timestamp;
	const char *callsign;
	int calls;
	int errors;
	int score;
	int max_speed;
	int eligible;
};

int qrq_history_append(const char *path, const struct qrq_history_entry *entry);

#endif
