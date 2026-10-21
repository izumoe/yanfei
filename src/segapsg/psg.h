/* SPDX-License-Identifier: BSD-3-Clause */
/* Sega PSG (the SN76489 variant in the Mega Drive's VDP): a C port of MAME's sn76496.cpp, integer only. See psg.c. */
#ifndef SEGAPSG_H
#define SEGAPSG_H
#include <stdint.h>

#define SEGAPSG_CLOCK 3579545u                     /* NTSC; the chip updates at SEGAPSG_CLOCK / 16 */

typedef struct {
    int32_t reg[8], last, volume[4], period[4], count[4], output[4];
    uint32_t rng;
    uint32_t frac;                              /* segapsg_render: ticks owed, Q16 */
    int32_t held;                               /* .. its last sample */
} segapsg_t;

void segapsg_reset(segapsg_t *p);
void segapsg_write(segapsg_t *p, uint8_t data);         /* a byte to the chip's port, as on the Mega Drive */
int32_t segapsg_tick(segapsg_t *p);                     /* one update; returns the output (MAME's sample, negated as the chip) */
void segapsg_render(segapsg_t *p, int32_t *out, uint32_t n, uint32_t step_q16);   /* adds n samples to out (L R L R),
                                                   each the mean of the step_q16 / 65536 ticks it spans */

#endif
