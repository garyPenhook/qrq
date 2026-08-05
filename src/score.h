#ifndef QRQ_SCORE_H
#define QRQ_SCORE_H

#include <stddef.h>

#define QRQ_SPEED_MIN 10
#define QRQ_SPEED_MAX 5000
#define QRQ_SESSION_SCORE_MAX 999999

struct qrq_score_state {
	int speed;
	int maxspeed;
	int error_count;
	int fixed_speed;
	int attempt_valid;
	int speed_up_step;
	int speed_down_step;
};

/* Scores one copied item, updates session state, and writes a displayable
 * difference string. Returns zero if the supplied output buffer is too small. */
int qrq_score_attempt(struct qrq_score_state *state, const char *sent,
		const char *input, int transmitted_speed, char *difference,
		size_t difference_size);

/* Accumulates nonnegative item points within the fixed-width toplist range. */
int qrq_score_accumulate(int total, int points);

#endif
