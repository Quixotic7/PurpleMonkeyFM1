/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: the panel and the screen (main loop; docs/PURPLEMONKEY.md).
 *
 *   pm_ui_init()   power-on: the sounds, the engine, the scene
 *   pm_ui_input()  as often as the main loop can: keys, buttons and knobs -> the engine's events (pm_out.c pm_post)
 *   pm_ui_frame()  once a frame: the engine's snapshot -> the sounds (pm_sound.c), the scene's events, the LEDs
 *   pm_ui_draw()   once a frame: the scene's changed tiles to the LCD, a bounded number a frame
 *
 * Controls (the printed names): FX = DRUMS, SEL = SYNTH, PLAY = BEAT, HOME = the familiar sound and groove;
 * SELECT = PET, PRESETS = BRIGHT, ALGORITHM = LENGTH, KNOB 1..4 = SPEED, BUSY, BOUNCE, SQUISH. Every other button
 * does nothing here (OCT- + OCT+ held stays the platform's update entry, main.c). No menus, nothing to save.
 *
 * The screen: a night scene in the pet's colours with the pet in front, drawn from the music: its pose (HELLO at
 * rest, PLAY while something sounds, TOGETHER with three keys held or a phrase running), a hop on every hit, a
 * bubble for a synth note, a ripple under its feet for a kick, a twinkling star for a hat, a coloured pop for the
 * other drums, four dots for the beat. A pet change shows its name for PM_TITLE_MS. Nothing fills the screen at
 * once: the scene is repainted in 16 px tiles, only where it changed, at most PM_TILE_BUDGET a frame (the LCD takes
 * ~1.3 us a pixel over SPI: a full repaint is ~77 ms, spread over a few frames), and at most PM_NPART particles
 * live at a time.
 * Included after gfx.c, panel.c, pm_out.c and pm_sound.c. */
#include "pm_sprites.h"

#define PM_TILE 16
#define PM_NT 15                                  /* 15 x 15 tiles */
#define PM_TILE_BUDGET 112u                       /* tiles a frame: 28672 px, ~38 ms of SPI: a whole spinning pet within
                                                   * one 50 ms animation tick (fewer, and a turn shows in two halves) */
#define PM_ANIM_MS 50u                            /* the scene moves at 20 frames a second */
#define PM_PET_SWAY 8                             /* px the pet's outline may move sideways (lean + wobble) */
#define PM_NPART 12u
#define PM_TITLE_MS 1000u
#define PM_TITLE_FADE_MS 240u
#define PM_GAUGE_MS 1300u
#define PM_PLAY_MS 1500u                          /* the PLAY pose after the last sound */
#define PM_GROUND 210                             /* the pet's feet */
#define PM_PALETTE (NPALETTES - 1u)                /* gfx.c: a colour palette (its text ramps are grey in MONO) */
#define PM_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

enum { PMP_FREE, PMP_BUBBLE, PMP_POP, PMP_RIPPLE };
typedef struct {
    uint8_t type, r;                              /* r: a bubble's radius, a pop's first radius */
    int16_t x, y;                                 /* where it began */
    uint16_t col;
    uint32_t t0;
} pm_part_t;

static struct {
    /* input */
    uint32_t keys, pend_down;                     /* the keys posted down; presses still to post */
    uint8_t mode, pet, beat;                      /* what was asked for (the engine follows within a block) */
    int8_t knob[PM_NKNOB];
    /* the engine as last seen */
    uint32_t n_note_on, n_step;
    uint8_t lane_hits[PM_NLANE];
    uint8_t seen;                                 /* the first snapshot was taken */
    /* the scene */
    uint16_t dirty[PM_NT];                        /* bit x of row y: tile (x, y) to repaint */
    uint8_t next_row;                             /* where the next frame starts looking (no starved rows) */
    uint8_t s_pet, s_mode, s_beat, s_pose, s_beatdot;
    int8_t s_hop;
    /* the pet's motion (pm_pet_move): a spring on its height (squash on a hit, stretch after), a lean that swaps
     * sides on each accent, a ripple up its body while it plays, breathing and a wave at rest */
    uint8_t s_flip, a_accent, a_amp, a_beatdot;
    int16_t a_vs, a_vel;                          /* height, 256 = as drawn; its speed */
    int8_t a_lean, a_lean_to;                     /* px at the head, + = right */
    uint16_t a_phase;                             /* the ripple's */
    uint32_t a_accent_t;
    uint8_t face, hello_done;                     /* the rigged pet's face (pm_face) */
    uint8_t strike[2], strike_i;                  /* each arm's strike, 255 = just thrown up, dying away; whose turn */
    int8_t aim[2];                                /* where along its instrument each arm last played */
    uint32_t face_t, hello_t, awake_t;            /* when it was put on; when the pet arrived; the last sign of life */
    uint32_t last_anim, last_sound, title_t0, gauge_t0, star_t0;
    uint8_t title_on, gauge_on, gauge_id, star_i, side;
    uint8_t title_fade;                           /* 0 full .. 4 gone */
    pm_part_t part[PM_NPART];
    uint16_t sky[240], hill_far, hill_near;
    uint8_t far_y[240], near_y[240];
    uint32_t frames, tiles, tiles_max;            /* diagnostics: frames drawn, tiles sent, the most in a frame */
} pu;

/* ------------------------------------------------------------- colours --- */
static uint16_t pm_mix(uint16_t a, uint16_t b, int32_t k)   /* a -> b, k of 256 */
{
    int32_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    return (uint16_t)((ar + (((br - ar) * k) >> 8)) << 11 | (ag + (((bg - ag) * k) >> 8)) << 5 |
                      (ab + (((bb - ab) * k) >> 8)));
}
/* a pet's night: the sky from top to bottom, the far hill, the near one */
static const uint16_t PM_WORLD[PM_NPET][4] = {
    [PM_MONKEY] = {PM_RGB(34, 14, 58), PM_RGB(92, 38, 96), PM_RGB(56, 26, 88), PM_RGB(34, 18, 62)},
    [PM_CAT] = {PM_RGB(24, 16, 66), PM_RGB(78, 48, 128), PM_RGB(48, 32, 102), PM_RGB(30, 20, 72)},
    [PM_DOG] = {PM_RGB(42, 16, 56), PM_RGB(108, 42, 98), PM_RGB(72, 30, 88), PM_RGB(44, 20, 62)},
    [PM_LLAMA] = {PM_RGB(10, 24, 58), PM_RGB(26, 80, 100), PM_RGB(18, 58, 86), PM_RGB(12, 36, 64)},
};
#define PM_C_CREAM PM_RGB(255, 244, 214)
#define PM_C_TEAL PM_RGB(96, 224, 200)
#define PM_C_CORAL PM_RGB(255, 120, 104)
#define PM_C_GREEN PM_RGB(90, 230, 120)
#define PM_C_STAR PM_RGB(150, 140, 200)
static const uint16_t PM_C_NOTE[5] = {PM_RGB(255, 214, 92), PM_RGB(120, 230, 200), PM_RGB(190, 150, 255),
                                      PM_RGB(255, 140, 170), PM_RGB(140, 200, 255)};
static const uint16_t PM_C_DOT[4] = {PM_RGB(255, 110, 110), PM_RGB(255, 214, 80), PM_RGB(90, 230, 150),
                                     PM_RGB(190, 140, 255)};
static const uint8_t PM_STAR[12][2] = {{22, 34}, {58, 16}, {92, 44}, {130, 20}, {160, 52}, {214, 86}, {36, 78},
                                       {76, 96}, {150, 92}, {228, 24}, {12, 118}, {186, 118}};
#define PM_MOON_X 198
#define PM_MOON_Y 40

/* --------------------------------------------------------------- tiles --- */
static void pm_dirty(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t tx0, ty0, tx1, ty1, ty;
    if (w <= 0 || h <= 0 || x >= 240 || y >= 240 || x + w <= 0 || y + h <= 0)
        return;
    tx0 = x < 0 ? 0 : x / PM_TILE;
    ty0 = y < 0 ? 0 : y / PM_TILE;
    tx1 = x + w > 240 ? PM_NT - 1 : (x + w - 1) / PM_TILE;
    ty1 = y + h > 240 ? PM_NT - 1 : (y + h - 1) / PM_TILE;
    for (ty = ty0; ty <= ty1; ty++)
        pu.dirty[ty] |= (uint16_t)(((1u << (tx1 - tx0 + 1)) - 1u) << tx0);
}
static void pm_dirty_all(void)
{
    uint32_t i;
    for (i = 0; i < PM_NT; i++)
        pu.dirty[i] = (1u << PM_NT) - 1u;
}

/* ------------------------------------------- drawing into a tile run --- */
/* the canvas (gfx.c cv_px, w x h) shows the screen from (ox, oy) */
static struct { int32_t ox, oy, w, h; } pg;

static inline void pg_px(int32_t x, int32_t y, uint16_t c)
{
    x -= pg.ox;
    y -= pg.oy;
    if ((uint32_t)x < (uint32_t)pg.w && (uint32_t)y < (uint32_t)pg.h)
        cv_px[y * pg.w + x] = swap16(c);
}
/* a disc of radius r, or with r_in > 0 a ring; sy: the height as a fraction of the width, in 1/8 (8 = a circle) */
static void pg_disc(int32_t cx, int32_t cy, int32_t r, int32_t r_in, int32_t sy, uint16_t c)
{
    int32_t ry = r * sy / 8, x, y, x0 = cx - r, x1 = cx + r, y0 = cy - ry, y1 = cy + ry;
    uint16_t sc = swap16(c);
    if (x0 < pg.ox) x0 = pg.ox;
    if (y0 < pg.oy) y0 = pg.oy;
    if (x1 >= pg.ox + pg.w) x1 = pg.ox + pg.w - 1;
    if (y1 >= pg.oy + pg.h) y1 = pg.oy + pg.h - 1;
    for (y = y0; y <= y1; y++) {
        int32_t dy = (y - cy) * 8 / sy, d2;
        uint16_t *row = cv_px + (y - pg.oy) * pg.w - pg.ox;
        for (x = x0; x <= x1; x++) {
            d2 = (x - cx) * (x - cx) + dy * dy;
            if (d2 <= r * r && d2 >= r_in * r_in)
                row[x] = sc;
        }
    }
}
/* a sprite's top-left at (x, y), through a palette of PM_SPR_NCOL colours */
static void pg_sprite(const pm_sprite_t *s, int32_t x, int32_t y, const uint16_t *pal)
{
    int32_t r, r0 = pg.oy - y, r1 = pg.oy + pg.h - y;
    if (x >= pg.ox + pg.w || x + s->w <= pg.ox)
        return;
    if (r0 < 0) r0 = 0;
    if (r1 > s->h) r1 = s->h;
    for (r = r0; r < r1; r++) {
        const uint8_t *d = s->d + s->row[r];
        uint16_t *row = cv_px + (y + r - pg.oy) * pg.w;
        int32_t px = x - pg.ox;
        while (*d != 255u) {
            uint32_t n;
            px += *d++;
            for (n = *d++; n; n--, d++, px++)
                if ((uint32_t)px < (uint32_t)pg.w)
                    row[px] = swap16(pal[*d]);
        }
    }
}
/* the pet: its sprite standing on (xc, ybot), vs / 256 as tall as drawn (squash and stretch, from the feet), its
 * rows pushed sideways by a lean (px at the top, none at the feet) and a ripple (amp px at the top, travelling up),
 * mirrored when flip. A row of the screen takes the sprite's row at that height: no row is drawn twice or left out */
static void pg_pet(const pm_sprite_t *s, int32_t xc, int32_t ybot, const uint16_t *pal, int32_t vs, int32_t lean,
                   int32_t amp, uint32_t phase, int flip)
{
    int32_t hh = (s->h * vs) >> 8, top = ybot - hh, y, y0 = top, y1 = ybot;
    if (y0 < pg.oy) y0 = pg.oy;
    if (y1 > pg.oy + pg.h) y1 = pg.oy + pg.h;
    for (y = y0; y < y1; y++) {
        int32_t r = ((y - top) << 8) / vs, up, dx, px, step = flip ? -1 : 1;
        const uint8_t *d;
        uint16_t *row = cv_px + (y - pg.oy) * pg.w;
        if (r >= s->h)
            r = s->h - 1;
        up = s->h - 1 - r;                        /* how far above the feet */
        dx = lean * up / s->h + ((amp * up / s->h * sine_i((phase + (uint32_t)r * 900u) << 16)) >> 15);
        d = s->d + s->row[r];
        px = (flip ? xc + s->w / 2 - 1 : xc - s->w / 2) + dx - pg.ox;
        while (*d != 255u) {
            uint32_t n;
            px += step * (int32_t)*d++;
            for (n = *d++; n; n--, d++, px += step)
                if ((uint32_t)px < (uint32_t)pg.w)
                    row[px] = swap16(pal[*d]);
        }
    }
}

#include "pm_rig.c"              /* rigged pets: parts turned about joints (a pet without a rig: pg_pet above) */

/* ---------------------------------------------------------- the scene --- */
#if PM_SPR_POSES                 /* (the whole-pose cut-outs: only when pm_sprites.h was made with them; the app's is not) */
static const pm_sprite_t *pm_pose_sprite(void) { return &PM_SPR[pu.s_pet * PM_SPR_POSES + pu.s_pose]; }
static void pm_pet_box(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    const pm_sprite_t *s = pm_pose_sprite();
    int32_t hh = (s->h * pu.a_vs) >> 8;
    *x = 120 - s->w / 2 - PM_PET_SWAY;            /* (everywhere its outline can be at this height) */
    *y = PM_GROUND - pu.s_hop - hh;
    *w = s->w + 2 * PM_PET_SWAY;
    *h = hh;
}
#endif
static void pm_pet_dirty(void)
{
    if (pm_rig_for(pu.s_pet)) {
        pm_rig_dirty();
        return;
    }
#if PM_SPR_POSES
    {
    int32_t x, y, w, h;
    pm_pet_box(&x, &y, &w, &h);
    pm_dirty(x, y, w, h);
    }
#endif
}
static const pm_sprite_t *pm_title_sprite(void) { return &PM_SPR[PM_SPR_NAME + pu.s_pet]; }
#define PM_TITLE_Y 14
static void pm_title_dirty(void)
{
    const pm_sprite_t *s = pm_title_sprite();
    pm_dirty(120 - s->w / 2, PM_TITLE_Y, s->w, s->h);
}
#define PM_HUD_H 22
#define PM_DOTS_Y 228
static void pm_foot_dirty(void) { pm_dirty(0, PM_DOTS_Y - 12, 240, 24); }

static void pm_world(uint32_t pet)                /* the pet's sky and hills */
{
    const uint16_t *w = PM_WORLD[pet % PM_NPET];
    uint32_t i;
    for (i = 0; i < 240u; i++) {
        pu.sky[i] = pm_mix(w[0], w[1], (int32_t)(i * 256u / 240u));
        /* two rolling lines of hills (sine_i: Q15 for a 32-bit phase), each pet's shifted along */
        pu.far_y[i] = (uint8_t)(150 + ((sine_i((i * 23u + pet * 61u) << 22) * 11 + sine_i((i * 9u + 40u) << 23) * 6) >> 15));
        pu.near_y[i] = (uint8_t)(200 + ((sine_i((i * 7u + pet * 90u + 300u) << 23) * 9) >> 15));
    }
    pu.hill_far = w[2];
    pu.hill_near = w[3];
}

/* a particle's place now: its centre and radius (a ripple: its half width); 0 = it has ended */
static int pm_part_at(const pm_part_t *p, uint32_t now, int32_t *x, int32_t *y, int32_t *r)
{
    uint32_t age = now - p->t0;
    *x = p->x;
    *y = p->y;
    switch (p->type) {
    case PMP_BUBBLE:
        if (age >= 1600u)
            return 0;
        *y = p->y - (int32_t)(age * 70u / 1000u);                    /* 70 px a second, up */
        *x = p->x + ((sine_i(age << 22) * 5) >> 15);                 /* a slow sway */
        *r = p->r;
        return *y > PM_HUD_H + 8;
    case PMP_POP:
        if (age >= 330u)
            return 0;
        *r = p->r - (int32_t)(age * p->r / 330u);
        return *r > 0;
    case PMP_RIPPLE:
        if (age >= 400u)
            return 0;
        *r = 18 + (int32_t)(age * 46u / 400u);
        return 1;
    default:
        return 0;
    }
}
static void pm_part_dirty(const pm_part_t *p, uint32_t now)
{
    int32_t x, y, r;
    if (!pm_part_at(p, now, &x, &y, &r))
        return;
    if (p->type == PMP_RIPPLE)
        pm_dirty(x - r - 1, y - r / 4 - 2, 2 * r + 3, r / 2 + 5);
    else
        pm_dirty(x - r - 1, y - r - 1, 2 * r + 3, 2 * r + 3);
}
static void pm_spawn(uint32_t type, int32_t x, int32_t y, uint32_t r, uint16_t col, uint32_t now)
{
    uint32_t i, k = PM_NPART;
    for (i = 0; i < PM_NPART; i++) {
        if (pu.part[i].type == PMP_FREE) {
            k = i;
            break;
        }
        if (k == PM_NPART || now - pu.part[i].t0 > now - pu.part[k].t0)
            k = i;                                /* full: the oldest goes */
    }
    pm_part_dirty(&pu.part[k], now);
    pu.part[k].type = (uint8_t)type;
    pu.part[k].x = (int16_t)x;
    pu.part[k].y = (int16_t)y;
    pu.part[k].r = (uint8_t)r;
    pu.part[k].col = col;
    pu.part[k].t0 = now;
    pm_part_dirty(&pu.part[k], now);
}

/* The rigged pet's face (pm_rig.c PM_RF_*), from what is going on, highest first:
 *   SURPRISED  the first PM_FACE_HELLO_MS after the pet arrives (a pet change, power-on)
 *   HAPPY      three or more keys held, or a bloom running
 *   SING       a synth note held from the keys
 *   SLEEPY     nothing played and nothing touched for PM_FACE_SLEEP_MS
 *   BLINK      PM_FACE_BLINK_MS every PM_FACE_BLINK_EVERY_MS, only out of NEUTRAL
 *   NEUTRAL
 * These are moods, not lip-sync. A face stays at least PM_FACE_HOLD_MS before a lower one or a sideways one may
 * replace it, so keys going down and up quickly do not make it flicker; SURPRISED and the blink (a face that is
 * short on purpose) are not held back, and nothing interrupts SURPRISED but its own time running out. */
#define PM_FACE_HELLO_MS 900u
#define PM_FACE_SLEEP_MS 20000u
#define PM_FACE_BLINK_EVERY_MS 3900u
#define PM_FACE_BLINK_MS 150u
#define PM_FACE_HOLD_MS 350u
static uint32_t pm_face(uint32_t now, uint32_t party, uint32_t playing)
{
    uint32_t want;
    if (playing || pm_snap.held || (pu.gauge_t0 && now - pu.gauge_t0 < PM_GAUGE_MS))
        pu.awake_t = now;
    if (pu.face == PM_RF_SURPRISED && now - pu.hello_t < PM_FACE_HELLO_MS)
        return pu.face;
    if (now - pu.hello_t < PM_FACE_HELLO_MS && !pu.hello_done)
        want = PM_RF_SURPRISED;
    else if (party)
        want = PM_RF_HAPPY;
    else if (pm_snap.voiced)
        want = PM_RF_SING;
    else if (now - pu.awake_t >= PM_FACE_SLEEP_MS)
        want = PM_RF_SLEEPY;
    else
        want = now % PM_FACE_BLINK_EVERY_MS < PM_FACE_BLINK_MS ? PM_RF_BLINK : PM_RF_NEUTRAL;
    if (want == pu.face)
        return want;
    if (want != PM_RF_SURPRISED && want != PM_RF_BLINK && pu.face != PM_RF_BLINK && now - pu.face_t < PM_FACE_HOLD_MS)
        return pu.face;                           /* not yet: the face it has was put on a moment ago */
    if (want == PM_RF_SURPRISED)
        pu.hello_done = 1;
    pu.face = (uint8_t)want;
    pu.face_t = now;
    return want;
}

/* the pet's motion, once an animation tick: everything that makes it more than a picture. The three drawn poses
 * are its key frames (HELLO at rest; PLAY while it plays, with TOGETHER as a flourish every fourth accent; TOGETHER
 * and PLAY in turn, turning round every four, with three keys held or a phrase running); between them it is moved,
 * not redrawn: see pg_pet. Repaints its box only when something about it changed */
static void pm_pet_move(uint32_t now)
{
    uint32_t playing = pu.last_sound && now - pu.last_sound < PM_PLAY_MS;
    uint32_t party = pm_count(pm_snap.held) >= 3u || pm_snap.ph_note, pose, flip = 0, amp;
    int32_t vs = pu.a_vs, vel = pu.a_vel, lean = pu.a_lean, hop = pu.s_hop, to;
    uint32_t phase = pu.a_phase;
    if (pm_rig_for(pu.s_pet)) {                   /* a rigged pet: its animation for what is going on, its face, the hits */
        const pm_rig_t *R = pm_rig_for(pu.s_pet);
        uint32_t anim = party ? PM_RA_DANCE : playing ? PM_RA_PLAY : PM_RA_IDLE, k, e;
        /* with the beat on the play and dance loops ride its clock, a loop every anim.beats beats; without it, and
         * at rest, they run on their own time */
        uint32_t beats = R->anim[anim].beats ? R->anim[anim].beats : 1u;
        int32_t ph = pu.s_beat && anim != PM_RA_IDLE ? (int32_t)((pm_snap.bar_q16 * 4u / beats) & 0xFFFFu) : -1;
        int8_t add[PM_RIG_MAXP] = {0};
        uint8_t var[PM_RIG_NLAYER];
        int32_t nod = 0, planted = R->leg[0][0] >= 0;
        /* a note or a hit: a hand strikes its instrument (the cat's mallet on a bar, the monkey's stick on its
         * drum, the dog's maraca); a pet with no arms nods */
        for (k = 0; k < 2u; k++) {
            int32_t v = pu.strike[k], in = k ? v : -v;                       /* in: towards the pet's middle */
            if (R->fore[k] >= 0 && R->hand[k] >= 0) {
                /* elbows and wrists: the play pose holds the hands in over the instrument; a strike lifts the
                 * forearm a little at the elbow and whips the hand up at the wrist, then both fall back on the
                 * bar. The upper arm only reaches along the instrument for the note (aim) */
                add[R->arm[k]] = pu.aim[k];
                add[R->fore[k]] = (int8_t)(in * 9 / 255);                    /* 13 degrees */
                add[R->hand[k]] = (int8_t)(in * 25 / 255);                   /* 35 degrees */
            } else if (R->arm[k] >= 0 && R->hand[k] >= 0) {                  /* (one-piece arms with hands) */
                add[R->arm[k]] = (int8_t)(-in * 5 / 255 + pu.aim[k]);
                add[R->hand[k]] = (int8_t)(in * 26 / 255);
            } else if (R->arm[k] >= 0) {
                add[R->arm[k]] = (int8_t)(-in * 46 / 255);                   /* (whole arms: up to 65 degrees outwards) */
            }
            nod += k ? -v : v;
        }
        /* the head: with the beat it tips to one side and back each beat and turns left and right over the bar;
         * every strike nods it; the neck (the llama's) follows at half */
        if (R->head >= 0) {
            int32_t h = nod * (R->arm[0] >= 0 ? 6 : 20) / 255;
            if (pu.s_beat && anim != PM_RA_IDLE)
                h += (sine_i((uint32_t)pm_snap.bar_q16 << 18) * 9 + sine_i((uint32_t)pm_snap.bar_q16 << 16) * 6) >> 15;
            add[R->head] = (int8_t)h;
            if (R->neck >= 0)
                add[R->neck] = (int8_t)(-h / 2);
        }
        /* the face: the expression's picture for each layer, then the layers that answer on their own: an ear
         * flicks on the side that just struck, the nose scrunches at a big hit (a snare) */
        e = pm_face(now, party, playing);
        for (k = 0; k < PM_RIG_NLAYER; k++)
            var[k] = R->expr[e % PM_RIG_NEXPR][k];
        for (k = 0; k < 2u; k++)
            if (pu.strike[k] > 110u && e != PM_RF_SLEEPY)
                var[PM_RL_EAR_L + k] = PM_RV_EAR_FLICK;
        if (pu.s_hop >= 6)
            var[PM_RL_NOSE] = PM_RV_NOSE_SCRUNCH;
        for (k = 0; k < 2u; k++)
            pu.strike[k] = (uint8_t)(pu.strike[k] * 9u / 16u);
        if (hop > 0)
            pu.s_hop = (int8_t)(hop > 2 ? hop - 2 : 0);
        /* a hit moves the whole pet: one with planted feet sinks into its knees (its feet stay), another hops */
        pm_rig_tick(R, anim, var, now, ph, 120, planted ? PM_GROUND + pu.s_hop * 3 / 4 : PM_GROUND - pu.s_hop, PM_GROUND, add);
        return;
    }
    if (party) {
        pose = (pu.a_accent & 1u) ? 1u : 2u;
        flip = (pu.a_accent >> 2) & 1u;
    } else if (playing) {
        pose = (pu.a_accent & 3u) == 3u ? 2u : 1u;
    } else {
        pose = 0;
    }
    /* the height: a spring back to 256 (it overshoots: the stretch); at rest, breathing */
    vel = (vel * 9) / 16 + ((256 - vs) * 6) / 16;
    vs += vel;
    if (vs > 250 && vs < 262 && vel > -2 && vel < 2) {
        vel = 0;
        vs = playing || party ? 256 : 256 + ((sine_i((now / 4u) << 22) * 5) >> 15);   /* a breath every 4 s */
    }
    vs = clamp(vs, 200, 288);
    if (hop > 0)
        hop = hop > 2 ? hop - 2 : 0;
    /* the lean: towards its side; at rest upright, but for a wave every six seconds */
    to = playing || party ? pu.a_lean_to : now % 6000u < 900u ? (now / 150u % 2u ? 4 : -4) : 0;
    lean += (to - lean) / 2 + (to > lean) - (to < lean);
    if ((to > pu.a_lean && lean > to) || (to < pu.a_lean && lean < to) || to == pu.a_lean)
        lean = to;
    amp = playing || party ? 3u : 0u;
    if (amp)
        phase += 5200u;
    if (pose == pu.s_pose && flip == pu.s_flip && vs == pu.a_vs && lean == pu.a_lean && hop == pu.s_hop &&
        amp == pu.a_amp && (uint16_t)phase == pu.a_phase)
        return;
    pm_pet_dirty();
    pu.s_pose = (uint8_t)pose;
    pu.s_flip = (uint8_t)flip;
    pu.a_vs = (int16_t)vs;
    pu.a_vel = (int16_t)vel;
    pu.a_lean = (int8_t)lean;
    pu.s_hop = (int8_t)hop;
    pu.a_amp = (uint8_t)amp;
    pu.a_phase = (uint16_t)phase;
    pm_pet_dirty();
}

static const char *const PM_KNOB_NAME[PM_NKNOB] = {"SPEED", "BUSY", "BOUNCE", "SQUISH", "BRIGHT", "LENGTH"};

/* the screen's rectangle (x0, y0, w, h) into the canvas, back to front */
static void pm_scene_paint(int32_t x0, int32_t y0, int32_t w, int32_t h, uint32_t now)
{
    int32_t x, y, i;
    cv_begin((uint32_t)w, (uint32_t)h, 0);
    pg.ox = x0;
    pg.oy = y0;
    pg.w = w;
    pg.h = h;
    for (y = 0; y < h; y++) {                     /* the sky and the hills */
        uint16_t *row = cv_px + y * w, sky = swap16(pu.sky[y0 + y]);
        uint16_t far = swap16(pm_mix(pu.hill_far, pu.sky[y0 + y], 60)), near = swap16(pu.hill_near);
        for (x = 0; x < w; x++)
            row[x] = y0 + y >= pu.near_y[x0 + x] ? near : y0 + y >= pu.far_y[x0 + x] ? far : sky;
    }
    if (y0 < 130) {                               /* the stars, the moon */
        for (i = 0; i < 12; i++) {
            int lit = pu.star_t0 && i == pu.star_i;
            if (lit)
                pg_disc(PM_STAR[i][0], PM_STAR[i][1], 3, 0, 8, PM_C_CREAM);
            else
                pg_px(PM_STAR[i][0], PM_STAR[i][1], PM_C_STAR);
        }
        for (y = PM_MOON_Y - 13; !pu.title_on && y <= PM_MOON_Y + 13; y++)   /* a crescent: a disc less a disc */
            for (x = PM_MOON_X - 13; x <= PM_MOON_X + 13; x++) {
                int32_t a = (x - PM_MOON_X) * (x - PM_MOON_X) + (y - PM_MOON_Y) * (y - PM_MOON_Y);
                int32_t b = (x - PM_MOON_X - 7) * (x - PM_MOON_X - 7) + (y - PM_MOON_Y + 3) * (y - PM_MOON_Y + 3);
                if (a <= 169 && b > 150)
                    pg_px(x, y, PM_C_CREAM);
            }
    }
    for (i = 0; i < (int32_t)PM_NPART; i++) {     /* behind the pet: the ripples */
        int32_t px, py, r;
        if (pu.part[i].type == PMP_RIPPLE && pm_part_at(&pu.part[i], now, &px, &py, &r))
            pg_disc(px, py, r, r - 3, 2, pu.part[i].col);
    }
    if (pm_rig_for(pu.s_pet))
        pm_rig_paint(pm_rig_for(pu.s_pet));
#if PM_SPR_POSES
    else
        pg_pet(pm_pose_sprite(), 120, PM_GROUND - pu.s_hop, PM_SPR_PAL[pm_pose_sprite()->pal], pu.a_vs, pu.a_lean,
               pu.a_amp, pu.a_phase, pu.s_flip);
#endif
    for (i = 0; i < (int32_t)PM_NPART; i++) {     /* in front: bubbles and pops */
        int32_t px, py, r;
        if (pu.part[i].type == PMP_FREE || pu.part[i].type == PMP_RIPPLE || !pm_part_at(&pu.part[i], now, &px, &py, &r))
            continue;
        if (pu.part[i].type == PMP_BUBBLE) {
            pg_disc(px, py, r, r - 2, 8, pu.part[i].col);
            pg_px(px - r / 3, py - r / 3, PM_C_CREAM);
        } else {
            pg_disc(px, py, r, 0, 8, pu.part[i].col);
        }
    }
    if (y0 < PM_HUD_H + 4) {                      /* the mode's word, the beat's triangle */
        const char *m = pu.s_mode == PM_DRUMS ? "DRUMS" : "SYNTH";
        uint16_t c = pu.s_mode == PM_DRUMS ? PM_C_CORAL : PM_C_TEAL, under = pu.sky[10];
        if (!pu.title_on)
            cv_text_on(8 - x0, 4 - y0, &AF_M, m, c, under);
        if (pu.s_beat)
            for (y = 0; y < 15; y++)              /* (222, 6) .. (233, 13): a play triangle */
                for (x = 0; x < (y < 8 ? y * 3 / 2 + 1 : (14 - y) * 3 / 2 + 1); x++)
                    pg_px(222 + x, 5 + y, PM_C_GREEN);
    }
    if (y0 + h > PM_DOTS_Y - 12) {                /* the foot: a knob's gauge, else the four beats */
        if (pu.gauge_on) {
            const pm_knob_t *k = &PM_KNOB[pu.gauge_id];
            int32_t n = k->max - k->min, v = pu.knob[pu.gauge_id] - k->min, bx = 96, bw = 132;
            cv_text_on(10 - x0, PM_DOTS_Y - 9 - y0, &AF_M, PM_KNOB_NAME[pu.gauge_id], PM_C_CREAM, pu.hill_near);
            for (i = 0; i <= n; i++)
                pg_disc(bx + i * bw / n, PM_DOTS_Y, i == v ? 6 : 2, 0, 8,
                        i <= v ? PM_C_NOTE[pu.gauge_id % 5u] : pm_mix(pu.hill_near, PM_C_CREAM, 90));
        } else {
            for (i = 0; i < 4; i++) {
                int on = pu.s_beat && i == pu.s_beatdot;
                pg_disc(66 + i * 36, PM_DOTS_Y, on ? 8 : 5, 0, 8,
                        on ? PM_C_DOT[i] : pm_mix(pu.hill_near, PM_C_DOT[i], pu.s_beat ? 120 : 60));
            }
        }
    }
    if (pu.title_on && y0 < PM_TITLE_Y + 70) {    /* the pet's name, fading out through its palette */
        const pm_sprite_t *s = pm_title_sprite();
        uint16_t pal[PM_SPR_NCOL];
        for (i = 0; i < (int32_t)PM_SPR_NCOL; i++)
            pal[i] = pm_mix(PM_SPR_PAL[s->pal][i], pu.sky[PM_TITLE_Y + 30], pu.title_fade * 64);
        pg_sprite(s, 120 - s->w / 2, PM_TITLE_Y, pal);
    }
}

/* ------------------------------------------------------------- the LEDs --- */
#ifndef LED_PLAY_GREEN
#define LED_PLAY_GREEN ((8u << 3) | 1u)            /* PLAY's green LED: column 8, row bit 1 (ui_input.c) */
#endif
static uint8_t pm_led_pos[41];                     /* (col << 3) | row bit, 0xFF = none (FM1_KEYMAP) */
static void pm_led_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        pm_led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    pm_led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
static void pm_led(uint8_t *m, uint32_t id)
{
    uint8_t q = pm_led_pos[id % 41u];
    if (q != 0xFF)
        m[q >> 3] |= (uint8_t)(1u << (q & 7u));
}
/* The hardware has white LEDs with three states (off, a dim glow, lit; hal/fm1_input.h): the keys glow and a held
 * key is lit (what the engine holds: a key from before a mode change stays a glow until it is pressed again);
 * DRUMS / SYNTH: the mode's button lit, the other a glow; PLAY (orange) lit with the beat on, its green LED on
 * each beat; HOME a glow; the buttons that do nothing are dark */
static void pm_leds(void)
{
    uint8_t lit[FM1_NCOL] = {0}, dim[FM1_NCOL] = {0};
    uint32_t k;
    for (k = 0; k < PM_NKEY; k++)
        pm_led((pm_snap.held >> k) & 1u ? lit : dim, 14u + k);
    pm_led(pm_snap.mode == PM_DRUMS ? lit : dim, panel.btn[B_FX]);
    pm_led(pm_snap.mode == PM_SYNTH ? lit : dim, panel.btn[B_SCL]);
    pm_led(dim, panel.btn[B_HOME]);
    pm_led(pm_snap.beat ? lit : dim, panel.btn[B_PLAY]);
    if (pm_snap.beat && pm_snap.step % 4u < 2u)
        lit[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    for (k = 0; k < FM1_NCOL; k++)
        fm1_led_dim[k] = (uint8_t)(dim[k] & ~lit[k]);
    for (k = 0; k < FM1_NCOL; k++)
        fm1_led[k] = lit[k];
}

/* ------------------------------------------------------------ the input --- */
static void pm_knob_turn(uint32_t id, int32_t d)
{
    int32_t v;
    if (!d)
        return;
    v = clamp(pu.knob[id] + d, PM_KNOB[id].min, PM_KNOB[id].max);
    pu.gauge_id = (uint8_t)id;                    /* (at a stop too: the knob answers) */
    pu.gauge_t0 = fm1_ms | 1u;
    if (v != pu.knob[id] && pm_post(PME_KNOB, id, v))
        pu.knob[id] = (int8_t)v;
}

static void pm_ui_input(void)
{
    static const uint8_t ENC_KNOB[NE] = {[EN_PRESET] = PM_K_BRIGHT, [EN_ALGO] = PM_K_LENGTH, [EN_K1] = PM_K_SPEED,
                                         [EN_K2] = PM_K_BUSY, [EN_K3] = PM_K_BOUNCE, [EN_K4] = PM_K_SQUISH};
    uint32_t pe = fm1_input_edges(0), now, k;
    int32_t d;
    pu.pend_down |= fm1_input_note_edges() & ((1u << PM_NKEY) - 1u);
    now = fm1_in.notes & ((1u << PM_NKEY) - 1u);
    /* buttons: an edge each */
    if ((pe >> panel.btn[B_FX]) & 1u)
        if (pm_post(PME_MODE, PM_DRUMS, 0))
            pu.mode = PM_DRUMS;
    if ((pe >> panel.btn[B_SCL]) & 1u)
        if (pm_post(PME_MODE, PM_SYNTH, 0))
            pu.mode = PM_SYNTH;
    if ((pe >> panel.btn[B_PLAY]) & 1u)
        if (pm_post(PME_BEAT, !pu.beat, 0))
            pu.beat = !pu.beat;
    if ((pe >> panel.btn[B_HOME]) & 1u)
        if (pm_post(PME_HOME, 0, 0)) {
            for (k = 0; k < PM_NKNOB; k++)
                pu.knob[k] = PM_KNOB[k].def;
            pu.gauge_t0 = 0;
        }
    /* keys: a press is posted before its release, and neither is dropped when the ring is full (a lost key-up would
     * be a note that never ends): it is tried again at the next scan */
    for (k = 0; k < PM_NKEY; k++) {
        uint32_t bit = 1u << k;
        if (pu.pend_down & bit) {
            if ((pu.keys & bit) && !pm_post(PME_KEY, k, 0))
                continue;
            pu.keys &= ~bit;
            if (!pm_post(PME_KEY, k, 1))
                continue;
            pu.keys |= bit;
            pu.pend_down &= ~bit;
        } else if ((pu.keys & bit) && !(now & bit)) {
            if (pm_post(PME_KEY, k, 0))
                pu.keys &= ~bit;
        }
    }
    /* knobs: a detent a step, each within its own short range */
    d = panel_enc(EN_SELECT);
    if (d) {
        uint32_t pet = (uint32_t)((int32_t)pu.pet + d % (int32_t)PM_NPET + (int32_t)PM_NPET) % PM_NPET;   /* round and round */
        if (pm_post(PME_PET, pet, 0))
            pu.pet = (uint8_t)pet;
    }
    for (k = EN_ALGO; k < NE; k++)
        pm_knob_turn(ENC_KNOB[k], panel_enc(k));
}

/* ------------------------------------------------------------ the frame --- */
static void pm_ui_init(void)
{
    uint32_t i;
    memset(&pu, 0, sizeof pu);
    palette_set(PM_PALETTE);                      /* (main.c's settings_init set the stored one: there is none) */
    pm_led_init();
    pm_sound_init();
    pm_out_init();
    pu.mode = PM_SYNTH;
    pu.pet = PM_CAT;
    for (i = 0; i < PM_NKNOB; i++)
        pu.knob[i] = PM_KNOB[i].def;
    pm_sound_apply(pu.pet, pu.knob[PM_K_BRIGHT], pu.knob[PM_K_LENGTH]);
    pu.s_pet = pu.pet;
    pu.s_mode = pu.mode;
    pu.a_vs = 256;
    pm_world(pu.s_pet);
    pu.title_on = 1;                              /* power-on greets with the pet's name too */
    pu.title_t0 = pu.hello_t = pu.awake_t = fm1_ms;
    pm_dirty_all();
}

static void pm_ui_frame(void)
{
    uint32_t now = fm1_ms, i, sounds = 0;
    int32_t note_side = -1, aim = 0;              /* a synth note: which hand plays it, and how far along the bars */
    int32_t hop = 0;
    pm_snapshot();
    pm_sound_apply(pm_snap.pet, pm_snap.knob[PM_K_BRIGHT], pm_snap.knob[PM_K_LENGTH]);
    pm_leds();
    if (!pu.seen) {                               /* (no events for what happened before the first look) */
        pu.seen = 1;
        pu.n_note_on = pm_snap.n_note_on;
        pu.n_step = pm_snap.n_step;
        memcpy(pu.lane_hits, pm_snap.lane_hits, sizeof pu.lane_hits);
        pu.last_anim = now;
    }
    /* what changed at once: the pet, the mode, the beat */
    if (pm_snap.pet != pu.s_pet) {
        pu.s_pet = pm_snap.pet;
        rg.valid = 0;
        pu.hello_t = pu.awake_t = now;            /* a new pet: surprised to be here, and wide awake */
        pu.hello_done = 0;
        pm_world(pu.s_pet);
        pu.title_on = 1;
        pu.title_fade = 0;
        pu.title_t0 = now;
        pm_dirty_all();
    }
    if (pm_snap.mode != pu.s_mode || pm_snap.beat != pu.s_beat) {
        pu.s_mode = pm_snap.mode;
        pu.s_beat = pm_snap.beat;
        pm_dirty(0, 0, 240, PM_HUD_H + 4);
        pm_foot_dirty();
    }
    /* the music since the last frame -> the scene */
    if (pm_snap.n_note_on != pu.n_note_on) {      /* a note: a bubble at the side its pitch is on, hop */
        int32_t n = pm_snap.last_note, x = 14 + ((n * 7) % 34) + (pu.side ? 176 : 0);
        pu.side ^= 1u;
        pm_spawn(PMP_BUBBLE, x, 176 - (n % 12) * 3, 6u + (uint32_t)(n % 3), PM_C_NOTE[n % 5], now);
        pu.n_note_on = pm_snap.n_note_on;
        hop = 5;
        sounds = 1;
        /* the low half of the keys is the left hand's, the high half the right's; within a hand's half the arm
         * reaches further out for the notes at its end (+ = towards the viewer's left) */
        note_side = n >= 66;
        aim = clamp(((note_side ? 75 : 57) - n) / 2, -6, 6);
    }
    for (i = 0; i < PM_NLANE; i++) {
        if (pm_snap.lane_hits[i] == pu.lane_hits[i])
            continue;
        pu.lane_hits[i] = pm_snap.lane_hits[i];
        sounds = 1;
        if (i == PM_L_KICK || i == PM_L_KICK2) {  /* a kick: a ripple under the feet */
            pm_spawn(PMP_RIPPLE, 120, PM_GROUND - 2, 0, pm_mix(pu.hill_near, PM_C_DOT[3], 150), now);
            hop = hop > 4 ? hop : 4;
        } else if (i == PM_L_SNARE || i == PM_L_SNARE2 || i == PM_L_CLAP) {
            hop = 8;                              /* a snare: the big hop */
        } else if (i == PM_L_CHH || i == PM_L_OHH || i == PM_L_SHAKER || i == PM_L_RIDE) {
            if (pu.star_t0)                       /* a hat: one star twinkles */
                pm_dirty(PM_STAR[pu.star_i][0] - 4, PM_STAR[pu.star_i][1] - 4, 9, 9);
            pu.star_i = (uint8_t)((pu.star_i + 5u) % 12u);
            pu.star_t0 = now | 1u;
            pm_dirty(PM_STAR[pu.star_i][0] - 4, PM_STAR[pu.star_i][1] - 4, 9, 9);
        } else {                                  /* toms, conga, bell, wood: a pop at one side */
            pm_spawn(PMP_POP, pu.side ? 204 : 36, 150 - (int32_t)(i * 5u), 9, PM_C_DOT[i % 4u], now);
            pu.side ^= 1u;
            hop = hop > 3 ? hop : 3;
        }
    }
    if (pm_snap.n_step != pu.n_step) {
        uint32_t dot = (pm_snap.step / 4u) % 4u;
        pu.n_step = pm_snap.n_step;
        if (pu.s_beat && dot != pu.s_beatdot) {
            pu.s_beatdot = (uint8_t)dot;
            if (!pu.gauge_on)
                pm_foot_dirty();
        }
    }
    if (sounds)
        pu.last_sound = now | 1u;
    {   /* the knob's gauge comes and goes */
        uint32_t on = pu.gauge_t0 && now - pu.gauge_t0 < PM_GAUGE_MS;
        static int16_t shown;
        int16_t show = (int16_t)(pu.gauge_id * 32 + pu.knob[pu.gauge_id]);
        if (on != pu.gauge_on || (on && show != shown)) {
            pu.gauge_on = (uint8_t)on;
            shown = show;
            pm_foot_dirty();
        }
    }
    if (sounds) {                                 /* an accent: each beat with the beat on, else a sound now and then */
        uint32_t beat_now = pu.s_beat ? pu.s_beatdot != pu.a_beatdot : now - pu.a_accent_t >= 240u;
        if (beat_now) {
            pu.a_beatdot = pu.s_beatdot;
            pu.a_accent_t = now;
            pu.a_accent++;
            pu.a_lean_to = (int8_t)((pu.a_accent & 1u) ? 5 : -5);
        }
    }
    if (hop) {                                    /* a note: its hand strikes; a drum hit: the hands take turns */
        if (note_side >= 0) {
            pu.strike_i = (uint8_t)note_side;
            pu.aim[note_side] = (int8_t)aim;
        } else {
            pu.strike_i ^= 1u;
        }
        pu.strike[pu.strike_i] = 255;
    }
    if (hop) {                                    /* a hit: down into its knees (the spring throws it back up) */
        pm_pet_dirty();
        pu.a_vs = (int16_t)(256 - 5 * hop);
        pu.a_vel = 0;
        if (hop > pu.s_hop)
            pu.s_hop = (int8_t)hop;
        pm_pet_dirty();
    }
    /* what moves: at PM_ANIM_MS */
    if (now - pu.last_anim >= PM_ANIM_MS) {
        uint32_t prev = pu.last_anim;
        pu.last_anim = now;
        pm_pet_move(now);
        for (i = 0; i < PM_NPART; i++) {
            pm_part_t *p = &pu.part[i];
            int32_t x, y, r;
            if (p->type == PMP_FREE)
                continue;
            pm_part_dirty(p, prev);
            if (!pm_part_at(p, now, &x, &y, &r))
                p->type = PMP_FREE;
            else
                pm_part_dirty(p, now);
        }
        if (pu.star_t0 && now - pu.star_t0 > 160u) {
            pu.star_t0 = 0;
            pm_dirty(PM_STAR[pu.star_i][0] - 4, PM_STAR[pu.star_i][1] - 4, 9, 9);
        }
        if (pu.title_on) {
            uint32_t age = now - pu.title_t0;
            uint32_t fade = age < PM_TITLE_MS ? 0u : 1u + (age - PM_TITLE_MS) * 3u / PM_TITLE_FADE_MS;
            if (fade != pu.title_fade) {
                pm_title_dirty();
                pu.title_fade = (uint8_t)(fade > 4u ? 4u : fade);
                if (fade >= 4u) {
                    pu.title_on = 0;
                    pm_dirty(0, 0, 240, PM_HUD_H + 4);   /* the mode's word comes back, and the moon */
                    pm_dirty(PM_MOON_X - 14, PM_MOON_Y - 14, 29, 29);
                }
            }
        }
    }
}

/* the changed tiles, a row's run at a time, at most PM_TILE_BUDGET a frame; the scan starts after the row the last
 * frame ended in. The keys are scanned between the runs (a blit holds the main loop for up to ~5 ms) */
static void pm_ui_draw(void)
{
    uint32_t n = 0, r, now = fm1_ms;
    pu.frames++;
    for (r = 0; r < PM_NT && n < PM_TILE_BUDGET; r++) {
        uint32_t ty = (pu.next_row + r) % PM_NT, tx = 0;
        while (pu.dirty[ty] && n < PM_TILE_BUDGET) {
            uint32_t len = 0;
            while (!((pu.dirty[ty] >> tx) & 1u))
                tx++;
            while (tx + len < PM_NT && ((pu.dirty[ty] >> (tx + len)) & 1u) && n + len < PM_TILE_BUDGET)
                len++;
            pu.dirty[ty] &= (uint16_t)~(((1u << len) - 1u) << tx);
            pm_scene_paint((int32_t)tx * PM_TILE, (int32_t)ty * PM_TILE, (int32_t)len * PM_TILE, PM_TILE, now);
            cv_blit(tx * PM_TILE, ty * PM_TILE);
            n += len;
            tx += len;
            pm_ui_input();
        }
        if (pu.dirty[ty])
            break;                                /* out of budget inside this row: it goes on next frame */
    }
    pu.next_row = (uint8_t)((pu.next_row + r) % PM_NT);
    pu.tiles += n;
    if (n > pu.tiles_max)
        pu.tiles_max = n;
}
