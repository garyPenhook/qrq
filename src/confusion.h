#ifndef QRQ_CONFUSION_H
#define QRQ_CONFUSION_H

#include <stddef.h>

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

/* Return the unique symbols from the most frequent confusion pairs. The
 * caller supplies the output buffer and receives an empty string when no
 * recorded confusions are available. */
int qrq_confusion_focus_symbols(const char *path, const char *callsign,
		char *symbols, size_t capacity);

#endif
