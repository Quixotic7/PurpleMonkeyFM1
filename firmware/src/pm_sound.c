/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: the synth part's sound (main loop). Part 0 plays FM6 with the pet's own patch
 * (tools/gen_pm_patches.py: pm_fm6.h); nothing of the patch is edited by the player but two bounded macros:
 *   BRIGHT (PRESETS)   FM6's MLVL (P_E2: the level of every operator that is not a carrier), PM_BRIGHT_STEP a detent
 *   LENGTH (ALGORITHM) every operator's decay rates (R2, R3) and, half as far, its release rate (R4): the same
 *                      sound shorter or longer, as an edit (the sounding notes follow, fm6_put_patch load 0)
 * A pet change loads its patch as a DX7 program change does (the sounding notes stop: eng_fm6.c fm6_lgen).
 * Included after engines.c (eng_fm6.c), params.c and pm_out.c. */
#include "pm_fm6.h"
#define PM_BRIGHT_STEP 3         /* MLVL per detent: +-24 of its +-64 at the ends */
#define PM_LENGTH_STEP 2         /* DX7 rate points per detent towards longer (16 at the end), half that towards shorter */
#define PM_RELEASE_MIN 34        /* the slowest release rate LENGTH reaches */
#ifndef PM_PET_LOAD
#define PM_PET_LOAD 0            /* a pet change: 0 the patch is edited under the sounding notes, 1 they stop (a DX7 program change) */
#endif
#define PM_SYNTH_LEVEL 110       /* P_LEVEL of the part (104 = 0 dB, 0.5 dB steps): a note against a kick */

static struct { uint8_t valid, pet; int8_t bright, length; } pms;

static void pm_sound_patch(uint32_t pet, int32_t length, int load)
{
    uint8_t v[FP_SIZE + 1u];
    uint32_t o, i;
    fm6_unpack(PM_FM6[pet % PM_NPET], v);
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
static void pm_sound_apply(uint32_t pet, int32_t bright, int32_t length)
{
    track_t *t = &trk[PM_PART];
    if (!pms.valid || pms.pet != pet) {
        static const uint8_t REV[PM_NPET] = {[PM_MONKEY] = 26, [PM_CAT] = 30, [PM_DOG] = 44, [PM_LLAMA] = 56};
        pm_sound_patch(pet, length, PM_PET_LOAD);
        t->p[P_REV] = REV[pet % PM_NPET];
    } else if (pms.length != length) {
        pm_sound_patch(pet, length, 0);
    }
    t->p[P_E2] = (int16_t)(bright * PM_BRIGHT_STEP);
    pms.valid = 1;
    pms.pet = (uint8_t)pet;
    pms.bright = (int8_t)bright;
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
