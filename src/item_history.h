#ifndef QRQ_ITEM_HISTORY_H
#define QRQ_ITEM_HISTORY_H

#include <stddef.h>
#include <stdint.h>

#define QRQ_ITEM_HISTORY_ITEM_MAX 28
#define QRQ_ITEM_HISTORY_TOP_COUNT 3

struct qrq_item_history_item {
	char sent[QRQ_ITEM_HISTORY_ITEM_MAX + 1];
	size_t attempts;
	size_t errors;
};

struct qrq_item_history_summary {
	size_t attempts;
	size_t correct;
	uint64_t total_response_ms;
	size_t difficult_count;
	struct qrq_item_history_item difficult[QRQ_ITEM_HISTORY_TOP_COUNT];
};

/* Append one completed copy. Response time is measured from the first send
 * until Enter, with repeated transmissions excluded by the caller. */
int qrq_item_history_append(const char *path, const char *callsign,
		const char *sent, int copied_correctly, uint64_t response_ms);

/* Summarize per-item accuracy and response time for one callsign. */
int qrq_item_history_summarize(const char *path, const char *callsign,
		struct qrq_item_history_summary *summary);

/* Mark loaded items that are due for persistent review. A wrong copy is due
 * immediately; successive correct copies use intervals of 1, 3, 7, 15, and
 * 31 subsequent attempts. A missing history file produces no due items. */
int qrq_item_history_schedule(const char *path, const char *callsign,
		const char *const *items, size_t item_count, unsigned char *due);

#endif
