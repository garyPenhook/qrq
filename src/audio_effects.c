#include "audio_effects.h"

#include <limits.h>
#include <math.h>

#define QRQ_TWO_PI 6.28318530717958647692

static uint32_t qrm_random(struct qrq_qrm_state *state) {
	state->random_state = state->random_state * 1664525U + 1013904223U;
	return state->random_state;
}

void qrq_qrm_init(struct qrq_qrm_state *state, uint32_t seed) {
	if (state == NULL) {
		return;
	}
	state->random_state = seed == 0 ? 0x8f3d7ab1U : seed;
	state->samples_remaining = 0;
	state->carrier_frequency = 700;
	state->phase = 0.0;
	state->transmitting = 0;
}

static size_t qrm_burst_length(struct qrq_qrm_state *state,
		size_t dot_samples) {
	static const unsigned int units[] = {1, 1, 3, 7};
	unsigned int unit_count = units[qrm_random(state) %
			(sizeof(units) / sizeof(units[0]))];

	if (dot_samples > SIZE_MAX / unit_count) {
		return SIZE_MAX;
	}
	return dot_samples * unit_count;
}

double qrq_qrm_next_sample(struct qrq_qrm_state *state,
		unsigned int sample_rate, size_t dot_samples, int level) {
	double amplitude;
	double sample;

	if (state == NULL || sample_rate == 0 || dot_samples == 0 || level <= 0) {
		return 0.0;
	}
	if (level > 100) {
		level = 100;
	}
	if (state->samples_remaining == 0) {
		state->transmitting = (qrm_random(state) & 1U) != 0;
		state->samples_remaining = qrm_burst_length(state, dot_samples);
		state->carrier_frequency = 400U + qrm_random(state) % 801U;
	}
	state->phase += QRQ_TWO_PI * (double)state->carrier_frequency /
			(double)sample_rate;
	if (state->phase >= QRQ_TWO_PI) {
		state->phase -= QRQ_TWO_PI;
	}
	state->samples_remaining--;
	if (!state->transmitting) {
		return 0.0;
	}
	amplitude = 0.8 * (double)level / 100.0;
	sample = amplitude * sin(state->phase);
	return sample;
}
