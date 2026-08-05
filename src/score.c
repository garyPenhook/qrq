#include "score.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

static int bounded_score(size_t length, int speed, int divisor) {
	if (length == 0 || speed <= 0 || divisor <= 0 ||
			length > (size_t)INT_MAX / 2 || speed > INT_MAX / (int)(2 * length)) {
		return 0;
	}
	return (2 * (int)length * speed) / divisor;
}

int qrq_score_attempt(struct qrq_score_state *state, const char *sent,
		const char *input, int transmitted_speed, char *difference,
		size_t difference_size) {
	size_t sent_length;
	size_t input_length;
	size_t display_length;
	size_t i;
	int mistakes = 0;
	int score = 0;

	if (state == NULL || sent == NULL || input == NULL || difference == NULL) {
		return 0;
	}
	sent_length = strlen(sent);
	input_length = strlen(input);
	display_length = sent_length > input_length ? sent_length : input_length;
	if (display_length == SIZE_MAX || difference_size < display_length + 1) {
		return 0;
	}

	if (strcmp(input, sent) == 0) {
		difference[0] = '-';
		difference[1] = '\0';
		if (state->speed > state->maxspeed) {
			state->maxspeed = state->speed;
		}
		if (!state->fixed_speed && state->speed_up_step > 0 &&
				state->speed <= INT_MAX - state->speed_up_step) {
			state->speed += state->speed_up_step;
		}
		if (state->attempt_valid) {
			score = bounded_score(sent_length, transmitted_speed, 1);
		}
		return score;
	}

	state->error_count++;
	for (i = 0; i < display_length; i++) {
		char sent_char = i < sent_length ? sent[i] : '\0';
		char input_char = i < input_length ? input[i] : '\0';
		if (sent_char != input_char) {
			mistakes++;
			difference[i] = input_char == '\0' ? ' ' :
				(char)tolower((unsigned char)input_char);
		}
		else {
			difference[i] = input_char;
		}
	}
	difference[display_length] = '\0';
	if (!state->fixed_speed && state->speed > 20 && state->speed_down_step > 0) {
		state->speed -= state->speed_down_step;
		if (state->speed < 20) {
			state->speed = 20;
		}
	}
	if (mistakes > 0 && mistakes < 4 && state->attempt_valid) {
		score = bounded_score(display_length, transmitted_speed, 5 * mistakes);
	}
	return score;
}
