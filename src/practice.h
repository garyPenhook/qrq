#ifndef QRQ_PRACTICE_H
#define QRQ_PRACTICE_H

#include <stddef.h>
#include <stdint.h>

#define QRQ_PRACTICE_NO_ITEM ((size_t)-1)

/* Select an unused callbase item. In adaptive mode, each prior mistake adds
 * one selection weight (capped to keep the calculation bounded). */
size_t qrq_practice_choose(size_t count, const unsigned char *used,
		const unsigned char *mistakes, int adaptive, uint32_t random_value);

#endif
