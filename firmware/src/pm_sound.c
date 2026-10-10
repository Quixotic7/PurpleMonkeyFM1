/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: the synth part's sound (main loop). Part 0 plays FM6 with the preset SOUND (the PRESETS knob)
 * picks from the bank (tools/gen_pm_patches.py: pm_fm6.h, PM_NPRESET voices, four a pet, the pet's own first);
 * nothing of a patch is edited by the player but four bounded knobs (KNOB 1..4 in SYNTH):
 *   TONE    the preset's own recipe (PM_MORPH: per detent, how far each of FM6's macros moves: FB, MLVL the
 *           modulators' level, MRAT their ratio, MEG their envelope rate, VMOD, DTUN the carriers' spread; and LEN)
 *   WOBBLE  0 .. 8: the patch's vibrato deeper and a little faster (LFO pitch depth, speed, sensitivity)
 *   SPACE   the reverb send around the pet's own, and a delay send above the middle
 *   LENGTH  every operator's decay rates (R2, R3) and, half as far, its release rate (R4)
 * WOBBLE and LENGTH (and TONE's LEN) are edits of the patch: the sounding notes follow (fm6_put_patch load 0). A
 * preset change loads its patch under the sounding notes too (PM_PET_LOAD 0).
 * Included after engines.c (eng_fm6.c), params.c and pm_out.c. */
#include "pm_fm6.h"
#define PM_LENGTH_STEP 2         /* DX7 rate points per LEN step towards longer (16 at the end), half that towards shorter */
#define PM_RELEASE_MIN 34        /* the slowest release rate LEN reaches */
#ifndef PM_PET_LOAD
#define PM_PET_LOAD 0            /* a preset change: 0 the patch is edited under the sounding notes, 1 they stop (a DX7 program change) */
#endif
#define PM_SYNTH_LEVEL 110       /* P_LEVEL of the part (104 = 0 dB, 0.5 dB steps): a note against a kick */
enum { PMM_FB, PMM_MLVL, PMM_MRAT, PMM_MEG, PMM_VMOD, PMM_DTUN, PMM_LEN };   /* PM_MORPH's columns */

static struct { uint8_t valid, pet, preset; int8_t tone, wobble, space, length; } pms;

static void pm_sound_patch(uint32_t preset, int32_t length, int32_t wobble, int load)
{
    uint8_t v[FP_SIZE + 1u];
    uint32_t o, i;
    fm6_unpack(PM_FM6[preset % PM_NPRESET], v);
    if (wobble > 0) {                            /* vibrato: depth up to 99, a little faster, pitch-sensitive */
        v[FP_LPMD] = (uint8_t)clamp((int32_t)v[FP_LPMD] + wobble * 9, 0, 99);
        v[FP_LFS] = (uint8_t)clamp((int32_t)v[FP_LFS] + wobble * 2, 0, 99);
        if (v[FP_LPMS] < 3u)
            v[FP_LPMS] = 3;
    }
    for (o = 0; o < 6u; o++)
        for (i = 1; i < 4u; i++) {               /* R2, R3: the decay; R4: the release, half as far and never slower
                                                  * than PM_RELEASE_MIN (a let-go note is gone in a few seconds) */
            uint8_t *r = &v[o * FP_OP + FP_R1 + i];
            int32_t d = length > 0 ? length * PM_LENGTH_STEP : length * PM_LENGTH_STEP / 2;
            if (i == 3u)
                *r = (uint8_t)clamp((int32_t)*r - d / 2, *r < PM_RELEASE_MIN ? *r : PM_RELEASE_MIN, 99);
            else
                *r = (uint8_t)clamp((int32_t)*r - d, 12, 99);
        }
    fm6_put_patch(PM_PART, v, load);
}

/* the engine's pet and knobs as the snapshot has them -> the part (cheap when nothing changed) */
static void pm_sound_apply(uint32_t pet, int32_t preset, int32_t tone, int32_t wobble, int32_t space, int32_t length)
{
    static const uint8_t REV[PM_NPET] = {[PM_MONKEY] = 26, [PM_CAT] = 30, [PM_DOG] = 44, [PM_LLAMA] = 56};
    track_t *t = &trk[PM_PART];
    const int8_t *w;
    int32_t len, morph = tone;
    preset = clamp(preset, 0, (int32_t)PM_NPRESET - 1);
    w = PM_MORPH[preset];
    len = clamp(tone * w[PMM_LEN] + length, -16, 16);
    if (!pms.valid || pms.preset != preset) {
        pm_sound_patch((uint32_t)preset, len, wobble, PM_PET_LOAD);
    } else if (pms.wobble != wobble || pms.length != length || (pms.tone != tone && w[PMM_LEN])) {
        pm_sound_patch((uint32_t)preset, len, wobble, 0);
    }
    if (!pms.valid || pms.pet != pet || pms.space != space) {
        t->p[P_REV] = (int16_t)clamp(REV[pet % PM_NPET] + space * 7, 0, 127);
        t->p[P_DLY] = (int16_t)clamp(space > 0 ? space * 12 : 0, 0, 127);
    }
    t->p[P_E1] = (int16_t)clamp(morph * w[PMM_FB] / 8, -7, 7);
    t->p[P_E2] = (int16_t)clamp(morph * w[PMM_MLVL], -64, 63);
    t->p[P_E3] = (int16_t)clamp(morph * w[PMM_MRAT] / 8, -16, 16);
    t->p[P_E4] = (int16_t)clamp(morph * w[PMM_MEG], -64, 63);
    t->p[P_E5] = (int16_t)clamp(morph * w[PMM_VMOD] / 8, -7, 7);
    t->p[P_E6] = (int16_t)clamp(morph > 0 ? morph * w[PMM_DTUN] : 0, 0, 127);   /* (DTUN only spreads: 0 at HOME and below) */
    pms.valid = 1;
    pms.pet = (uint8_t)pet;
    pms.preset = (uint8_t)preset;
    pms.tone = (int8_t)tone;
    pms.wobble = (int8_t)wobble;
    pms.space = (int8_t)space;
    pms.length = (int8_t)length;
}

/* power-on: the song's globals, part 0 = FM6 POLY, the other parts silent */
static void pm_sound_init(void)
{
    uint32_t i, k;
    fm1_irq_off();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        for (i = 0; i < P_E0; i++)
            t->p[i] = TP[i].def;
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = 0;                  /* FM6's macros: neutral */
        t->eng_req = t->engine = ENGI_FM6;
        t->p[P_VOICE] = V_POLY;
        t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
        t->p[P_MUTE] = k != PM_PART;
    }
    trk[PM_PART].p[P_LEVEL] = PM_SYNTH_LEVEL;
    fm6_init();                                  /* every part the init voice, Dexed's function settings */
    fm6_slot[PM_PART] = 0;                       /* (PTCH stays 0: the patch is the pet's, not a slot's) */
    pms.valid = 0;
    fm1_irq_on();
}
