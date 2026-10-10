/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: the voice, for TALK (a key says its letter) and the pets' names.
 *
 * Linear predictive speech, the way the talking toys of 1978 spoke: each word was said once and analysed (by
 * tools/gen_pm_speech.py, into pm_speech_data.h) into a frame of numbers every 12.5 ms: how loud, the pitch or "no
 * pitch", and ten reflection coefficients, the shape of the mouth. Here those frames are played back at 8 kHz:
 *   the excitation   a voiced frame: PM_LPC_CHIRP once a pitch period (an impulse with its frequencies spread in
 *                    time); an unvoiced frame: +-PM_LPC_NOISE at random; either times the frame's energy
 *   the mouth        a ten-stage lattice filter of the reflection coefficients (stable for any |k| < 1, so for
 *                    any frame and anything between two frames)
 *   between frames   energy, pitch and coefficients move in a straight line to the next frame's in four steps
 *                    of 25 samples. Not from silence and not between a voiced frame and an unvoiced one: there
 *                    the new frame begins at once (a stop's burst is a burst)
 * and the 8 kHz samples are drawn out to the audio rate PM_SPEECH_FS in straight lines. The frame's layout in bits
 * is in the tool's header comment. Integer arithmetic only (Q15 coefficients, 32-bit states, 64-bit products);
 * ten stages at 8 kHz are about as much work as one FM operator. Nothing here touches the hardware:
 * tests/pm_speech_test.c renders it on a host.
 *
 *   pm_speech_init()                 power-on
 *   pm_speech_say(word)              PM_W_A .. PM_W_Z a letter's name, then the words; cuts a word still going
 *   pm_speech_render(buf, n)         n samples of what is being said into buf (added), 0 when silent
 *   pm_speech_busy()                 1 while a word sounds
 *   pm_speech_letter(word)           'A' .. 'Z' for a letter, 0 for a word (the screen shows the letter) */
#ifndef PM_SPEECH_C
#define PM_SPEECH_C
#include <stdint.h>
#include "pm_speech_data.h"

#ifndef PM_SPEECH_FS
#define PM_SPEECH_FS 44100u
#endif
#define PM_LPC_STEP (PM_LPC_FRAME / PM_LPC_STEPS)         /* samples an interpolation step */
#define PM_LPC_INC ((uint32_t)(((uint64_t)PM_LPC_FS << 32) / PM_SPEECH_FS))   /* 8 kHz samples an output sample, 0.32 (a constant: no division on the device) */
#define PM_SPEECH_CEIL 30000                     /* no sample beyond this (the words are made to peak under it) */

typedef struct { int32_t e, p, k[PM_LPC_ORDER]; } pms_par_t;   /* energy, period (0 unvoiced), coefficients Q15 */
static struct {
    uint8_t on, end, over;                       /* something sounds; the word's last frame has been read; and played */
    uint8_t word;                                /* the word said last (or being said) */
    uint8_t step, cnt;                           /* the interpolation step 0 .. PM_LPC_STEPS - 1, the sample in it */
    uint32_t bit;                                /* where in PM_LPC_DATA, in bits */
    pms_par_t from, to, now;                     /* the frame's start, its target, the step's values */
    int32_t b[PM_LPC_ORDER + 1];                 /* the lattice */
    uint32_t pc;                                 /* samples into the pitch period */
    uint32_t rnd;
    uint32_t ph;                                 /* the 8 kHz sample's phase at the audio rate */
    int32_t s0, s1;                              /* the 8 kHz samples the output is between */
    int32_t cut;                                 /* where a word was when another cut it: dying away under the new one */
} spk;

static uint32_t pms_bits(uint32_t n)             /* the next n bits of the word, the high bit first */
{
    uint32_t v = 0;
    while (n--) {
        uint32_t byte = spk.bit >> 3;
        v = (v << 1) | (byte < sizeof PM_LPC_DATA ? (PM_LPC_DATA[byte] >> (7u - (spk.bit & 7u))) & 1u : 1u);   /* (past the end: 1s, the stop code) */
        spk.bit++;
    }
    return v;
}
/* the next frame: what was the target is where it starts; -> 0 when the word is over */
static int pms_frame(void)
{
    uint32_t e, i, rep, p;
    if (spk.end)
        return 0;
    spk.from = spk.to;
    e = pms_bits(4);
    if (e == 0u || e == 15u) {                   /* silence, or the end: the voice it has fades over this frame */
        spk.to.e = 0;
        spk.end = e == 15u;
        return 1;
    }
    rep = pms_bits(1);
    p = pms_bits(5);
    spk.to.e = PM_LPC_ENERGY[e];
    spk.to.p = PM_LPC_PITCH[p];
    if (!rep)
        for (i = 0; i < PM_LPC_ORDER; i++)
            spk.to.k[i] = PM_LPC_K[i][pms_bits(PM_LPC_KBITS[i])];
    if (spk.from.e == 0 || (spk.from.p == 0) != (spk.to.p == 0))   /* from silence, or the voice goes on or off: at once */
        spk.from = spk.to;
    return 1;
}
static void pms_interp(void)                     /* the values of this step: (step + 1) / PM_LPC_STEPS of the way */
{
    int32_t t = (int32_t)spk.step + 1, i;
    spk.now.e = spk.from.e + (spk.to.e - spk.from.e) * t / PM_LPC_STEPS;
    spk.now.p = spk.to.e ? spk.from.p + (spk.to.p - spk.from.p) * t / PM_LPC_STEPS : spk.from.p;   /* (a fade keeps its pitch) */
    for (i = 0; i < PM_LPC_ORDER; i++)
        spk.now.k[i] = spk.to.e ? spk.from.k[i] + (spk.to.k[i] - spk.from.k[i]) * t / PM_LPC_STEPS : spk.from.k[i];
}
/* the next 8 kHz sample; spk.over after the word's last frame */
static int32_t pms_sample(void)
{
    int32_t x, i;
    if (spk.cnt == 0) {
        if (spk.step == 0 && !pms_frame()) {
            spk.over = 1;
            return 0;
        }
        pms_interp();
    }
    if (++spk.cnt >= PM_LPC_STEP) {
        spk.cnt = 0;
        spk.step = (uint8_t)((spk.step + 1u) % PM_LPC_STEPS);
    }
    if (spk.now.p) {                             /* voiced: the chirp once a period */
        x = spk.pc < PM_LPC_CHIRP_N ? PM_LPC_CHIRP[spk.pc] * spk.now.e : 0;
        if (++spk.pc >= (uint32_t)spk.now.p)
            spk.pc = 0;
    } else {                                     /* unvoiced: noise */
        spk.rnd = spk.rnd * 1664525u + 1013904223u;
        x = (spk.rnd >> 31) ? PM_LPC_NOISE * spk.now.e : -PM_LPC_NOISE * spk.now.e;
        spk.pc = 0;
    }
    for (i = PM_LPC_ORDER - 1; i >= 0; i--) {    /* the lattice: b[i] is stage i's backward value a sample ago */
        x -= (int32_t)(((int64_t)spk.now.k[i] * spk.b[i]) >> 15);
        if (x > (1 << 26)) x = 1 << 26;          /* (never, but a bound costs nothing) */
        if (x < -(1 << 26)) x = -(1 << 26);
        spk.b[i + 1] = spk.b[i] + (int32_t)(((int64_t)spk.now.k[i] * x) >> 15);
    }
    spk.b[0] = x;
    x >>= PM_LPC_SHIFT;
    return x > PM_SPEECH_CEIL ? PM_SPEECH_CEIL : x < -PM_SPEECH_CEIL ? -PM_SPEECH_CEIL : x;
}

static void pm_speech_init(void)
{
    uint32_t i;
    for (i = 0; i < sizeof spk; i++)
        ((uint8_t *)&spk)[i] = 0;
    spk.rnd = 0x1234567u;
}
static void pm_speech_say(uint32_t word)
{
    uint32_t i;
    int32_t held = spk.on ? spk.cut + spk.s0 + (int32_t)(((int64_t)(spk.s1 - spk.s0) * (spk.ph >> 17)) >> 15) : 0;
    if (word >= PM_NWORD)
        return;
    for (i = 0; i < sizeof spk; i++)             /* a word begins from silence, with an empty lattice .. */
        ((uint8_t *)&spk)[i] = 0;
    spk.rnd = 0x1234567u;
    spk.word = (uint8_t)word;
    spk.bit = (uint32_t)PM_LPC_WORD[word] * 8u;
    spk.cut = held;                              /* .. over the last of the word it cuts (no step: gone in ~10 ms) */
    spk.on = 1;
}
static uint32_t pm_speech_busy(void) { return spk.on; }
static char pm_speech_letter(uint32_t word) { return word < 26u ? (char)('A' + word) : 0; }

/* n samples of the word into buf (added to what is there); -> 0 when nothing is said */
static int pm_speech_render(int32_t *buf, uint32_t n)
{
    uint32_t i;
    if (!spk.on)
        return 0;
    for (i = 0; i < n; i++) {
        uint32_t ph = spk.ph + PM_LPC_INC;
        if (ph < spk.ph) {                       /* (the phase wrapped: the next 8 kHz sample) */
            if (spk.over && spk.s1 == 0 && spk.cut == 0) {   /* the word is over and the line has come down to 0 */
                spk.on = 0;
                spk.s0 = 0;
                return 1;
            }
            spk.s0 = spk.s1;
            spk.s1 = spk.over ? 0 : pms_sample();
        }
        spk.ph = ph;
        spk.cut = spk.cut * 127 / 128;           /* (towards 0, and it gets there) */
        buf[i] += spk.cut + spk.s0 + (int32_t)(((int64_t)(spk.s1 - spk.s0) * (ph >> 17)) >> 15);
    }
    return 1;
}
#endif
