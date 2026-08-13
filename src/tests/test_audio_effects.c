#include "audio_effects.h"

#include <assert.h>
#include <math.h>

int main(void) {
	struct qrq_qrm_state first;
	struct qrq_qrm_state second;
	struct qrq_pileup_state first_pileup;
	struct qrq_pileup_state second_pileup;
	int saw_signal = 0;
	int saw_silence = 0;
	size_t index;

	qrq_qrm_init(&first, 123U);
	qrq_qrm_init(&second, 123U);
	assert(qrq_qrm_next_sample(NULL, 8000U, 80U, 50) == 0.0);
	assert(qrq_qrm_next_sample(&first, 0U, 80U, 50) == 0.0);
	assert(qrq_qrm_next_sample(&first, 8000U, 0U, 50) == 0.0);
	assert(qrq_qrm_next_sample(&first, 8000U, 80U, 0) == 0.0);

	qrq_qrm_init(&first, 123U);
	for (index = 0; index < 20000; ++index) {
		double first_sample = qrq_qrm_next_sample(&first, 8000U, 80U, 75);
		double second_sample = qrq_qrm_next_sample(&second, 8000U, 80U, 75);

		assert(first_sample == second_sample);
		assert(isfinite(first_sample));
		assert(first_sample >= -0.6000001 && first_sample <= 0.6000001);
		if (fabs(first_sample) > 0.000001) {
			saw_signal = 1;
		} else {
			saw_silence = 1;
		}
	}
	assert(saw_signal);
	assert(saw_silence);

	saw_signal = 0;
	saw_silence = 0;
	qrq_pileup_start(NULL, 123U, "K1ABC", 80U, 700U);
	qrq_pileup_start(&first_pileup, 123U, NULL, 80U, 700U);
	assert(qrq_pileup_next_sample(NULL, 8000U, 50) == 0.0);
	assert(qrq_pileup_next_sample(&first_pileup, 8000U, 50) == 0.0);
	qrq_pileup_start(&first_pileup, 123U, "K1ABC", 80U, 700U);
	qrq_pileup_start(&second_pileup, 123U, "K1ABC", 80U, 700U);
	for (index = 0; index < 20000; ++index) {
		double first_sample = qrq_pileup_next_sample(&first_pileup, 8000U, 75);
		double second_sample = qrq_pileup_next_sample(&second_pileup, 8000U, 75);

		assert(first_sample == second_sample);
		assert(isfinite(first_sample));
		assert(first_sample >= -0.5250001 && first_sample <= 0.5250001);
		if (fabs(first_sample) > 0.000001) {
			saw_signal = 1;
		} else {
			saw_silence = 1;
		}
	}
	assert(saw_signal);
	assert(saw_silence);
	return 0;
}
