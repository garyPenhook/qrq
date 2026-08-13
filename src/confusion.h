#ifndef QRQ_CONFUSION_H
#define QRQ_CONFUSION_H

#include <stddef.h>
#include <time.h>

#define QRQ_CONFUSION_TOP_COUNT 5

struct qrq_confusion_pair {
	unsigned char expected;
	unsigned char received;
	size_t count;
};

struct qrq_confusion_summary {
	size_t errors;
	size_t pair_count;
	struct qrq_confusion_pair pairs[QRQ_CONFUSION_TOP_COUNT];
};

/* Append the character-level differences for one incorrect copy. A zero
 * expected or received symbol represents an extra or omitted character. */
int qrq_confusion_append(const char *path, const char *callsign,
		const char *sent, const char *received);

/* Read the most frequent character-level copy errors for one callsign. */
int qrq_confusion_summarize(const char *path, const char *callsign,
		struct qrq_confusion_summary *summary);

/* Summarize timestamped mistakes inside a rolling window.  A zero-day window
 * retains the historical, unweighted view.  A nonzero window favors newer
 * entries (up to four times their base weight) and omits older entries. */
int qrq_confusion_summarize_recent(const char *path, const char *callsign,
		unsigned int days, time_t now, struct qrq_confusion_summary *summary);

/* Return the unique symbols from the most frequent confusion pairs. The
 * caller supplies the output buffer and receives an empty string when no
 * recorded confusions are available. */
int qrq_confusion_focus_symbols(const char *path, const char *callsign,
		char *symbols, size_t capacity);

/* The windowed counterpart used by confusion-focused drills. */
int qrq_confusion_focus_symbols_recent(const char *path, const char *callsign,
		unsigned int days, time_t now, char *symbols, size_t capacity);

/* Replace one local confusion-history file with a fresh timestamped file.
 * Existing data is first renamed to "<path>.bak" and is never overwritten.
 * On success, backup receives that path; it is empty when no old file existed. */
int qrq_confusion_reset(const char *path, char *backup, size_t capacity);

#endif
