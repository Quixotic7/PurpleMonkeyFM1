/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: the musical engine (docs/PURPLEMONKEY.md). Plain C with no hardware and no Felucca names in it:
 * the 27 keys, the mode, the beat and the knobs go in, notes and drum hits come out through pm_out_t. The firmware
 * runs it in the audio ISR (pm_out.c: pm_tick at the top of every block); tests/pm_engine_test.c runs it on a host.
 *
 *   SYNTH  a key plays its note of a pentatonic scale (white keys rising, a black key the note of the white key left
 *          of it) at once, and the pet sings it back: a bar later, softly, at the step it was played on (the echo:
 *          random tapping becomes a phrase that repeats). Two or more keys held for PM_PHRASE_MS bloom: a phrase
 *          on the step clock over the held notes and the octave above them. With the beat on it is a motif a bar
 *          long (PM_MOTIF: rhythm and shape, ending on the lowest held note) in eighths; without it long soft
 *          quarters up and down that overlap, a texture more than a tune, with the lowest held note an octave
 *          down under the rest (kept until the keys go: it does not follow the fingers).
 *   SONG   with the beat on, a chord a bar goes round (PM_PROG: the pet's own four bars of I, IV, V, vi), and a
 *          bass note plays its root on beats 1 and 3: every key of the scale fits whatever bar it lands in
 *   RUNS   three keys within PM_MASH_MS (a mash, no holding needed): a run of PM_RUN_N sixteenths up or down the
 *          scale from the last key, once a bar at most
 *   STYLE  what the keys are in SYNTH (pm_set_style): KEYS as above; TUNE: any key plays the next note of a
 *          nursery tune (PM_TUNE: mash one key and the whole melody comes out; hold a key and it plays on by
 *          itself at SPEED; the high keys an octave up; the song's bass follows the tune's own chords). The pet
 *          and the world pick three tunes (pm_tune_of); TUNE again steps to the next. TALK: a key says its
 *          letter (out->say: pm_speech.c), the last key "yay"; no notes
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
#define PM_NPRESET 16u           /* the sounds (tools/gen_pm_patches.py): four a pet, the pet's own first */
#define PM_NWORLD 6u             /* the environments (pm_ui.c PM_WORLDS): night, sea, balloons, space, meadow, snow */
#define PM_ECHO_VEL 46u          /* the echo's velocity (a key's is 104 .. 76) */
#define PM_MASH_MS 260u          /* three presses within this: a run */
#define PM_RUN_N 6u              /* a run's notes (sixteenths) */

enum { PM_SYNTH, PM_DRUMS };
enum { PM_MONKEY, PM_CAT, PM_DOG, PM_LLAMA, PM_NPET };
enum { PM_SRC_KEY, PM_SRC_REPEAT, PM_SRC_BEAT };      /* who asked for a drum hit */
/* the kit's sounds (pm_drum_synth.c's lanes, tools/gen_drumkits.py LANES) */
enum { PM_L_KICK, PM_L_SNARE, PM_L_CLAP, PM_L_CHH, PM_L_OHH, PM_L_TOMLO, PM_L_TOMHI, PM_L_CRASH, PM_L_RIDE,
       PM_L_SHAKER, PM_L_CONGA, PM_L_RIM, PM_L_COWBELL, PM_L_CLAVE, PM_L_KICK2, PM_L_SNARE2, PM_NLANE };

/* the knobs: detents within a bounded range, the middle of it the familiar setting (HOME). The four KNOBs are
 * SPEED .. SQUISH in DRUMS and TONE .. LENGTH in SYNTH (the UI's map); SOUND (PRESETS) picks the preset, WORLD
 * (ALGORITHM) the environment, both round and round (wrap): their HOME is the pet's own, not PM_KNOB's def */
enum { PM_K_SPEED, PM_K_BUSY, PM_K_BOUNCE, PM_K_SQUISH, PM_K_SOUND, PM_K_TONE, PM_K_WOBBLE, PM_K_SPACE, PM_K_LENGTH,
       PM_K_WORLD, PM_NKNOB };
typedef struct { int8_t min, max, def, wrap; } pm_knob_t;
extern const pm_knob_t PM_KNOB[PM_NKNOB];
#define pm_preset_home(pet) ((uint32_t)((pet) % PM_NPET) * 4u)
#define pm_world_home(pet) ((uint32_t)(((pet) % PM_NPET) == 0u ? 4u : ((pet) % PM_NPET) == 1u ? 0u : ((pet) % PM_NPET) == 2u ? 2u : 3u))
int32_t pm_knob_def(uint32_t id, uint32_t pet);       /* a knob's HOME setting for this pet */
enum { PM_ST_KEYS, PM_ST_TUNE, PM_ST_TALK, PM_NSTYLE };
/* a nursery tune: notes (MIDI, in C), each its length in sixteenths and the chord root under it (semitones above C) */
typedef struct { uint8_t note, len, root; } pm_tnote_t;
typedef struct { const char *name; uint8_t n; const pm_tnote_t *note; } pm_tune_t;
#define PM_NTUNE 12u
extern const pm_tune_t PM_TUNE[PM_NTUNE];
#define PM_PET_TUNES 3u          /* a pet in a world has three of them (pm_tune_of): TUNE again steps to the next */
uint32_t pm_tune_of(uint32_t pet, uint32_t world, uint32_t i);   /* which tune: pet, world, 0 .. PM_PET_TUNES - 1 */

typedef struct {
    void (*note_on)(void *ud, uint8_t note, uint8_t vel);
    void (*note_off)(void *ud, uint8_t note);
    void (*drum)(void *ud, uint8_t lane, int8_t semi, uint8_t vel, uint8_t src);
    void (*say)(void *ud, uint8_t word);                 /* TALK: a key's letter (0 .. 25 = A .. Z, 26 = yay); may be 0 */
} pm_out_t;

typedef struct {
    const pm_out_t *out;
    void *ud;
    /* what the player set */
    uint8_t mode, pet, beat, style;
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
    uint8_t ph_bass;             /* the bass: the bloom's (the lowest held note an octave down, while the keys stay) or,
                                  * with the beat on, the song's (the bar's chord root on beats 1 and 3); 0 = none */
    uint8_t bass_song;           /* 1: ph_bass is the song's */
    uint8_t bar;                 /* bars since the beat began (the chord: PM_PROG[pet][bar % 4]) */
    /* the echo: what the keys played in the last two bars, by step; sung back a bar later */
    uint8_t ek_note[PM_NSTEP];   /* the note pressed at that step, 0 = none */
    uint32_t ek_at[PM_NSTEP];    /* n_step when it was pressed (an echo only of the bar before) */
    /* the runs: the last presses' times, the run playing */
    uint32_t press_t[3];         /* samples since power-on of the last three presses (wraps) */
    uint8_t press_i, last_key;
    uint8_t run_note, run_left;  /* the run's next note, notes to go (0 = none) */
    int8_t run_dir;
    uint32_t run_bar;            /* n_step the last run began at (one a bar) */
    /* the tune (TUNE style) */
    uint8_t tune_i, tune_pos;    /* which of the pet's tunes, where in it */
    uint8_t song_note;           /* the tune note sounding, 0 = none */
    uint32_t song_left;          /* samples until it ends */
    uint8_t song_root;           /* the chord root under the last tune note (the song's bass follows it) */
    uint32_t t_samples;          /* samples since power-on (wraps) */
    /* the clock */
    uint32_t pos;                /* into the step, in samples x BPM (a step: PM_FS * 15 of them, swung) */
    uint8_t step;                /* 0..PM_NSTEP-1: the step sounding */
    uint8_t busy;                /* BUSY as the pattern plays it (taken at each beat) */
    uint8_t lane_rr[PM_NLANE];   /* a lane with several held keys: which plays next */
    /* what happened, for whoever looks (the UI's snapshot, the tests): counters that wrap */
    uint32_t n_note_on, n_note_off, n_drum, n_step, n_say;
    uint8_t last_note, last_say;
    uint8_t lane_hits[PM_NLANE];
} pm_t;

void pm_init(pm_t *pm, const pm_out_t *out, void *ud);
void pm_key(pm_t *pm, uint32_t key, int down);        /* key 0 (F3) .. 26 (G5) */
void pm_set_mode(pm_t *pm, uint32_t mode);            /* the old mode's notes and repeats end; held keys wait for a new press */
void pm_set_beat(pm_t *pm, int on);                   /* on: the pattern from its first step, now */
void pm_set_pet(pm_t *pm, uint32_t pet);              /* (the sounds are the caller's; mode, beat, clock and WORLD stay; SOUND
                                                       * goes to the pet's own) */
void pm_set_style(pm_t *pm, uint32_t style);          /* PM_ST_*: what the keys do in SYNTH (held keys let go); TUNE
                                                       * while in TUNE: the next of the pet's tunes here */
void pm_set_knob(pm_t *pm, uint32_t id, int32_t v);   /* clamped to its range */
void pm_home(pm_t *pm);                               /* every knob but WORLD to its default; mode, pet, beat and clock stay */
void pm_panic(pm_t *pm);                              /* every note off, every key forgotten */
void pm_tick(pm_t *pm, uint32_t nsamples);            /* the clock: nsamples of audio went by */

uint32_t pm_bpm(const pm_t *pm);
uint32_t pm_key_note(const pm_t *pm, uint32_t key);   /* the synth note of a key */
uint32_t pm_key_lane(uint32_t key, int32_t *semi);    /* the drum sound of a key */
uint32_t pm_notes_on(const pm_t *pm);                 /* notes the engine holds on (0 after every release) */
uint32_t pm_chord_root(const pm_t *pm);               /* the bar's chord root, semitones above C (0 C, 5 F, 7 G, 9 A) */
const pm_tune_t *pm_tune(const pm_t *pm);             /* the tune the pet is on (TUNE style) */
#endif
