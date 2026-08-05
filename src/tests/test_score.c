#include "score.h"

#include <assert.h>
#include <string.h>

static struct qrq_score_state state(int speed) {
	return (struct qrq_score_state){
		.speed = speed,
		.maxspeed = 0,
		.error_count = 0,
		.fixed_speed = 0,
		.attempt_valid = 1,
		.speed_up_step = 10,
		.speed_down_step = 10,
	};
}

int main(void) {
	char difference[32];
	struct qrq_score_state session = state(100);

	assert(qrq_score_attempt(&session, "K1ABC", "K1ABC", 100, difference,
			sizeof(difference)) == 1000);
	assert(strcmp(difference, "-") == 0);
	assert(session.speed == 110 && session.maxspeed == 100 && session.error_count == 0);

	assert(qrq_score_attempt(&session, "K1ABC", "K1ABD", 110, difference,
			sizeof(difference)) == 220);
	assert(strcmp(difference, "K1ABd") == 0);
	assert(session.speed == 100 && session.error_count == 1);

	assert(qrq_score_attempt(&session, "ABC", "A", 100, difference,
			sizeof(difference)) == 60);
	assert(strcmp(difference, "A  ") == 0);
	assert(session.speed == 90 && session.error_count == 2);

	session.fixed_speed = 1;
	assert(qrq_score_attempt(&session, "A", "A", 100, difference,
			sizeof(difference)) == 200);
	assert(session.speed == 90);

	session.attempt_valid = 0;
	assert(qrq_score_attempt(&session, "A", "A", 100, difference,
			sizeof(difference)) == 0);
	return 0;
}
