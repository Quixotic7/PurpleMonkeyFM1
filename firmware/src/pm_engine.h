/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: the musical engine (docs/PURPLEMONKEY.md). Plain C with no hardware and no Felucca names in it:
 * the 27 keys, the mode, the beat and the knobs go in, notes and drum hits come out through pm_out_t. The firmware
 * runs it in the audio ISR (pm_out.c: pm_tick at the top of every block); tests/pm_engine_test.c runs it on a host.
 *
 *   SYNTH  a key plays its note of a pentatonic scale (white keys rising, a black key the note of the white key left
 *          of it) at once; two or more keys held for PM_PHRASE_MS bloom: the lowest of them sounds an octave down
 *          under the rest (an open voicing, kept until the keys go: it does not follow the fingers), and a phrase
 *          walks the held notes an octave above them on the step clock: short eighths with the beat on; without
 *          it long soft quarters that overlap, a texture more than a tune
 *   DRUMS  a key hits its sound of a small kit at once; held past PM_REPEAT_MS it repeats on the step clock with its
 *          lane's own rhythm (kick on the beats, snare on 2 and 4, hats between ..), so several held keys make a
 *          groove, not a roll
 *   BEAT   a backing pattern on the same clock, in either mode; BUSY picks how much of it plays, BOUNCE swings the
 *          clock, SPEED is its tempo
 * Bounds: at most PM_MAX_HELD synth notes sound from the keys, three more from the bloom (an older one is released for a newer), a lane hits
 * once a step whatever asks for it, at most PM_STEP_HITS hits a step; every counter saturates or wraps safely. */
#ifndef PM_ENGINE_H
#define PM_ENGINE_H
#include <stdint.h>

#define PM_NKEY 27u
#define PM_FS 44100u
#define PM_NSTEP 32u             /* the pattern: two bars of sixteenths */
#define PM_MAX_HELD 5u           /* synth notes sounding from held keys (+ the bloom's bass and two phrase notes: 8) */
#define PM_STEP_HITS 5u          /* drum hits a step, the backing pattern's and the repeats' together */
#define PM_PHRASE_MS 500u        /* two or more keys held this long: the phrase */
#define PM_REPEAT_MS 280u        /* a drum key held this long: its repeat */

enum { PM_SYNTH, PM_DRUMS };
enum { PM_MONKEY, PM_CAT, PM_DOG, PM_LLAMA, PM_NPET };
enum { PM_SRC_KEY, PM_SRC_REPEAT, PM_SRC_BEAT };      /* who asked for a drum hit */
/* the kit's sounds (pm_drum_synth.c's lanes, tools/gen_drumkits.py LANES) */
enum { PM_L_KICK, PM_L_SNARE, PM_L_CLAP, PM_L_CHH, PM_L_OHH, PM_L_TOMLO, PM_L_TOMHI, PM_L_CRASH, PM_L_RIDE,
       PM_L_SHAKER, PM_L_CONGA, PM_L_RIM, PM_L_COWBELL, PM_L_CLAVE, PM_L_KICK2, PM_L_SNARE2, PM_NLANE };

/* the knobs: detents within a bounded range, the middle of it the familiar setting (HOME) */
enum { PM_K_SPEED, PM_K_BUSY, PM_K_BOUNCE, PM_K_SQUISH, PM_K_BRIGHT, PM_K_LENGTH, PM_NKNOB };
typedef struct { int8_t min, max, def; } pm_knob_t;
extern const pm_knob_t PM_KNOB[PM_NKNOB];

typedef struct {
    void (*note_on)(void *ud, uint8_t note, uint8_t vel);
    void (*note_off)(void *ud, uint8_t note);
    void (*drum)(void *ud, uint8_t lane, int8_t semi, uint8_t vel, uint8_t src);
} pm_out_t;

typedef struct {
    const pm_out_t *out;
    void *ud;
    /* what the player set */
    uint8_t mode, pet, beat;
    int8_t knob[PM_NKNOB];
    /* the keys */
    uint32_t held;               /* bit k: key k is down and plays in this mode (pressed since the mode began) */
    uint32_t age[PM_NKEY];       /* samples since it went down, saturating */
    uint8_t order[PM_NKEY];      /* the held keys, oldest first */
    uint8_t norder;
    /* the synth's notes */
    uint8_t cnt[128];            /* who sounds a note: keys and the phrase (off at 0) */
    uint32_t voiced;             /* bit k: key k's note is on */
    uint32_t multi_age;          /* samples with two or more keys held, saturating */
    uint8_t ph_note[2];          /* the phrase's sounding notes, 0 = none (two: without the beat they overlap) */
    uint8_t ph_idx, ph_slot;
    uint32_t ph_left[2];         /* samples until each one's note-off */
    uint8_t ph_bass;             /* the bloom: the lowest held note an octave down, while the keys stay; 0 = none */
    /* the clock */
    uint32_t pos;                /* into the step, in samples x BPM (a step: PM_FS * 15 of them, swung) */
    uint8_t step;                /* 0..PM_NSTEP-1: the step sounding */
    uint8_t busy;                /* BUSY as the pattern plays it (taken at each beat) */
    uint8_t lane_rr[PM_NLANE];   /* a lane with several held keys: which plays next */
    /* what happened, for whoever looks (the UI's snapshot, the tests): counters that wrap */
    uint32_t n_note_on, n_note_off, n_drum, n_step;
    uint8_t last_note;
    uint8_t lane_hits[PM_NLANE];
} pm_t;

void pm_init(pm_t *pm, const pm_out_t *out, void *ud);
void pm_key(pm_t *pm, uint32_t key, int down);        /* key 0 (F3) .. 26 (G5) */
void pm_set_mode(pm_t *pm, uint32_t mode);            /* the old mode's notes and repeats end; held keys wait for a new press */
void pm_set_beat(pm_t *pm, int on);                   /* on: the pattern from its first step, now */
void pm_set_pet(pm_t *pm, uint32_t pet);              /* (the sounds are the caller's; mode, beat and clock stay) */
void pm_set_knob(pm_t *pm, uint32_t id, int32_t v);   /* clamped to its range */
void pm_home(pm_t *pm);                               /* every knob to its default; mode, pet, beat and clock stay */
void pm_panic(pm_t *pm);                              /* every note off, every key forgotten */
void pm_tick(pm_t *pm, uint32_t nsamples);            /* the clock: nsamples of audio went by */

uint32_t pm_bpm(const pm_t *pm);
uint32_t pm_key_note(const pm_t *pm, uint32_t key);   /* the synth note of a key */
uint32_t pm_key_lane(uint32_t key, int32_t *semi);    /* the drum sound of a key */
uint32_t pm_notes_on(const pm_t *pm);                 /* notes the engine holds on (0 after every release) */
#endif
