#include "audio_effects.h"

#include <limits.h>
#include <math.h>

#define QRQ_TWO_PI 6.28318530717958647692

enum qrq_pileup_phase {
	QRQ_PILEUP_START_DELAY,
	QRQ_PILEUP_MARK,
	QRQ_PILEUP_ELEMENT_GAP,
	QRQ_PILEUP_CHARACTER_GAP,
	QRQ_PILEUP_WORD_GAP,
	QRQ_PILEUP_DONE
};

static uint32_t qrm_random(struct qrq_qrm_state *state) {
	state->random_state = state->random_state * 1664525U + 1013904223U;
	return state->random_state;
}

static uint32_t pileup_random(struct qrq_pileup_state *state) {
	state->random_state = state->random_state * 1664525U + 1013904223U;
	return state->random_state;
}

static const char *pileup_code(unsigned char character) {
	static const char *const letters[] = {
		".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....",
		"..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.",
		"--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-",
		"-.--", "--.."
	};
	static const char *const digits[] = {
		"-----", ".----", "..---", "...--", "....-", ".....",
		"-....", "--...", "---..", "----."
	};

	if (character >= 'a' && character <= 'z') {
		character = (unsigned char)(character - 'a' + 'A');
	}
	if (character >= 'A' && character <= 'Z') {
		return letters[character - 'A'];
	}
	if (character >= '0' && character <= '9') {
		return digits[character - '0'];
	}
	switch (character) {
		case '/': return "-..-.";
		case '+': return ".-.-.";
		case '.': return ".-.-.-";
		case ',': return "--..--";
		case '=': return "-...-";
		case '-': return "-....-";
		case '?': return "..--..";
		default: return "..--..";
	}
}

static size_t pileup_units(size_t dot_samples, unsigned int units) {
	if (dot_samples == 0 || dot_samples > SIZE_MAX / units) {
		return SIZE_MAX;
	}
	return dot_samples * units;
}

static void pileup_schedule_next(struct qrq_pileup_state *state) {
	const char *code;
	unsigned char character;

	state->transmitting = 0;
	while (state->phase_kind != QRQ_PILEUP_DONE) {
		switch (state->phase_kind) {
			case QRQ_PILEUP_START_DELAY:
				state->samples_remaining = pileup_units(state->dot_samples,
						1U + pileup_random(state) % 4U);
				state->phase_kind = QRQ_PILEUP_MARK;
				return;
			case QRQ_PILEUP_MARK:
				character = (unsigned char)state->text[state->text_index];
				if (character == '\0') {
					state->phase_kind = QRQ_PILEUP_DONE;
					return;
				}
				if (character == ' ') {
					state->text_index++;
					state->samples_remaining = pileup_units(state->dot_samples, 4U);
					state->phase_kind = QRQ_PILEUP_WORD_GAP;
					return;
				}
				code = pileup_code(character);
				if (code[state->code_index] == '\0') {
					state->text_index++;
					state->code_index = 0;
					state->samples_remaining = pileup_units(state->dot_samples, 2U);
					state->phase_kind = QRQ_PILEUP_CHARACTER_GAP;
					return;
				}
				state->transmitting = 1;
				state->samples_remaining = pileup_units(state->dot_samples,
						code[state->code_index++] == '-' ? 3U : 1U);
				state->phase_kind = QRQ_PILEUP_ELEMENT_GAP;
				return;
			case QRQ_PILEUP_ELEMENT_GAP:
				state->samples_remaining = state->dot_samples;
				state->phase_kind = QRQ_PILEUP_MARK;
				return;
			case QRQ_PILEUP_CHARACTER_GAP:
				state->phase_kind = QRQ_PILEUP_MARK;
				break;
			case QRQ_PILEUP_WORD_GAP:
				state->phase_kind = QRQ_PILEUP_MARK;
				break;
			default:
				state->phase_kind = QRQ_PILEUP_DONE;
				return;
		}
	}
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

void qrq_pileup_start(struct qrq_pileup_state *state, uint32_t seed,
		const char *text, size_t dot_samples, unsigned int primary_frequency) {
	unsigned int offset;
	unsigned int candidate;

	if (state == NULL) {
		return;
	}
	state->random_state = seed == 0 ? 0x4f1bbcdcU : seed;
	state->text = text;
	state->text_index = 0;
	state->code_index = 0;
	state->samples_remaining = 0;
	state->dot_samples = dot_samples == 0 ? 1 : dot_samples;
	state->carrier_frequency = 700;
	state->phase = 0.0;
	state->phase_kind = text == NULL || text[0] == '\0' ? QRQ_PILEUP_DONE :
		QRQ_PILEUP_START_DELAY;
	state->transmitting = 0;
	if (state->phase_kind == QRQ_PILEUP_DONE) {
		return;
	}
	offset = 100U + pileup_random(state) % 301U;
	if ((pileup_random(state) & 1U) != 0U && primary_frequency > offset + 300U) {
		candidate = primary_frequency - offset;
	} else if (primary_frequency <= 1600U - offset) {
		candidate = primary_frequency + offset;
	} else {
		candidate = primary_frequency > offset + 300U ? primary_frequency - offset : 700U;
	}
	state->carrier_frequency = candidate;
}

double qrq_pileup_next_sample(struct qrq_pileup_state *state,
		unsigned int sample_rate, int level) {
	double sample;

	if (state == NULL || state->phase_kind == QRQ_PILEUP_DONE ||
			sample_rate == 0 || level <= 0) {
		return 0.0;
	}
	if (level > 100) {
		level = 100;
	}
	if (state->samples_remaining == 0) {
		pileup_schedule_next(state);
	}
	if (state->phase_kind == QRQ_PILEUP_DONE) {
		return 0.0;
	}
	if (state->samples_remaining != 0) {
		state->samples_remaining--;
	}
	if (!state->transmitting) {
		return 0.0;
	}
	state->phase += QRQ_TWO_PI * (double)state->carrier_frequency /
		(double)sample_rate;
	if (state->phase >= QRQ_TWO_PI) {
		state->phase -= QRQ_TWO_PI;
	}
	sample = 0.70 * (double)level / 100.0 * sin(state->phase);
	return sample;
}
