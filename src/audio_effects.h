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

/* Stateful CW caller used for pileup practice.  It transmits one supplied
 * item at a nearby pitch and never participates in answer selection. */
struct qrq_pileup_state {
	uint32_t random_state;
	const char *text;
	size_t text_index;
	size_t code_index;
	size_t samples_remaining;
	size_t dot_samples;
	unsigned int carrier_frequency;
	double phase;
	int phase_kind;
	int transmitting;
};

/* Initialize deterministic, CW-like interfering bursts. The generator is
 * intentionally independent from practice selection and receiver-noise RNGs. */
void qrq_qrm_init(struct qrq_qrm_state *state, uint32_t seed);

/* Return one narrow-band interfering-CW sample in [-0.8, 0.8]. The level is
 * a percentage, while dot_samples controls the burst cadence. */
double qrq_qrm_next_sample(struct qrq_qrm_state *state,
		unsigned int sample_rate, size_t dot_samples, int level);

/* Start one delayed, independently pitched CW caller.  The supplied text
 * must stay valid until qrq_pileup_next_sample() is no longer called. */
void qrq_pileup_start(struct qrq_pileup_state *state, uint32_t seed,
		const char *text, size_t dot_samples, unsigned int primary_frequency);

/* Return one pileup sample in [-0.70, 0.70].  A zero level is silent. */
double qrq_pileup_next_sample(struct qrq_pileup_state *state,
		unsigned int sample_rate, int level);

#endif
