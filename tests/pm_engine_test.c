/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey's engine on a host (firmware/src/pm_engine.c alone: no hardware, no Felucca): the keys, the clock,
 * the pattern, the repeats, the phrase, the transitions and the bounds. tests/run_pm_tests.sh builds and runs it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/pm_engine.c"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* what the engine sent */
static int sounding[128];            /* 1: a note-on without its note-off yet */
static uint32_t n_on, n_off, n_hit, hit_src[3], lane_n[PM_NLANE];
static uint32_t off_unsounded;       /* note-offs for a note that was not on */
static uint8_t last_lane, last_vel;
static int8_t last_semi;
static uint32_t step_hits, step_hits_max, step_lane_mask, lane_twice;   /* per step: hits not from a key */

static void t_on(void *ud, uint8_t note, uint8_t vel)
{
    (void)ud;
    n_on++;
    sounding[note] = 1;
    last_vel = vel;
}
static void t_off(void *ud, uint8_t note)
{
    (void)ud;
    n_off++;
    if (!sounding[note])
        off_unsounded++;
    sounding[note] = 0;
}
static void t_drum(void *ud, uint8_t lane, int8_t semi, uint8_t vel, uint8_t src)
{
    (void)ud;
    n_hit++;
    hit_src[src % 3u]++;
    lane_n[lane % PM_NLANE]++;
    last_lane = lane;
    last_semi = semi;
    last_vel = vel;
    if (src != PM_SRC_KEY) {
        if ((step_lane_mask >> lane) & 1u)
            lane_twice++;
        step_lane_mask |= 1u << lane;
        if (++step_hits > step_hits_max)
            step_hits_max = step_hits;
    }
}
static const pm_out_t OUT = {t_on, t_off, t_drum};
static pm_t pm;

static void reset(void)
{
    memset(sounding, 0, sizeof sounding);
    n_on = n_off = n_hit = off_unsounded = step_hits = step_hits_max = step_lane_mask = lane_twice = 0;
    memset(hit_src, 0, sizeof hit_src);
    memset(lane_n, 0, sizeof lane_n);
    pm_init(&pm, &OUT, 0);
}
static uint32_t nsounding(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < 128u; i++)
        n += (uint32_t)sounding[i];
    return n;
}
/* ms of audio in 32-sample blocks, as the firmware ticks; the per-step counters follow the engine's steps */
static void run_ms(uint32_t ms)
{
    uint32_t n = ms * PM_FS / 1000u / 32u, i;
    for (i = 0; i < n; i++) {
        uint32_t s0 = pm.n_step;
        step_hits = 0;                           /* (a block holds at most one step at these tempos) */
        step_lane_mask = 0;
        pm_tick(&pm, 32);
        CHECK(pm.n_step - s0 <= 1u, "two steps in a block");
    }
}

static void test_synth_keys(void)
{
    uint32_t k, prev = 0;
    printf("synth: every key a note, at once\n");
    reset();
    for (k = 0; k < PM_NKEY; k++) {
        uint32_t note = pm_key_note(&pm, k), pc = note % 12u, a = n_on;
        CHECK(note >= 48u && note <= 84u, "key %u: note %u outside C3..C6", k, note);
        CHECK(pc == 0 || pc == 2 || pc == 4 || pc == 7 || pc == 9, "key %u: note %u not pentatonic", k, note);
        CHECK(note >= prev, "key %u: note %u below the key before", k, note);
        prev = note;
        pm_key(&pm, k, 1);
        CHECK(n_on == a + 1u && sounding[note], "key %u: no note-on on the press", k);
        pm_key(&pm, k, 0);
        CHECK(!sounding[note] && pm_notes_on(&pm) == 0, "key %u: its note still on after the release", k);
    }
    CHECK(pm_key_note(&pm, 1) == pm_key_note(&pm, 0), "F#3 is not F3's note");
    /* two keys of one note: the second press sounds again, the note ends with the last release */
    pm_key(&pm, 0, 1);
    pm_key(&pm, 1, 1);
    CHECK(n_on == PM_NKEY + 2u, "the second key of a note did not sound");
    pm_key(&pm, 0, 0);
    CHECK(sounding[pm_key_note(&pm, 0)], "the note ended with one of its two keys still down");
    pm_key(&pm, 1, 0);
    CHECK(nsounding() == 0, "the note is on with both keys up");
}

static void test_synth_bounds(void)
{
    uint32_t k, vmin = 127;
    printf("synth: many keys held stay bounded\n");
    reset();
    for (k = 0; k < PM_NKEY; k++) {
        pm_key(&pm, k, 1);
        if (last_vel < vmin)
            vmin = last_vel;
        CHECK(nsounding() <= PM_MAX_HELD, "%u notes on with %u keys down", nsounding(), k + 1u);
    }
    CHECK(pm.held == (1u << PM_NKEY) - 1u, "a held key is not known as held (its LED)");
    CHECK(vmin >= 60u && vmin < 104u, "more notes are not softer (lowest velocity %u)", vmin);
    run_ms(3000);
    CHECK(nsounding() <= PM_MAX_HELD + 3u, "%u notes on with every key held and the bloom running", nsounding());
    for (k = 0; k < PM_NKEY; k++)
        pm_key(&pm, k, 0);
    run_ms(1000);
    CHECK(nsounding() == 0 && pm_notes_on(&pm) == 0, "%u notes stuck after every release", nsounding());
    CHECK(off_unsounded == 0, "%u note-offs for notes that were not on", off_unsounded);
}

static void test_phrase(void)
{
    uint32_t a;
    printf("synth: held keys grow a phrase\n");
    reset();
    pm_key(&pm, 7, 1);                           /* one key: no phrase however long */
    run_ms(3000);
    CHECK(n_on == 1u, "a single held key played %u notes", n_on);
    pm_key(&pm, 11, 1);
    pm_key(&pm, 14, 1);
    a = n_on;
    run_ms(PM_PHRASE_MS - 60u);
    CHECK(n_on == a, "the phrase began before %u ms", PM_PHRASE_MS);
    run_ms(4000);
    /* beat off: the bass once, then a note a quarter (577 ms at 104 BPM): about 8 in 4 s; long notes: two overlap */
    CHECK(n_on - a >= 6u && n_on - a <= 9u, "the bloom played %u notes in 4 s without the beat", n_on - a);
    CHECK(sounding[pm_key_note(&pm, 7) - 12u], "no bass an octave under the lowest held note");
    CHECK(nsounding() >= 5u && nsounding() <= 6u, "%u notes on: three keys, the bass, one or two phrase notes", nsounding());
    pm_set_beat(&pm, 1);
    a = n_on;
    run_ms(4000);                                /* beat on: eighths, about 14 */
    CHECK(n_on - a >= 12u && n_on - a <= 15u, "the phrase played %u notes in 4 s with the beat", n_on - a);
    pm_key(&pm, 7, 0);
    pm_key(&pm, 11, 0);
    a = n_on;
    run_ms(2000);
    CHECK(n_on == a, "the phrase went on with one key left");
    CHECK(nsounding() == 1u, "%u notes on with one key left (the bass and the phrase should be gone)", nsounding());
    pm_key(&pm, 14, 0);
    run_ms(700);
    CHECK(nsounding() == 0, "a note left after the phrase");
}

static void test_drum_keys(void)
{
    uint32_t k;
    printf("drums: every key a hit, at once; a held key repeats on its own rhythm\n");
    reset();
    pm_set_mode(&pm, PM_DRUMS);
    for (k = 0; k < PM_NKEY; k++) {
        uint32_t a = n_hit;
        int32_t semi;
        uint32_t lane = pm_key_lane(k, &semi);
        pm_key(&pm, k, 1);
        CHECK(n_hit == a + 1u && last_lane == lane && last_semi == semi, "key %u: no hit of its sound on the press", k);
        CHECK(lane != PM_L_CRASH, "key %u plays the crash", k);
        pm_key(&pm, k, 0);
    }
    CHECK(n_on == 0, "a drum key played a synth note");
    /* the kick held for 8 bars from a bar line: it repeats on the beats only */
    reset();
    pm_set_mode(&pm, PM_DRUMS);
    pm_key(&pm, 0, 1);
    run_ms(PM_REPEAT_MS + 10u);
    {
        uint32_t a = hit_src[PM_SRC_REPEAT], s0 = pm.n_step;
        run_ms(8u * 2308u);                      /* 8 bars at 104 BPM */
        CHECK(hit_src[PM_SRC_REPEAT] - a >= 31u && hit_src[PM_SRC_REPEAT] - a <= 33u,
              "the held kick hit %u times in 8 bars (%u steps)", hit_src[PM_SRC_REPEAT] - a, pm.n_step - s0);
    }
    pm_key(&pm, 0, 0);
    {
        uint32_t a = n_hit;
        run_ms(3000);
        CHECK(n_hit == a, "a repeat went on after the release");
    }
    /* many keys held: a groove, bounded */
    for (k = 0; k < PM_NKEY; k++)
        pm_key(&pm, k, 1);
    step_hits_max = 0;
    run_ms(6000);
    CHECK(step_hits_max >= 2u && step_hits_max <= PM_STEP_HITS, "every key held: up to %u hits a step", step_hits_max);
    CHECK(lane_twice == 0, "a sound hit twice in a step (%u times)", lane_twice);
    for (k = 0; k < PM_NKEY; k++)
        pm_key(&pm, k, 0);
}

static void test_beat(void)
{
    uint32_t pet, b, a;
    printf("beat: starts at once, keeps its phase, BUSY adds to the anchors\n");
    reset();
    pm_set_beat(&pm, 1);
    CHECK(n_hit >= 1u && lane_n[PM_L_KICK] == 1u, "BEAT did not start with its first kick");
    CHECK(pm.step == 0, "BEAT did not start at the first step");
    /* the clock runs through mode and pet changes: one step after another, none skipped or repeated */
    {
        uint32_t i, last = pm.step, steps = pm.n_step;
        for (i = 0; i < 40000u; i++) {
            if (i % 997u == 0u)
                pm_set_mode(&pm, i / 997u % 2u ? PM_DRUMS : PM_SYNTH);
            if (i % 1291u == 0u)
                pm_set_pet(&pm, i / 1291u);
            pm_tick(&pm, 32);
            if (pm.n_step != steps) {
                CHECK(pm.step == (last + 1u) % PM_NSTEP, "step %u after step %u", (unsigned)pm.step, last);
                last = pm.step;
                steps = pm.n_step;
            }
        }
        /* 40000 blocks = 29.02 s at 104 BPM: 201.2 sixteenths */
        CHECK(steps >= 200u && steps <= 203u, "%u steps in 29 s at 104 BPM", steps);
    }
    pm_set_beat(&pm, 0);
    a = hit_src[PM_SRC_BEAT];
    run_ms(3000);
    CHECK(hit_src[PM_SRC_BEAT] == a, "the pattern played on after BEAT stopped");
    pm_key(&pm, 3, 1);
    pm_key(&pm, 3, 0);
    CHECK(n_on + n_hit > 0, "the keys do not play with the beat stopped");
    /* per pet: the rows are two bars long; each BUSY level plays what the one below it does, and more */
    for (pet = 0; pet < PM_NPET; pet++) {
        uint32_t prev = 0, r;
        for (r = 0; r < PM_ROWS; r++)
            CHECK(strlen(PM_PATTERN[pet][r].steps) == PM_NSTEP, "pet %u row %u is not %u steps", pet, r, PM_NSTEP);
        for (b = 0; b <= (uint32_t)PM_KNOB[PM_K_BUSY].max; b++) {
            uint32_t n = 0, s;
            for (s = 0; s < PM_NSTEP; s++)
                for (r = 0; r < PM_ROWS; r++) {
                    char c = PM_PATTERN[pet][r].steps[s];
                    n += c >= '1' && c <= '9' && (uint32_t)(c - '0') <= b + 1u;
                }
            CHECK(n > prev, "pet %u: BUSY %u plays %u hits, the level below %u", pet, b, n, prev);
            if (b == 0)
                CHECK(n >= 6u, "pet %u: the quietest groove has %u hits", pet, n);
            prev = n;
        }
        {   /* the anchors: a kick on each bar's first beat at every level */
            CHECK(PM_PATTERN[pet][0].lane == PM_L_KICK && PM_PATTERN[pet][0].steps[0] == '1' &&
                  PM_PATTERN[pet][0].steps[16] == '1', "pet %u: no kick on the bar at level 1", pet);
        }
    }
    /* BUSY waits for the beat */
    reset();
    pm_set_beat(&pm, 1);
    run_ms(160);                                 /* inside the first beat (a sixteenth: 144 ms) */
    pm_set_knob(&pm, PM_K_BUSY, 6);
    CHECK(pm.busy == PM_KNOB[PM_K_BUSY].def, "BUSY changed the pattern inside a beat");
    run_ms(600);
    CHECK(pm.busy == 6, "BUSY was not taken at the beat");
}

static void test_knobs(void)
{
    uint32_t i;
    printf("knobs: bounded, usable at the ends; HOME\n");
    reset();
    for (i = 0; i < PM_NKNOB; i++) {
        pm_set_knob(&pm, i, 1000);
        CHECK(pm.knob[i] == PM_KNOB[i].max, "knob %u: %d above its range", i, pm.knob[i]);
        pm_set_knob(&pm, i, -1000);
        CHECK(pm.knob[i] == PM_KNOB[i].min, "knob %u: %d below its range", i, pm.knob[i]);
        CHECK(PM_KNOB[i].def >= PM_KNOB[i].min && PM_KNOB[i].def <= PM_KNOB[i].max, "knob %u: default outside", i);
    }
    CHECK(pm_bpm(&pm) == 72u, "the slowest tempo is %u", pm_bpm(&pm));
    pm_set_knob(&pm, PM_K_SPEED, 100);
    CHECK(pm_bpm(&pm) == 136u, "the fastest tempo is %u", pm_bpm(&pm));
    pm_set_knob(&pm, PM_K_BOUNCE, 100);          /* full BOUNCE: a pair of steps is still two sixteenths, 2 : 1 */
    CHECK(pm_step_len(&pm, 0) + pm_step_len(&pm, 1) == 2u * PM_STEP_UNITS, "BOUNCE changes the beat's length");
    CHECK(pm_step_len(&pm, 1) * 2u >= pm_step_len(&pm, 0) - 8u, "BOUNCE is past a 2 : 1 shuffle");
    {   /* every knob at an end, the beat on, every key down: still bounded, still a clock */
        uint32_t k, s0;
        pm_set_mode(&pm, PM_DRUMS);
        pm_set_beat(&pm, 1);
        for (k = 0; k < PM_NKEY; k++)
            pm_key(&pm, k, 1);
        s0 = pm.n_step;
        step_hits_max = 0;
        run_ms(4000);
        CHECK(pm.n_step - s0 >= 35u && pm.n_step - s0 <= 37u, "%u steps in 4 s at 136 BPM", pm.n_step - s0);
        CHECK(step_hits_max <= PM_STEP_HITS, "%u hits in a step at the knobs' ends", step_hits_max);
        for (k = 0; k < PM_NKEY; k++)
            pm_key(&pm, k, 0);
    }
    pm_home(&pm);
    for (i = 0; i < PM_NKNOB; i++)
        CHECK(pm.knob[i] == PM_KNOB[i].def, "HOME left knob %u at %d", i, pm.knob[i]);
    CHECK(pm.beat == 1 && pm.mode == PM_DRUMS, "HOME changed the beat or the mode");
}

static void test_transitions(void)
{
    uint32_t a, step;
    printf("transitions: mode, pet, stop / start leave nothing sounding or repeating\n");
    reset();
    pm_key(&pm, 4, 1);
    pm_key(&pm, 9, 1);
    pm_key(&pm, 12, 1);
    run_ms(1500);                                /* the phrase runs */
    pm_set_mode(&pm, PM_DRUMS);
    CHECK(nsounding() == 0, "%u synth notes on after the change to DRUMS", nsounding());
    a = n_hit;
    run_ms(3000);                                /* the keys are still down: they are the old mode's */
    CHECK(n_hit == a && n_on + 0u == n_off, "keys held over the mode change play in the new mode");
    CHECK(pm.held == 0, "keys held over the mode change count as held");
    pm_key(&pm, 4, 0);                           /* letting them go does nothing */
    pm_key(&pm, 9, 0);
    pm_key(&pm, 12, 0);
    CHECK(n_hit == a && off_unsounded == 0, "a stale key's release did something");
    pm_key(&pm, 4, 1);                           /* pressed again: the new mode */
    CHECK(n_hit == a + 1u, "a key pressed again after the change does not play");
    run_ms(1000);                                /* its repeat runs */
    pm_set_mode(&pm, PM_SYNTH);
    a = n_hit;
    run_ms(2000);
    CHECK(n_hit == a, "a drum repeat went on in SYNTH");
    pm_key(&pm, 4, 0);
    /* a pet change touches neither the mode nor the beat nor the clock */
    pm_set_beat(&pm, 1);
    run_ms(500);
    step = pm.n_step;
    pm_set_pet(&pm, PM_LLAMA);
    CHECK(pm.mode == PM_SYNTH && pm.beat == 1 && pm.n_step == step, "a pet change moved the mode, the beat or the clock");
    /* stop and start, quickly and often */
    for (a = 0; a < 50u; a++) {
        pm_set_beat(&pm, 0);
        run_ms(7);
        pm_set_beat(&pm, 1);
        CHECK(pm.step == 0, "a restart did not begin the groove");
        run_ms(13);
    }
    pm_set_beat(&pm, 0);
    pm_panic(&pm);
    CHECK(nsounding() == 0 && pm_notes_on(&pm) == 0, "notes on after a panic");
}

/* a small child at the panel: random keys, buttons and knobs for a long time, then hands off */
static void test_mash(void)
{
    uint32_t i, seed = 0x504D4631u, worst = 0;
    printf("random playing: 400000 actions, then hands off\n");
    reset();
    for (i = 0; i < 400000u; i++) {
        uint32_t r, op;
        seed = seed * 1664525u + 1013904223u;
        r = seed >> 8;
        op = r % 100u;
        if (op < 60u)
            pm_key(&pm, (r >> 8) % PM_NKEY, (r >> 16) & 1u);
        else if (op < 64u)
            pm_set_mode(&pm, (r >> 8) & 1u);
        else if (op < 67u) {
            step_hits = 0;                       /* (BEAT on is a step of its own: the groove's first, now) */
            step_lane_mask = 0;
            pm_set_beat(&pm, (r >> 8) & 1u);
        }
        else if (op < 70u)
            pm_set_pet(&pm, (r >> 8) % PM_NPET);
        else if (op < 78u)
            pm_set_knob(&pm, (r >> 8) % PM_NKNOB, (int32_t)((r >> 12) % 40u) - 20);
        else if (op == 78u)
            pm_home(&pm);
        else {
            uint32_t s0 = pm.n_step;
            step_hits = 0;
            step_lane_mask = 0;
            pm_tick(&pm, 32u * (1u + (r >> 8) % 6u));
            CHECK(pm.n_step - s0 <= 1u, "two steps in a tick");
        }
        if (nsounding() > worst)
            worst = nsounding();
        if (pm.mode == PM_DRUMS)
            CHECK(nsounding() == 0, "a synth note on in DRUMS at action %u", i);
    }
    CHECK(worst <= PM_MAX_HELD + 3u && worst <= 8u, "%u notes on at once", worst);
    CHECK(step_hits_max <= PM_STEP_HITS, "%u pattern and repeat hits in a step", step_hits_max);
    CHECK(lane_twice == 0, "a sound hit twice in a step");
    CHECK(off_unsounded == 0, "%u note-offs for notes that were not on", off_unsounded);
    for (i = 0; i < PM_NKEY; i++)
        pm_key(&pm, i, 0);
    pm_set_beat(&pm, 0);
    {
        uint32_t a = n_hit;
        run_ms(3000);
        CHECK(nsounding() == 0 && pm_notes_on(&pm) == 0, "%u notes stuck after random playing", nsounding());
        CHECK(n_hit == a, "%u hits with nothing held and the beat off", n_hit - a);
        CHECK(pm.held == 0 && pm.voiced == 0 && pm.norder == 0, "a key still held with every key up");
    }
    printf("  (most notes on at once %u, most pattern + repeat hits in a step %u, %u note-ons, %u hits)\n", worst,
           step_hits_max, n_on, n_hit);
}

/* long sessions: the saturating and wrapping counters */
static void test_long(void)
{
    uint32_t i, a;
    printf("a key held past its age counter's limit (6.8 hours), the clock running\n");
    reset();
    pm_set_mode(&pm, PM_DRUMS);
    pm_key(&pm, 0, 1);
    pm.age[0] = PM_AGE_MAX - 100u;
    pm.n_step = 0xFFFFFFF0u;
    for (i = 0; i < PM_NLANE; i++)
        pm.lane_hits[i] = 250;
    a = hit_src[PM_SRC_REPEAT];
    run_ms(5000);
    CHECK(pm.age[0] >= PM_AGE_MAX - 100u && pm.age[0] <= PM_AGE_MAX + 64u, "a held key's age ran away");
    CHECK(hit_src[PM_SRC_REPEAT] - a >= 8u, "the repeat stopped after a long hold");
    pm_key(&pm, 0, 0);
    pm_set_mode(&pm, PM_SYNTH);
    pm_key(&pm, 2, 1);
    pm_key(&pm, 5, 1);
    pm.multi_age = PM_AGE_MAX - 10u;
    run_ms(3000);
    CHECK(pm.multi_age <= PM_AGE_MAX + 64u, "the phrase's hold time ran away");
    pm_key(&pm, 2, 0);
    pm_key(&pm, 5, 0);
    run_ms(1000);
    CHECK(nsounding() == 0, "a note stuck after a long hold");
}

int main(void)
{
    test_synth_keys();
    test_synth_bounds();
    test_phrase();
    test_drum_keys();
    test_beat();
    test_knobs();
    test_transitions();
    test_mash();
    test_long();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "PASS", checks, fails);
    return fails != 0;
}
