/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: the musical engine (pm_engine.h has the rules). */
#include "pm_engine.h"

#define PM_STEP_UNITS (PM_FS * 15u)                    /* a straight sixteenth, in samples x BPM */
#define PM_MS(ms) ((ms) * (PM_FS / 100u) / 10u)        /* ms -> samples */
#define PM_AGE_MAX 0x40000000u
static uint32_t pm_step_len(const pm_t *pm, uint32_t step);

/* SPEED: 72 .. 136 BPM around 104. BUSY: the pattern's level 1 .. 7. BOUNCE: straight .. a 2 : 1 shuffle.
 * SQUISH, SOUND, TONE, WOBBLE, SPACE, LENGTH: the sounds' (pm_out.c, pm_drum_synth.c, pm_sound.c); WORLD the
 * screen's (pm_ui.c); kept here so HOME and the UI have one place. SOUND's and WORLD's def are stand-ins: their
 * HOME is the pet's own (pm_knob_def), and they wrap round */
const pm_knob_t PM_KNOB[PM_NKNOB] = {{-8, 8, 0, 0}, {0, 6, 2, 0}, {0, 8, 0, 0}, {-8, 8, 0, 0}, {0, PM_NPRESET - 1, 4, 1},
                                     {-8, 8, 0, 0}, {0, 8, 0, 0}, {-8, 8, 0, 0}, {-8, 8, 0, 0}, {0, PM_NWORLD - 1, 0, 1}};
int32_t pm_knob_def(uint32_t id, uint32_t pet)
{
    return id == PM_K_SOUND ? (int32_t)pm_preset_home(pet) : id == PM_K_WORLD ? (int32_t)pm_world_home(pet) : id < PM_NKNOB ? PM_KNOB[id].def : 0;
}

/* the nursery tunes (TUNE style), in C: public-domain melodies. C4 = 60; q = 4 sixteenths */
#define Q 4
#define E 2
#define H 8
#define C4 60
#define D4 62
#define E4 64
#define F4 65
#define G4 67
#define A4 69
#define B4 71
#define C5 72
#define G3 55
static const pm_tnote_t T_TWINKLE[] = {
    {C4, Q, 0}, {C4, Q, 0}, {G4, Q, 0}, {G4, Q, 0}, {A4, Q, 5}, {A4, Q, 5}, {G4, H, 0},
    {F4, Q, 5}, {F4, Q, 5}, {E4, Q, 0}, {E4, Q, 0}, {D4, Q, 7}, {D4, Q, 7}, {C4, H, 0},
    {G4, Q, 0}, {G4, Q, 0}, {F4, Q, 5}, {F4, Q, 5}, {E4, Q, 0}, {E4, Q, 0}, {D4, H, 7},
    {G4, Q, 0}, {G4, Q, 0}, {F4, Q, 5}, {F4, Q, 5}, {E4, Q, 0}, {E4, Q, 0}, {D4, H, 7},
    {C4, Q, 0}, {C4, Q, 0}, {G4, Q, 0}, {G4, Q, 0}, {A4, Q, 5}, {A4, Q, 5}, {G4, H, 0},
    {F4, Q, 5}, {F4, Q, 5}, {E4, Q, 0}, {E4, Q, 0}, {D4, Q, 7}, {D4, Q, 7}, {C4, H, 0}};
static const pm_tnote_t T_MARY[] = {
    {E4, Q, 0}, {D4, Q, 0}, {C4, Q, 0}, {D4, Q, 0}, {E4, Q, 0}, {E4, Q, 0}, {E4, H, 0},
    {D4, Q, 7}, {D4, Q, 7}, {D4, H, 7}, {E4, Q, 0}, {G4, Q, 0}, {G4, H, 0},
    {E4, Q, 0}, {D4, Q, 0}, {C4, Q, 0}, {D4, Q, 0}, {E4, Q, 0}, {E4, Q, 0}, {E4, Q, 0}, {E4, Q, 0},
    {D4, Q, 7}, {D4, Q, 7}, {E4, Q, 7}, {D4, Q, 7}, {C4, H, 0}};
static const pm_tnote_t T_ROW[] = {
    {C4, Q + E, 0}, {C4, Q + E, 0}, {C4, Q, 0}, {D4, E, 0}, {E4, Q + E, 0},
    {E4, Q, 0}, {D4, E, 0}, {E4, Q, 0}, {F4, E, 0}, {G4, H, 0},
    {C5, E, 0}, {C5, E, 0}, {C5, E, 0}, {G4, E, 0}, {G4, E, 0}, {G4, E, 0}, {E4, E, 0}, {E4, E, 0}, {E4, E, 0}, {C4, E, 0}, {C4, E, 0}, {C4, E, 0},
    {G4, Q, 7}, {F4, E, 7}, {E4, Q, 0}, {D4, E, 7}, {C4, H, 0}};
static const pm_tnote_t T_FRERE[] = {
    {C4, Q, 0}, {D4, Q, 0}, {E4, Q, 0}, {C4, Q, 0}, {C4, Q, 0}, {D4, Q, 0}, {E4, Q, 0}, {C4, Q, 0},
    {E4, Q, 0}, {F4, Q, 5}, {G4, H, 7}, {E4, Q, 0}, {F4, Q, 5}, {G4, H, 7},
    {G4, E, 0}, {A4, E, 0}, {G4, E, 0}, {F4, E, 5}, {E4, Q, 0}, {C4, Q, 0},
    {G4, E, 0}, {A4, E, 0}, {G4, E, 0}, {F4, E, 5}, {E4, Q, 0}, {C4, Q, 0},
    {C4, Q, 0}, {G3, Q, 7}, {C4, H, 0}, {C4, Q, 0}, {G3, Q, 7}, {C4, H, 0}};
static const pm_tnote_t T_MACDONALD[] = {
    {C4, Q, 0}, {C4, Q, 0}, {C4, Q, 0}, {G3, Q, 7}, {A4, Q, 5}, {A4, Q, 5}, {G4, H, 0},
    {E4, Q, 0}, {E4, Q, 0}, {D4, Q, 7}, {D4, Q, 7}, {C4, H, 0},
    {G3, Q, 7}, {C4, Q, 0}, {C4, Q, 0}, {C4, Q, 0}, {G3, Q, 7}, {A4, Q, 5}, {A4, Q, 5}, {G4, H, 0},
    {E4, Q, 0}, {E4, Q, 0}, {D4, Q, 7}, {D4, Q, 7}, {C4, H, 0}};
#define B3 59
#define A3 57
static const pm_tnote_t T_LONDON[] = {
    {G4, Q + E, 0}, {A4, E, 0}, {G4, Q, 0}, {F4, Q, 5}, {E4, Q, 0}, {F4, Q, 5}, {G4, H, 0},
    {D4, Q, 7}, {E4, Q, 7}, {F4, H, 5}, {E4, Q, 0}, {F4, Q, 5}, {G4, H, 0},
    {G4, Q + E, 0}, {A4, E, 0}, {G4, Q, 0}, {F4, Q, 5}, {E4, Q, 0}, {F4, Q, 5}, {G4, H, 0},
    {D4, H, 7}, {G4, Q, 7}, {E4, Q, 0}, {C4, H, 0}};
static const pm_tnote_t T_HOTCROSS[] = {
    {E4, Q, 0}, {D4, Q, 7}, {C4, H, 0}, {E4, Q, 0}, {D4, Q, 7}, {C4, H, 0},
    {C4, E, 0}, {C4, E, 0}, {C4, E, 0}, {C4, E, 0}, {D4, E, 7}, {D4, E, 7}, {D4, E, 7}, {D4, E, 7},
    {E4, Q, 0}, {D4, Q, 7}, {C4, H, 0}};
static const pm_tnote_t T_ITSY[] = {
    {G3, E, 0}, {C4, Q, 0}, {C4, E, 0}, {C4, Q, 0}, {D4, E, 7}, {E4, Q + E, 0}, {E4, Q + E, 0},
    {E4, Q, 0}, {D4, E, 7}, {C4, Q, 0}, {D4, E, 7}, {E4, Q, 0}, {C4, Q + E + E, 0},
    {E4, Q + E, 0}, {E4, Q, 0}, {F4, E, 5}, {G4, Q + E + E, 0}, {G4, Q, 0}, {F4, E, 5}, {E4, Q, 0}, {F4, E, 5}, {G4, Q + E + E, 0},
    {C4, Q + E, 0}, {C4, Q, 0}, {D4, E, 7}, {E4, Q + E + E, 0}, {E4, Q, 0}, {D4, E, 7}, {C4, Q, 0}, {D4, E, 7}, {E4, Q, 0}, {C4, H, 0}};
static const pm_tnote_t T_MICE[] = {
    {E4, Q, 0}, {D4, Q, 7}, {C4, H, 0}, {E4, Q, 0}, {D4, Q, 7}, {C4, H, 0},
    {G4, Q, 7}, {F4, E, 5}, {F4, E, 5}, {E4, H, 0}, {G4, Q, 7}, {F4, E, 5}, {F4, E, 5}, {E4, H, 0},
    {G4, E, 0}, {C5, E, 0}, {C5, E, 0}, {B4, E, 7}, {A4, E, 5}, {B4, E, 7}, {C5, E, 0}, {G4, E, 0}, {G4, E, 0},
    {E4, Q, 0}, {D4, Q, 7}, {C4, H, 0}};
static const pm_tnote_t T_ODE[] = {
    {E4, Q, 0}, {E4, Q, 0}, {F4, Q, 5}, {G4, Q, 0}, {G4, Q, 0}, {F4, Q, 5}, {E4, Q, 0}, {D4, Q, 7},
    {C4, Q, 0}, {C4, Q, 0}, {D4, Q, 7}, {E4, Q, 0}, {E4, Q + E, 0}, {D4, E, 7}, {D4, H, 7},
    {E4, Q, 0}, {E4, Q, 0}, {F4, Q, 5}, {G4, Q, 0}, {G4, Q, 0}, {F4, Q, 5}, {E4, Q, 0}, {D4, Q, 7},
    {C4, Q, 0}, {C4, Q, 0}, {D4, Q, 7}, {E4, Q, 0}, {D4, Q + E, 7}, {C4, E, 0}, {C4, H, 0}};
static const pm_tnote_t T_JINGLE[] = {
    {E4, Q, 0}, {E4, Q, 0}, {E4, H, 0}, {E4, Q, 0}, {E4, Q, 0}, {E4, H, 0},
    {E4, Q, 0}, {G4, Q, 0}, {C4, Q + E, 0}, {D4, E, 0}, {E4, H + H, 0},
    {F4, Q, 5}, {F4, Q, 5}, {F4, Q + E, 5}, {F4, E, 5}, {F4, Q, 5}, {E4, Q, 0}, {E4, Q, 0}, {E4, E, 0}, {E4, E, 0},
    {E4, Q, 0}, {D4, Q, 7}, {D4, Q, 7}, {E4, Q, 0}, {D4, H, 7}, {G4, H, 7},
    {E4, Q, 0}, {E4, Q, 0}, {E4, H, 0}, {E4, Q, 0}, {E4, Q, 0}, {E4, H, 0},
    {E4, Q, 0}, {G4, Q, 0}, {C4, Q + E, 0}, {D4, E, 0}, {E4, H + H, 0},
    {F4, Q, 5}, {F4, Q, 5}, {F4, Q + E, 5}, {F4, E, 5}, {F4, Q, 5}, {E4, Q, 0}, {E4, Q, 0}, {E4, E, 0}, {E4, E, 0},
    {G4, Q, 7}, {G4, Q, 7}, {F4, Q, 5}, {D4, Q, 7}, {C4, H + H, 0}};
static const pm_tnote_t T_SKIP[] = {
    {E4, Q, 0}, {E4, Q, 0}, {C4, Q, 0}, {C4, Q, 0}, {E4, Q, 0}, {E4, Q, 0}, {G4, H, 0},
    {D4, Q, 7}, {D4, Q, 7}, {B3, Q, 7}, {B3, Q, 7}, {D4, Q, 7}, {D4, Q, 7}, {G4, H, 7},
    {E4, Q, 0}, {E4, Q, 0}, {C4, Q, 0}, {C4, Q, 0}, {E4, Q, 0}, {E4, Q, 0}, {G4, H, 0},
    {D4, Q, 7}, {E4, Q, 7}, {F4, Q, 5}, {E4, Q, 7}, {D4, Q, 7}, {D4, Q, 7}, {C4, H, 0}};
#define TUNE(nm, t) {nm, (uint8_t)(sizeof t / sizeof t[0]), t}
const pm_tune_t PM_TUNE[PM_NTUNE] = {TUNE("TWINKLE", T_TWINKLE), TUNE("MARY", T_MARY), TUNE("ROW BOAT", T_ROW),
                                     TUNE("FRERE", T_FRERE), TUNE("MACDONALD", T_MACDONALD), TUNE("LONDON", T_LONDON),
                                     TUNE("HOT CROSS", T_HOTCROSS), TUNE("ITSY SPIDER", T_ITSY), TUNE("3 MICE", T_MICE),
                                     TUNE("ODE TO JOY", T_ODE), TUNE("JINGLE", T_JINGLE), TUNE("SKIP TO LOU", T_SKIP)};
/* a pet in a world: three tunes, four apart in the library, so neighbouring pets and worlds share few */
uint32_t pm_tune_of(uint32_t pet, uint32_t world, uint32_t i)
{
    return ((pet % PM_NPET) * 3u + (world % PM_NWORLD) * 5u + (i % PM_PET_TUNES) * 4u) % PM_NTUNE;
}
const pm_tune_t *pm_tune(const pm_t *pm) { return &PM_TUNE[pm_tune_of(pm->pet, (uint32_t)pm->knob[PM_K_WORLD], pm->tune_i)]; }

/* the song: a chord a bar, four bars round, the pet's own: the chords' roots as semitones above C (I 0, IV 5, V 7,
 * vi 9: every note of C major pentatonic fits each of them), and the bass note under each (C2 .. A2) */
static const uint8_t PM_PROG[PM_NPET][4] = {[PM_MONKEY] = {0, 5, 0, 7}, [PM_CAT] = {0, 9, 5, 7}, [PM_DOG] = {0, 7, 9, 5}, [PM_LLAMA] = {0, 5, 9, 5}};
uint32_t pm_chord_root(const pm_t *pm)
{
    if (pm->style == PM_ST_TUNE)
        return pm->song_root;                         /* the tune's own chord */
    return PM_PROG[pm->pet % PM_NPET][pm->bar % 4u];
}
/* the bloom's motifs with the beat on: a bar of eighths, each an index into the held notes and the octave above
 * them (0 = the lowest held note an octave up, 1 the next ..; - below it) or a rest; the last sounding one is 0 */
#define PM_REST 99
static const int8_t PM_MOTIF[4][8] = {
    {0, 1, 2, 1, 0, -1, 0, PM_REST},              /* a little arch */
    {0, PM_REST, 1, 0, 2, PM_REST, 0, PM_REST},   /* bouncy */
    {2, 1, 0, PM_REST, 2, 1, 0, PM_REST},         /* falling twice */
    {0, 2, 1, 3, 2, 1, 0, PM_REST},               /* climb and fall */
};

/* ------------------------------------------------------------ the keys --- */
/* a key's white-key number (F3 = 0 .. G5 = 15); a black key: the white key left of it */
static uint32_t pm_white(uint32_t k)
{
    static const int8_t W[12] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6};   /* from F */
    int32_t i = W[k % 12u];
    if (i < 0)
        i = W[(k - 1u) % 12u];
    return (k / 12u) * 7u + (uint32_t)i;
}
/* SYNTH: the white keys climb C major pentatonic from C3 (16 of them: C3 .. C6, three octaves) */
uint32_t pm_key_note(const pm_t *pm, uint32_t key)
{
    static const uint8_t PENT[5] = {0, 2, 4, 7, 9};
    uint32_t d = pm_white(key % PM_NKEY);
    (void)pm;
    return 48u + 12u * (d / 5u) + PENT[d % 5u];
}
/* DRUMS: nine sounds, three times over; the tuned ones a little higher each time */
static const struct { uint8_t lane; int8_t semi; } PM_DRUM_KEY[PM_NKEY] = {
    {PM_L_KICK, 0}, {PM_L_CHH, 0}, {PM_L_SNARE, 0}, {PM_L_TOMLO, 0}, {PM_L_RIM, 0}, {PM_L_TOMHI, 0},
    {PM_L_CONGA, 0}, {PM_L_SHAKER, 0}, {PM_L_COWBELL, 0},
    {PM_L_KICK2, 0}, {PM_L_CHH, 0}, {PM_L_SNARE2, 0}, {PM_L_TOMLO, 4}, {PM_L_CLAVE, 0}, {PM_L_TOMHI, 4},
    {PM_L_CONGA, 5}, {PM_L_OHH, 0}, {PM_L_COWBELL, 5},
    {PM_L_KICK, 0}, {PM_L_CHH, 0}, {PM_L_CLAP, 0}, {PM_L_TOMLO, 7}, {PM_L_RIM, 0}, {PM_L_TOMHI, 7},
    {PM_L_CONGA, 9}, {PM_L_SHAKER, 0}, {PM_L_RIDE, 0},
};
uint32_t pm_key_lane(uint32_t key, int32_t *semi)
{
    key %= PM_NKEY;
    if (semi)
        *semi = PM_DRUM_KEY[key].semi;
    return PM_DRUM_KEY[key].lane;
}
/* a held drum key repeats on these steps of the bar (bit s: sixteenth s) and at this velocity */
static const struct { uint16_t steps; uint8_t vel; } PM_REPEAT[PM_NLANE] = {
    [PM_L_KICK] = {0x1111, 96}, [PM_L_KICK2] = {0x1111, 96},          /* the beats */
    [PM_L_SNARE] = {0x1010, 88}, [PM_L_SNARE2] = {0x1010, 84}, [PM_L_CLAP] = {0x1010, 84},   /* 2 and 4 */
    [PM_L_CHH] = {0x4444, 70}, [PM_L_OHH] = {0x0404, 62},             /* the off-beats */
    [PM_L_TOMLO] = {0x4848, 84}, [PM_L_TOMHI] = {0x8480, 80},
    [PM_L_CONGA] = {0x2488, 78}, [PM_L_SHAKER] = {0xFFFF, 52},
    [PM_L_RIM] = {0x1449, 76}, [PM_L_CLAVE] = {0x1449, 76},           /* son clave */
    [PM_L_COWBELL] = {0x4141, 70}, [PM_L_RIDE] = {0x5151, 62}, [PM_L_CRASH] = {0x0001, 60},
};

/* ------------------------------------------------------- the patterns --- */
/* a pet's backing groove: two bars of sixteenths per sound; a digit is the BUSY level (1 .. 7) from which that
 * hit plays, '.' never. Level 1 is the anchor every level keeps; an open hat comes after the closed one */
typedef struct { uint8_t lane, vel; const char *steps; } pm_row_t;
#define PM_ROWS 6u
static const pm_row_t PM_PATTERN[PM_NPET][PM_ROWS] = {
    [PM_MONKEY] = {
        {PM_L_KICK,    100, "1.....4.1.....4.1.....4.1...6.4."},
        {PM_L_SNARE,    88, "....1.......1.......1.......1..7"},
        {PM_L_CLAVE,    70, "2..2..2...2.2...2..2..2...2.2..."},
        {PM_L_CHH,      60, "..3...3...3...3...3...3...3...3."},
        {PM_L_CONGA,    74, "...5.5...5.5..5....5.5...5.5.6.6"},
        {PM_L_COWBELL,  56, "7.......7.......7.......7...7..."},
    },
    [PM_CAT] = {
        {PM_L_KICK,    100, "1.......1.4.....1.......1.4...6."},
        {PM_L_SNARE,    90, "....1.......1.......1.......1..7"},
        {PM_L_CHH,      62, "3.2.3.2.3.2.3.2.3.2.3.2.3.2.3.2."},
        {PM_L_SHAKER,   44, ".5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5"},
        {PM_L_CONGA,    72, "......6..6........6...6..6.6...."},
        {PM_L_OHH,      58, "..............7...............7."},
    },
    [PM_DOG] = {
        {PM_L_KICK,    100, "1.......1..4....1.......1..4..6."},
        {PM_L_CLAP,     84, "....1.......1.......1.......1..."},
        {PM_L_SHAKER,   50, "2.2.2.2.2.2.2.2.2.2.2.2.2.2.2.2."},
        {PM_L_SHAKER,   40, ".4.4.4.4.4.4.4.4.4.4.4.4.4.4.4.4"},
        {PM_L_CHH,      60, "..3...3...3...3...3...3...3...3."},
        {PM_L_TOMLO,    76, "..........5..5............5.6.7."},
    },
    [PM_LLAMA] = {
        {PM_L_KICK,     92, "1.......1.......1.......1.....5."},
        {PM_L_RIM,      70, "....1.......1.......1.......1..."},
        {PM_L_RIDE,     58, "2...2..32...2..32...2..32...2..3"},
        {PM_L_CHH,      52, "....4.......4.......4.......4..."},
        {PM_L_COWBELL,  50, "......6.......6.......6.......6."},
        {PM_L_TOMHI,    66, ".............7.7.............7.7"},
    },
};

/* ------------------------------------------------------------- output --- */
static void pm_note_on(pm_t *pm, uint32_t note, uint32_t vel)   /* always sounds (a second owner retriggers it) */
{
    note &= 127u;
    if (pm->cnt[note] < 255u)
        pm->cnt[note]++;
    pm->n_note_on++;
    pm->last_note = (uint8_t)note;
    pm->out->note_on(pm->ud, (uint8_t)note, (uint8_t)vel);
}
static void pm_note_off(pm_t *pm, uint32_t note)                /* off when its last owner lets go */
{
    note &= 127u;
    if (!pm->cnt[note] || --pm->cnt[note])
        return;
    pm->n_note_off++;
    pm->out->note_off(pm->ud, (uint8_t)note);
}
static void pm_hit(pm_t *pm, uint32_t lane, int32_t semi, uint32_t vel, uint32_t src)
{
    pm->n_drum++;
    pm->lane_hits[lane % PM_NLANE]++;
    pm->out->drum(pm->ud, (uint8_t)lane, (int8_t)semi, (uint8_t)vel, (uint8_t)src);
}
static uint32_t pm_count(uint32_t m)
{
    uint32_t n = 0;
    for (; m; m &= m - 1u)
        n++;
    return n;
}
static void pm_phrase_off1(pm_t *pm, uint32_t i)
{
    if (pm->ph_note[i])
        pm_note_off(pm, pm->ph_note[i]);
    pm->ph_note[i] = 0;
    pm->ph_left[i] = 0;
}
static void pm_bass_off(pm_t *pm)
{
    if (pm->ph_bass)
        pm_note_off(pm, pm->ph_bass);
    pm->ph_bass = 0;
    pm->bass_song = 0;
}
static void pm_phrase_off(pm_t *pm)               /* the bloom ends: its phrase notes and its bass (not the song's) */
{
    pm_phrase_off1(pm, 0);
    pm_phrase_off1(pm, 1);
    if (!pm->bass_song)
        pm_bass_off(pm);
}
static void pm_echo_forget(pm_t *pm)
{
    uint32_t s;
    for (s = 0; s < PM_NSTEP; s++)
        pm->ek_note[s] = 0;
}
static void pm_song_off(pm_t *pm)
{
    if (pm->song_note)
        pm_note_off(pm, pm->song_note);
    pm->song_note = 0;
    pm->song_left = 0;
}
static void pm_run_off(pm_t *pm) { pm->run_left = 0; }
static void pm_tune_next(pm_t *pm, uint32_t key)   /* the tune's next note (the high keys an octave up); it ends: round again */
{
    const pm_tune_t *t = pm_tune(pm);
    const pm_tnote_t *n = &t->note[pm->tune_pos % t->n];
    pm_song_off(pm);
    pm->song_note = (uint8_t)(n->note + (key >= 14u ? 12u : 0u));
    pm->song_root = n->root;
    pm->song_left = (uint32_t)n->len * PM_STEP_UNITS / pm_bpm(pm);
    pm_note_on(pm, pm->song_note, 96);
    if (++pm->tune_pos >= t->n)
        pm->tune_pos = 0;
}
/* the scale: the pentatonic note a step above or below one (the notes the keys play, C3 .. C6 and round) */
static uint32_t pm_scale_step(uint32_t note, int32_t dir)
{
    static const uint8_t PENT[5] = {0, 2, 4, 7, 9};
    int32_t oct = (int32_t)note / 12, i, best = 0;
    for (i = 0; i < 5; i++)
        if (PENT[i] <= note % 12u)
            best = i;
    best += dir;
    if (best < 0) { best = 4; oct--; }
    if (best > 4) { best = 0; oct++; }
    note = (uint32_t)(oct * 12 + PENT[best]);
    return note < 48u ? note + 12u : note > 96u ? note - 12u : note;
}

/* ------------------------------------------------------------ the keys --- */
static void pm_order_drop(pm_t *pm, uint32_t key)
{
    uint32_t i, k = 0;
    for (i = 0; i < pm->norder; i++)
        if (pm->order[i] != key)
            pm->order[k++] = pm->order[i];
    pm->norder = (uint8_t)k;
}
static void pm_release_keys(pm_t *pm)                 /* every key's note off, every key forgotten */
{
    uint32_t k;
    for (k = 0; k < PM_NKEY; k++)
        if ((pm->voiced >> k) & 1u)
            pm_note_off(pm, pm_key_note(pm, k));
    pm_phrase_off(pm);
    pm_song_off(pm);
    pm_run_off(pm);
    pm->held = pm->voiced = 0;
    pm->norder = 0;
    pm->multi_age = 0;
}

void pm_key(pm_t *pm, uint32_t key, int down)
{
    uint32_t bit;
    if (key >= PM_NKEY)
        return;
    bit = 1u << key;
    if (!down) {
        if (!(pm->held & bit))
            return;                                   /* (a key from before the mode change, or never seen) */
        pm->held &= ~bit;
        pm_order_drop(pm, key);
        if (pm->voiced & bit) {
            pm->voiced &= ~bit;
            pm_note_off(pm, pm_key_note(pm, key));
        }
        return;
    }
    if (pm->held & bit) {                             /* down twice (a lost release): as a new press */
        pm_key(pm, key, 0);
    }
    pm->held |= bit;
    pm->age[key] = 0;
    pm->order[pm->norder++] = (uint8_t)key;
    if (pm->mode == PM_DRUMS) {
        int32_t semi;
        uint32_t lane = pm_key_lane(key, &semi);
        pm_hit(pm, lane, semi, 112, PM_SRC_KEY);      /* now, not on the grid */
        return;
    }
    if (pm->style == PM_ST_TALK) {                    /* a key says its letter; the 27th says yay */
        pm->n_say++;
        pm->last_say = (uint8_t)(key < 26u ? key : 26u);
        if (pm->out->say)
            pm->out->say(pm->ud, pm->last_say);
        return;
    }
    if (pm->style == PM_ST_TUNE) {                    /* any key: the next note of the tune */
        pm_tune_next(pm, key);
        return;
    }
    {   /* a mash: three presses within PM_MASH_MS start a run from this key, up if the keys climbed, else down;
         * one a bar */
        uint32_t i = pm->press_i % 3u, n = 0;
        pm->press_t[i] = pm->t_samples | 1u;
        pm->press_i++;
        for (i = 0; i < 3u; i++)
            if (pm->press_t[i] && pm->t_samples - pm->press_t[i] < PM_MS(PM_MASH_MS))
                n++;
        if (n >= 3u && !pm->run_left && pm->n_step - pm->run_bar >= 16u) {
            pm->run_dir = key >= pm->last_key ? 1 : -1;
            pm->run_note = (uint8_t)pm_key_note(pm, key);
            pm->run_left = PM_RUN_N;
            pm->run_bar = pm->n_step;
        }
        pm->last_key = (uint8_t)key;
    }
    {   /* SYNTH: at most PM_MAX_HELD notes from the keys: the oldest sounding key lets go (it stays held) */
        uint32_t i, n = pm_count(pm->voiced), vel;
        for (i = 0; n >= PM_MAX_HELD && i < pm->norder; i++) {
            uint32_t o = pm->order[i];
            if ((pm->voiced >> o) & 1u) {
                pm->voiced &= ~(1u << o);
                pm_note_off(pm, pm_key_note(pm, o));
                n--;
            }
        }
        vel = 104u - 7u * n;                          /* more notes: each a little softer */
        pm->voiced |= bit;
        pm_note_on(pm, pm_key_note(pm, key), vel);
        {   /* remembered at the step it is nearest to, for the echo a bar later */
            uint32_t s = pm->step;
            if (pm->pos * 2u >= pm_step_len(pm, s))
                s = (s + 1u) % PM_NSTEP;
            pm->ek_note[s] = (uint8_t)pm_key_note(pm, key);
            pm->ek_at[s] = pm->n_step;
        }
    }
}

void pm_set_mode(pm_t *pm, uint32_t mode)
{
    mode = mode == PM_DRUMS ? PM_DRUMS : PM_SYNTH;
    if (mode == pm->mode)
        return;
    pm_release_keys(pm);
    if (mode == PM_DRUMS)
        pm_bass_off(pm);                              /* (the song's bass is SYNTH's) */
    pm->mode = (uint8_t)mode;
}
void pm_set_pet(pm_t *pm, uint32_t pet)
{
    pm->pet = (uint8_t)(pet % PM_NPET);
    pm->knob[PM_K_SOUND] = (int8_t)pm_preset_home(pm->pet);   /* (the world stays: the pet and the world are independent) */
    pm->tune_i = 0;
    pm->tune_pos = 0;
}
void pm_set_style(pm_t *pm, uint32_t style)
{
    style = style < PM_NSTYLE ? style : PM_ST_KEYS;
    if (style == pm->style) {
        if (style == PM_ST_TUNE) {                    /* TUNE again: the next tune here */
            pm_song_off(pm);
            pm->tune_i = (uint8_t)((pm->tune_i + 1u) % PM_PET_TUNES);
            pm->tune_pos = 0;
        }
        return;
    }
    pm_release_keys(pm);
    pm->style = (uint8_t)style;
    pm->tune_pos = 0;
}
void pm_set_knob(pm_t *pm, uint32_t id, int32_t v)
{
    const pm_knob_t *k;
    if (id >= PM_NKNOB)
        return;
    k = &PM_KNOB[id];
    if (k->wrap) {                                    /* round and round */
        int32_t n = k->max - k->min + 1;
        v = ((v - k->min) % n + n) % n + k->min;
    }
    v = v < k->min ? k->min : v > k->max ? k->max : v;
    if (id == PM_K_WORLD && v != pm->knob[id]) {      /* a new world: its tunes from the first */
        pm->tune_i = 0;
        pm->tune_pos = 0;
    }
    pm->knob[id] = (int8_t)v;
}
void pm_home(pm_t *pm)
{
    uint32_t i;
    for (i = 0; i < PM_NKNOB; i++)
        if (i != PM_K_WORLD)                          /* (HOME leaves the world alone) */
            pm->knob[i] = (int8_t)pm_knob_def(i, pm->pet);
}
void pm_panic(pm_t *pm)
{
    uint32_t n;
    pm_release_keys(pm);
    pm_bass_off(pm);
    pm_echo_forget(pm);
    for (n = 0; n < 128u; n++)
        if (pm->cnt[n]) {
            pm->cnt[n] = 1;
            pm_note_off(pm, n);
        }
}
uint32_t pm_bpm(const pm_t *pm) { return (uint32_t)(104 + 4 * pm->knob[PM_K_SPEED]); }
uint32_t pm_notes_on(const pm_t *pm)
{
    uint32_t n, c = 0;
    for (n = 0; n < 128u; n++)
        c += pm->cnt[n] != 0;
    return c;
}

/* ----------------------------------------------------------- the clock --- */
static uint32_t pm_step_len(const pm_t *pm, uint32_t step)   /* BOUNCE: the even steps longer, the odd ones shorter */
{
    uint32_t sw = PM_STEP_UNITS / 24u * (uint32_t)pm->knob[PM_K_BOUNCE];
    return (step & 1u) ? PM_STEP_UNITS - sw : PM_STEP_UNITS + sw;
}

static void pm_step(pm_t *pm)                         /* step pm->step begins */
{
    uint32_t s = pm->step, fired = 0, nh = 0, i, l;
    pm->n_step++;
    if (s % 4u == 0u)
        pm->busy = (uint8_t)pm->knob[PM_K_BUSY];      /* a BUSY change waits for the beat */
    if (pm->beat && s % 16u == 0u && pm->n_step > 1u)
        pm->bar++;                                    /* (the first step of the beat is bar 0) */
    if (pm->beat && pm->mode == PM_SYNTH) {           /* the song's bass: the chord's root on beats 1 and 3, let go
                                                       * two steps before the next (the bloom's bass makes way) */
        if (s % 8u == 0u) {
            if (pm->ph_bass)
                pm_bass_off(pm);
            pm->ph_bass = (uint8_t)(36u + pm_chord_root(pm));
            pm->bass_song = 1;
            pm_note_on(pm, pm->ph_bass, s % 16u == 0u ? 60u : 52u);
        } else if (s % 8u == 6u && pm->bass_song) {
            pm_bass_off(pm);
        }
    } else if (pm->bass_song) {
        pm_bass_off(pm);
    }
    if (pm->mode == PM_SYNTH && pm->run_left) {       /* a run: the next note up or down the scale, a sixteenth each */
        uint32_t slot = pm->ph_slot ^= 1u;
        pm->run_note = (uint8_t)pm_scale_step(pm->run_note, pm->run_dir);
        pm_phrase_off1(pm, slot);
        pm->ph_note[slot] = pm->run_note;
        pm->ph_left[slot] = PM_STEP_UNITS / pm_bpm(pm) * 9u / 10u;
        pm_note_on(pm, pm->run_note, 72u + 4u * pm->run_left);
        pm->run_left--;
    }
    if (pm->mode == PM_SYNTH && pm->style == PM_ST_KEYS && pm->norder < 2u && !pm->run_left) {   /* the echo: what a key played a bar ago, sung back softly */
        uint32_t q = (s + 16u) % PM_NSTEP, age = pm->n_step - pm->ek_at[q];
        if (pm->ek_note[q] && age >= 15u && age <= 17u) {
            uint32_t slot = pm->ph_slot ^= 1u;
            pm_phrase_off1(pm, slot);
            pm->ph_note[slot] = pm->ek_note[q];
            pm->ph_left[slot] = 2u * PM_STEP_UNITS / pm_bpm(pm) * 4u / 5u;
            pm_note_on(pm, pm->ph_note[slot], PM_ECHO_VEL);
            pm->ek_note[q] = 0;
        }
    }
    if (pm->beat) {
        const pm_row_t *row = PM_PATTERN[pm->pet % PM_NPET];
        for (i = 0; i < PM_ROWS; i++) {
            char c = row[i].steps[s];
            uint32_t vel = row[i].vel + (s % 4u == 0u ? 8u : 0u);
            if (c < '1' || c > '9' || (uint32_t)(c - '0') > pm->busy + 1u)
                continue;
            if (((fired >> row[i].lane) & 1u) || nh >= PM_STEP_HITS)
                continue;
            fired |= 1u << row[i].lane;
            nh++;
            pm_hit(pm, row[i].lane, 0, vel, PM_SRC_BEAT);
        }
    }
    if (pm->mode == PM_DRUMS) {
        for (l = 0; l < PM_NLANE; l++) {
            int8_t cand[4];
            uint32_t nc = 0;
            if (!((PM_REPEAT[l].steps >> (s % 16u)) & 1u))
                continue;
            for (i = 0; i < pm->norder && nc < 4u; i++) {
                uint32_t k = pm->order[i];
                if (PM_DRUM_KEY[k].lane == l && pm->age[k] >= PM_MS(PM_REPEAT_MS))
                    cand[nc++] = PM_DRUM_KEY[k].semi;
            }
            if (!nc || ((fired >> l) & 1u) || nh >= PM_STEP_HITS)
                continue;
            fired |= 1u << l;
            nh++;
            pm_hit(pm, l, cand[pm->lane_rr[l]++ % nc], PM_REPEAT[l].vel + (s % 4u == 0u ? 8u : 0u), PM_SRC_REPEAT);
        }
        return;
    }
    {   /* SYNTH: the phrase, up and down the held notes an octave above them */
        uint32_t div = pm->beat ? 2u : 4u, m = 0, period;
        uint8_t arr[PM_NKEY];
        if (pm->style != PM_ST_KEYS || pm->norder < 2u || pm->multi_age < PM_MS(PM_PHRASE_MS) || s % div)
            return;
        for (i = 0; i < pm->norder; i++) {            /* the held notes, each once, rising */
            uint32_t note = pm_key_note(pm, pm->order[i]), j, at = m;
            for (j = 0; j < m; j++) {
                if (arr[j] == note)
                    break;
                if (arr[j] > note && at == m)
                    at = j;
            }
            if (j < m)
                continue;
            for (j = m; j > at; j--)
                arr[j] = arr[j - 1u];
            arr[at] = (uint8_t)note;
            m++;
        }
        if (m < 2u)
            return;
        if (!pm->ph_bass && !pm->beat) {          /* the bloom opens: the lowest note an octave down, once */
            pm->ph_bass = (uint8_t)(arr[0] - 12u);
            pm_note_on(pm, pm->ph_bass, 56);
        }
        if (pm->beat) {                           /* a motif a bar long over the held notes and the octave above */
            const int8_t *mo = PM_MOTIF[(pm->bar + pm->pet) % 4u];
            int32_t ix = mo[(s % 16u) / 2u], note;
            uint32_t len = 2u * PM_STEP_UNITS / pm_bpm(pm);
            if (ix == PM_REST)
                return;
            ix += (int32_t)m;                     /* 0 = the lowest held note an octave up */
            ix = ix < 0 ? 0 : ix >= 2 * (int32_t)m ? 2 * (int32_t)m - 1 : ix;
            note = arr[ix % (int32_t)m] + (ix >= (int32_t)m ? 12 : 0);
            pm_phrase_off1(pm, 1);
            pm_phrase_off1(pm, 0);
            pm->ph_note[0] = (uint8_t)(note > 96 ? note - 12 : note);
            pm->ph_left[0] = len * 4u / 5u;
            pm_note_on(pm, pm->ph_note[0], 62u + (s % 8u == 0u ? 8u : 0u));
            return;
        }
        period = 2u * m - 2u;
        pm->ph_idx = (uint8_t)((pm->ph_idx + 1u) % period);
        i = pm->ph_idx < m ? pm->ph_idx : period - pm->ph_idx;
        {   /* without the beat: long notes up and down, each over the next */
            uint32_t slot = pm->ph_slot ^= 1u, len = div * PM_STEP_UNITS / pm_bpm(pm);
            pm_phrase_off1(pm, slot);
            pm->ph_note[slot] = (uint8_t)(arr[i] + 12u > 96u ? arr[i] : arr[i] + 12u);
            pm->ph_left[slot] = len * 9u / 5u;
            pm_note_on(pm, pm->ph_note[slot], 50u + (s % 8u == 0u ? 8u : 0u));
        }
    }
}

void pm_set_beat(pm_t *pm, int on)
{
    on = on != 0;
    if (on == pm->beat)
        return;
    pm->beat = (uint8_t)on;
    if (on) {                                         /* the groove from its first step, at once */
        pm->pos = 0;
        pm->step = 0;
        pm->bar = 0;
        pm_step(pm);
    } else if (pm->bass_song) {
        pm_bass_off(pm);
    }
}

void pm_tick(pm_t *pm, uint32_t n)
{
    uint32_t k, len;
    pm->t_samples += n;
    if (pm->song_note) {
        if (pm->song_left <= n) {
            pm_song_off(pm);
            if (pm->style == PM_ST_TUNE && pm->mode == PM_SYNTH && pm->norder)   /* a key held: the tune plays on, at SPEED */
                pm_tune_next(pm, pm->order[pm->norder - 1u]);
        } else
            pm->song_left -= n;
    }
    for (k = 0; k < PM_NKEY; k++)
        if (((pm->held >> k) & 1u) && pm->age[k] < PM_AGE_MAX)
            pm->age[k] += n;
    if (pm->mode == PM_SYNTH && pm->norder >= 2u) {
        if (pm->multi_age < PM_AGE_MAX)
            pm->multi_age += n;
    } else {
        pm->multi_age = 0;
    }
    for (k = 0; k < 2u; k++)
        if (pm->ph_note[k]) {
            if (pm->ph_left[k] <= n)
                pm_phrase_off1(pm, k);
            else
                pm->ph_left[k] -= n;
        }
    if (pm->ph_bass && !pm->bass_song && pm->norder < 2u)   /* the keys went: the bloom's bass with them */
        pm_bass_off(pm);
    pm->pos += n * pm_bpm(pm);
    while (pm->pos >= (len = pm_step_len(pm, pm->step))) {
        pm->pos -= len;
        pm->step = (uint8_t)((pm->step + 1u) % PM_NSTEP);
        pm_step(pm);
    }
}

void pm_init(pm_t *pm, const pm_out_t *out, void *ud)
{
    uint32_t i;
    uint8_t *p = (uint8_t *)pm;
    for (i = 0; i < sizeof *pm; i++)
        p[i] = 0;
    pm->out = out;
    pm->ud = ud;
    pm->mode = PM_SYNTH;
    pm->pet = PM_CAT;
    pm->style = PM_ST_KEYS;
    pm_home(pm);
    pm->t_samples = 1;
    pm->busy = (uint8_t)pm->knob[PM_K_BUSY];
    pm->step = PM_NSTEP - 1u;                         /* the first tick's step is 0 */
    pm->pos = PM_STEP_UNITS;
}
