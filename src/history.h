#ifndef QRQ_HISTORY_H
#define QRQ_HISTORY_H

#include <time.h>
#include <stddef.h>

struct qrq_history_entry {
	time_t timestamp;
	const char *callsign;
	int calls;
	int errors;
	int score;
	int max_speed;
	int eligible;
};

struct qrq_history_summary {
	size_t sessions;
	int average_score;
	int average_accuracy;
	int best_score;
	int best_speed;
	int first_score;
	int last_score;
};

int qrq_history_append(const char *path, const struct qrq_history_entry *entry);
int qrq_history_summarize(const char *path, const char *callsign,
		struct qrq_history_summary *summary);

#endif
