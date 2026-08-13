#ifndef QRQ_AUDIO_EFFECTS_H
#define QRQ_AUDIO_EFFECTS_H

#include <stddef.h>
#include <stdint.h>

struct qrq_qrm_state {
	uint32_t random_state;
	size_t samples_remaining;
	unsigned int carrier_frequency;
	double phase;
	int transmitting;
};

/* Initialize deterministic, CW-like interfering bursts. The generator is
 * intentionally independent from practice selection and receiver-noise RNGs. */
void qrq_qrm_init(struct qrq_qrm_state *state, uint32_t seed);

/* Return one narrow-band interfering-CW sample in [-0.8, 0.8]. The level is
 * a percentage, while dot_samples controls the burst cadence. */
double qrq_qrm_next_sample(struct qrq_qrm_state *state,
		unsigned int sample_rate, size_t dot_samples, int level);

#endif
