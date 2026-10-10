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
 * SELECT = PET, PRESETS = SOUND, ALGORITHM = WORLD, KNOB 1..4 = SPEED, BUSY, BOUNCE, SQUISH in DRUMS and TONE,
 * WOBBLE, SPACE, LENGTH in SYNTH; ENV = KEYS, LFO = TUNE, EDIT = TALK (what the keys do in SYNTH). Every other button
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

/* a sound's particles (BUBBLE .. PUFF: a note's, by the world; POP a drum's; RIPPLE a kick's), then the world's own
 * movers (from CLOUD), which a sound's never push out */
enum { PMP_FREE, PMP_BUBBLE, PMP_AIR, PMP_BALLOON, PMP_STAR, PMP_FLOWER, PMP_PUFF, PMP_POP, PMP_FIREWORK, PMP_RIPPLE,
       PMP_CLOUD, PMP_FLAKE, PMP_FIREFLY, PMP_BAND, PMP_FISH, PMP_SHOOT, PMP_BUTTERFLY };
typedef struct {
    uint8_t type, r;                              /* r: a bubble's radius, a pop's first radius */
    int16_t x, y;                                 /* where it began */
    uint16_t col;
    uint32_t t0;
} pm_part_t;

typedef struct pm_world pm_world_t;               /* (below: the worlds) */
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
    uint8_t s_pet, s_mode, s_beat, s_pose, s_beatdot, s_world, s_style, s_tune, style, s_talking;
    uint32_t n_say, say_t0, name_due;             /* letters said (seen); when the last was; when the pet's name is due */
    char say_ch;                                  /* the letter shown big, 0 = none */
    int8_t s_hop;
    /* the pet's motion (pm_pet_move): a spring on its height (squash on a hit, stretch after), a lean that swaps
     * sides on each accent, a ripple up its body while it plays, breathing and a wave at rest */
    uint8_t s_flip, a_accent, a_amp, a_beatdot;
    int16_t a_vs, a_vel;                          /* height, 256 = as drawn; its speed */
    int8_t a_lean, a_lean_to;                     /* px at the head, + = right */
    uint16_t a_phase;                             /* the ripple's */
    uint32_t a_accent_t;
    uint8_t face, hello_done;                     /* the rigged pet's face (pm_face) */
    uint32_t rx_t[3];                             /* when each reaction was set off (0: it is over) */
    uint8_t strike[3], strike_i;                  /* the rig's reactions (PM_RR_*: a note by the left hand, by the right,
                                                   * a snare): how strong each is now, 0 .. 255; whose turn a drum hit is */
    int8_t aim[2];                                /* where along its instrument each arm last played */
    uint32_t face_t, hello_t, awake_t;            /* when it was put on; when the pet arrived; the last sign of life */
    uint32_t last_anim, last_sound, title_t0, gauge_t0, star_t0;
    uint8_t title_on, gauge_on, gauge_id, star_i, side;
    uint8_t title_fade;                           /* 0 full .. 4 gone */
    pm_part_t part[PM_NPART];
    uint16_t sky[240], hill_far, hill_near;
    uint8_t far_y[240], near_y[240];
    const pm_world_t *world;                      /* WORLD's (PM_WORLDS): its colours, its ground and what moves in it */
    uint32_t drift_t, drift_ph, seed;             /* the sky's slow drift towards the accent: its clock and phase; a PRNG */
    uint8_t drift_row;                            /* the tile row recoloured next */
    uint32_t frames, tiles, tiles_max;            /* diagnostics: frames drawn, tiles sent, the most in a frame */
} pu;

/* ------------------------------------------------------------- colours --- */
static uint16_t pm_mix(uint16_t a, uint16_t b, int32_t k)   /* a -> b, k of 256 */
{
    int32_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    return (uint16_t)((ar + (((br - ar) * k) >> 8)) << 11 | (ag + (((bg - ag) * k) >> 8)) << 5 |
                      (ab + (((bb - ab) * k) >> 8)));
}
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
#define PM_LETTER_Y 30
#define PM_LETTER_MS 700u
#define PM_NAME_SETTLE_MS 250u                    /* a pet change: its name is said this long after the last turn */
static void pm_title_dirty(void)
{
    const pm_sprite_t *s = pm_title_sprite();
    pm_dirty(120 - s->w / 2, PM_TITLE_Y, s->w, s->h);
}
#define PM_HUD_H 22
#define PM_DOTS_Y 228
static void pm_foot_dirty(void) { pm_dirty(0, PM_DOTS_Y - 12, 240, 24); }

/* ---------------------------------------------------------- the worlds --- */
/* WORLD (the ALGORITHM knob): the environment the pet plays in. Each: its sky (top, bottom, an accent the sky drifts
 * towards), its ground (two lines of hills, a seabed, the moon's surface, in its colours), what is drawn in the sky
 * (the moon and stars, a sun, a ringed planet, seaweed up from the ground), what moves about in it (the movers: so
 * many of a kind) and what a synth note sends up */
enum { PMG_HILLS, PMG_SEABED, PMG_MOON };
enum { PMD_NONE, PMD_MOONSTARS, PMD_STARS, PMD_SUN, PMD_PLANET, PMD_SEAWEED };
struct pm_world { const char *name; uint16_t top, bottom, accent, far, near; uint8_t ground, decor, mover, nmover, note; };
static const pm_world_t PM_WORLDS[PM_NWORLD] = {
    {"NIGHT", PM_RGB(24, 16, 66), PM_RGB(78, 48, 128), PM_RGB(110, 70, 170), PM_RGB(48, 32, 102), PM_RGB(30, 20, 72), PMG_HILLS, PMD_MOONSTARS, PMP_FIREFLY, 2, PMP_BUBBLE},
    {"SEA", PM_RGB(6, 36, 84), PM_RGB(10, 96, 124), PM_RGB(60, 190, 170), PM_RGB(16, 76, 96), PM_RGB(26, 62, 70), PMG_SEABED, PMD_SEAWEED, PMP_FISH, 3, PMP_AIR},
    {"BALLOONS", PM_RGB(70, 140, 220), PM_RGB(170, 210, 240), PM_RGB(255, 230, 170), PM_RGB(96, 176, 112), PM_RGB(62, 142, 82), PMG_HILLS, PMD_SUN, PMP_CLOUD, 3, PMP_BALLOON},
    {"SPACE", PM_RGB(4, 2, 18), PM_RGB(26, 10, 54), PM_RGB(120, 60, 200), PM_RGB(78, 74, 92), PM_RGB(54, 50, 66), PMG_MOON, PMD_PLANET, PMP_SHOOT, 1, PMP_STAR},
    {"MEADOW", PM_RGB(120, 190, 240), PM_RGB(200, 232, 250), PM_RGB(255, 240, 160), PM_RGB(112, 192, 92), PM_RGB(72, 152, 62), PMG_HILLS, PMD_SUN, PMP_BUTTERFLY, 3, PMP_FLOWER},
    {"SNOW", PM_RGB(16, 24, 60), PM_RGB(92, 112, 164), PM_RGB(200, 220, 255), PM_RGB(192, 202, 232), PM_RGB(152, 166, 212), PMG_HILLS, PMD_STARS, PMP_FLAKE, 5, PMP_PUFF},
};
/* the sky's colour at row y: the world's top .. bottom, drifting slowly towards its accent in a wave that
 * travels down the sky (the drift: one tile row a tick, so the whole sky washes over in a few seconds) */
static uint16_t pm_sky_at(uint32_t y)
{
    const pm_world_t *w = pu.world;
    uint16_t base = pm_mix(w->top, w->bottom, (int32_t)(y * 256u / 240u));
    int32_t k = (sine_i(pu.drift_ph + (y << 22)) + 32768) >> 9;   /* 0 .. 128 */
    return pm_mix(base, w->accent, k * 56 / 128);                /* (never more than a fifth of the way) */
}
static uint32_t pm_rnd(void) { pu.seed = pu.seed * 1664525u + 1013904223u; return pu.seed >> 8; }
static void pm_world(uint32_t world)              /* the world's sky and ground */
{
    uint32_t i;
    world %= PM_NWORLD;
    pu.world = &PM_WORLDS[world];
    pu.drift_ph = 0;
    pu.drift_row = 0;
    for (i = 0; i < 240u; i++) {
        pu.sky[i] = pm_sky_at(i);
        switch (pu.world->ground) {
        case PMG_SEABED:                          /* a sandy floor with a few dunes, the water's far edge above it */
            pu.far_y[i] = (uint8_t)(176 + ((sine_i((i * 13u + 50u) << 23) * 4) >> 15));
            pu.near_y[i] = (uint8_t)(203 + ((sine_i((i * 11u + 120u) << 23) * 4) >> 15));
            break;
        case PMG_MOON:                            /* a low, bare horizon */
            pu.far_y[i] = (uint8_t)(168 + ((sine_i((i * 17u + 20u) << 23) * 5) >> 15));
            pu.near_y[i] = (uint8_t)(201 + ((sine_i((i * 9u + 200u) << 23) * 5) >> 15));
            break;
        default:                                  /* two rolling lines of hills (sine_i: Q15 for a 32-bit phase), each world's own */
            pu.far_y[i] = (uint8_t)(150 + ((sine_i((i * 23u + world * 61u) << 22) * 11 + sine_i((i * 9u + 40u) << 23) * 6) >> 15));
            pu.near_y[i] = (uint8_t)(200 + ((sine_i((i * 7u + world * 90u + 300u) << 23) * 9) >> 15));
            break;
        }
    }
    pu.hill_far = pu.world->far;
    pu.hill_near = pu.world->near;
    for (i = 0; i < PM_NPART; i++)                 /* the old world's movers go, and its note particles */
        if (pu.part[i].type != PMP_FREE && pu.part[i].type != PMP_RIPPLE && pu.part[i].type != PMP_POP)
            pu.part[i].type = PMP_FREE;
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
    case PMP_AIR:                                                    /* an air bubble: small, quick, wobbling up */
        if (age >= 1400u)
            return 0;
        *y = p->y - (int32_t)(age * 95u / 1000u);
        *x = p->x + ((sine_i(age << 23) * 4) >> 15);
        *r = 2 + (int32_t)(p->r / 3u);
        return *y > PM_HUD_H + 6;
    case PMP_BALLOON:                                                /* a balloon: floats up slowly, swaying */
        if (age >= 4200u)
            return 0;
        *y = p->y - (int32_t)(age * 34u / 1000u);
        *x = p->x + ((sine_i(age << 21) * 8) >> 15);
        *r = p->r;
        return *y > PM_HUD_H;
    case PMP_STAR:                                                   /* a star: drifts up, twinkling */
        if (age >= 2200u)
            return 0;
        *y = p->y - (int32_t)(age * 30u / 1000u);
        *x = p->x + ((sine_i(age << 22) * 3) >> 15);
        *r = (age / 150u) & 1u ? 3 : 2;
        return *y > PM_HUD_H + 6;
    case PMP_FLOWER:                                                 /* a flower: grows up from the ground, stays, fades */
        if (age >= 2600u)
            return 0;
        *r = age < 400u ? (int32_t)(age * p->r / 400u) : p->r;
        return *r > 0;
    case PMP_PUFF:                                                   /* a puff: swells and thins out */
        if (age >= 700u)
            return 0;
        *r = (int32_t)(p->r + age * 10u / 700u);
        *y = p->y - (int32_t)(age * 20u / 1000u);
        return 1;
    case PMP_POP:
        if (age >= 330u)
            return 0;
        *r = p->r - (int32_t)(age * p->r / 330u);
        return *r > 0;
    case PMP_FIREWORK:                                               /* a burst: sparks fly out, then fall */
        if (age >= 800u)
            return 0;
        *r = 4 + (int32_t)(age * p->r / 800u);
        *y = p->y + (int32_t)(age * age / 9000u);
        return 1;
    case PMP_RIPPLE:
        if (age >= 400u)
            return 0;
        *r = 18 + (int32_t)(age * 46u / 400u);
        return 1;
    case PMP_CLOUD:                                                  /* drifts right, 5 px a second */
        *x = p->x + (int32_t)(age * 5u / 1000u);
        *r = p->r;
        return *x - *r < 240;
    case PMP_FLAKE:                                                  /* falls, swaying, to the far hill */
        *y = p->y + (int32_t)(age * 16u / 1000u);
        *x = p->x + ((sine_i(age << 21) * 6) >> 15);
        *r = p->r;
        return *y < 150;
    case PMP_FIREFLY:                                                /* wanders, glows and fades every 1.6 s */
        if (age >= 6400u)
            return 0;
        *x = p->x + ((sine_i(age << 20) * 9) >> 15);
        *y = p->y + ((sine_i((age << 20) + 0x40000000u) * 5) >> 15) - (int32_t)(age / 400u);
        *r = (age % 1600u) < 800u ? 2 : 1;
        return 1;
    case PMP_BAND:                                                   /* an aurora band: drifts and sways, 12 s */
        if (age >= 12000u)
            return 0;
        *y = p->y + ((sine_i(age << 18) * 8) >> 15);
        *x = p->x + (int32_t)(age * 3u / 1000u) - 18;
        *r = p->r;
        return 1;
    case PMP_FISH:                                                   /* a fish: swims across (r & 1: leftwards), bobbing */
        *x = (p->r & 1u) ? p->x - (int32_t)(age * 22u / 1000u) : p->x + (int32_t)(age * 22u / 1000u);
        *y = p->y + ((sine_i(age << 21) * 4) >> 15);
        *r = 4 + (int32_t)(p->r >> 1);
        return *x > -16 && *x < 256;
    case PMP_SHOOT:                                                  /* a shooting star: a quick diagonal streak */
        if (age >= 900u)
            return 0;
        *x = p->x + (int32_t)(age * 160u / 1000u);
        *y = p->y + (int32_t)(age * 60u / 1000u);
        *r = 2;
        return *x < 250;
    case PMP_BUTTERFLY:                                              /* a butterfly: wanders, flapping */
        if (age >= 9000u)
            return 0;
        *x = p->x + ((sine_i(age << 19) * 26) >> 15) + (int32_t)(age / 400u);
        *y = p->y + ((sine_i(age << 21) * 9) >> 15);
        *r = (age / 90u) & 1u ? 3 : 2;
        return *x < 250;
    default:
        return 0;
    }
}
static void pm_part_dirty(const pm_part_t *p, uint32_t now);
/* the sky's movers by style, kept up to their number: a free particle slot gets one when one is missing */
static void pm_world_movers(uint32_t now)
{
    uint32_t type = pu.world->mover, have = 0, i, k = PM_NPART;
    if (!type || !pu.world->nmover)
        return;
    for (i = 0; i < PM_NPART; i++) {
        if (pu.part[i].type == type)
            have++;
        else if (pu.part[i].type == PMP_FREE && k == PM_NPART)
            k = i;
    }
    if (have >= pu.world->nmover || k == PM_NPART || (pm_rnd() & 3u))   /* (one every few ticks, not all at once) */
        return;
    {
        pm_part_t *p = &pu.part[k];
        uint32_t r = pm_rnd();
        p->type = (uint8_t)type;
        p->t0 = now;
        p->col = pm_mix(pu.world->accent, pu.sky[40], 96);
        switch (type) {
        case PMP_CLOUD: p->x = (int16_t)-(20 + (int32_t)(r % 30u)); p->y = (int16_t)(30 + (r >> 5) % 70u); p->r = (uint8_t)(10 + (r >> 12) % 9u); break;
        case PMP_FLAKE: p->x = (int16_t)(r % 240u); p->y = (int16_t)(PM_HUD_H + 2); p->r = (uint8_t)(1 + (r >> 9) % 2u); break;
        case PMP_FIREFLY: p->x = (int16_t)(20 + r % 200u); p->y = (int16_t)(60 + (r >> 8) % 110u); p->r = 2; p->col = pm_mix(pu.world->accent, PM_C_CREAM, 80); break;
        case PMP_FISH: p->r = (uint8_t)(2 + (r >> 9) % 6u); p->x = (int16_t)((p->r & 1u) ? 250 : -10); p->y = (int16_t)(40 + (r >> 4) % 110u);
            p->col = PM_C_DOT[(r >> 14) % 4u]; break;
        case PMP_SHOOT: p->x = (int16_t)(10 + r % 120u); p->y = (int16_t)(PM_HUD_H + 4 + (r >> 8) % 40u); p->r = 2; p->col = PM_C_CREAM; break;
        case PMP_BUTTERFLY: p->x = (int16_t)(r % 160u); p->y = (int16_t)(60 + (r >> 8) % 90u); p->r = 3; p->col = PM_C_NOTE[(r >> 14) % 5u]; break;
        default: p->x = (int16_t)(r % 60u); p->y = (int16_t)(34 + (r >> 6) % 50u); p->r = (uint8_t)(5 + (r >> 12) % 3u); break;
        }
        pm_part_dirty(p, now);
    }
}
static void pm_part_dirty(const pm_part_t *p, uint32_t now)
{
    int32_t x, y, r;
    if (!pm_part_at(p, now, &x, &y, &r))
        return;
    if (p->type == PMP_RIPPLE)
        pm_dirty(x - r - 1, y - r / 4 - 2, 2 * r + 3, r / 2 + 5);
    else if (p->type == PMP_CLOUD)
        pm_dirty(x - 2 * r - 1, y - r - 1, 4 * r + 3, 2 * r + 3);
    else if (p->type == PMP_BAND)
        pm_dirty(x - r - 1, y - r - 6, 2 * r + 116, 2 * r + 12);
    else if (p->type == PMP_FISH)
        pm_dirty(x - 2 * r - 2, y - r - 1, 4 * r + 5, 2 * r + 3);
    else if (p->type == PMP_SHOOT)
        pm_dirty(x - 14, y - 7, 18, 10);
    else if (p->type == PMP_BALLOON)
        pm_dirty(x - r - 1, y - r - 1, 2 * r + 3, 3 * r + 10);
    else if (p->type == PMP_FLOWER)
        pm_dirty(x - r - 1, y - 3 * r - 4, 2 * r + 3, 3 * r + 6);
    else if (p->type == PMP_BUTTERFLY)
        pm_dirty(x - r - 2, y - r - 1, 2 * r + 5, 2 * r + 3);
    else if (p->type == PMP_FIREWORK)
        pm_dirty(x - r - 3, y - r - 3, 2 * r + 7, 2 * r + 7);
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
        if (pu.part[i].type >= PMP_CLOUD)
            continue;                             /* (the world's movers stay; a sound's particle makes way) */
        if (k == PM_NPART || now - pu.part[i].t0 > now - pu.part[k].t0)
            k = i;                                /* full: the oldest goes */
    }
    if (k == PM_NPART)
        k = 0;
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
    else if (pm_snap.voiced || pu.s_talking)      /* (saying something: the mouth moves) */
        want = PM_RF_SING;
    else if (now - pu.awake_t >= PM_FACE_SLEEP_MS)
        want = PM_RF_SLEEPY;
    else {                                        /* (a pet whose neutral face is an animation blinks in it, if it does) */
        const pm_rig_t *R = pm_rig_for(pu.s_pet);
        want = !(R && R->face[PM_RF_NEUTRAL].n > 1u) && now % PM_FACE_BLINK_EVERY_MS < PM_FACE_BLINK_MS ? PM_RF_BLINK : PM_RF_NEUTRAL;
    }
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
        int8_t add[PM_RIG_MAXP] = {0}, addx[PM_RIG_MAXP] = {0}, addy[PM_RIG_MAXP] = {0};
        int16_t acc[PM_RIG_MAXP] = {0}, accx[PM_RIG_MAXP] = {0}, accy[PM_RIG_MAXP] = {0};
        uint8_t var[PM_RIG_NLAYER];
        int32_t dip = 0;
        /* reactions (pm_rig_t.react, from moves.json): a note by the left hand, by the right, a snare. Each is a
         * pose on top of the animation (the mallet whipped up, the head nodding, the body dipping) that is put on
         * at once and dies away; several add up */
        for (k = 0; k < PM_RIG_NREACT; k++) {     /* how strong each is now: on over rise, full for hold, away over fade */
            uint32_t t = now - pu.rx_t[k], rise = R->react[k].rise, hold = R->react[k].hold, fade = R->react[k].fade;
            int32_t v = 0, i;
            if (!pu.rx_t[k])
                v = 0;
            else if (t < rise)
                v = (int32_t)(255u * t / rise);
            else if (t < rise + hold)
                v = 255;
            else if (t < rise + hold + fade) {
                int32_t u = (int32_t)(255u - 255u * (t - rise - hold) / fade);
                v = u * u / 255;                  /* (quickly at first, then settling) */
            } else
                pu.rx_t[k] = 0;
            pu.strike[k] = (uint8_t)v;
            if (!v)
                continue;
            for (i = 0; i < (int32_t)R->npart; i++) {
                acc[i] = (int16_t)(acc[i] + R->react[k].a[i] * v / 255);
                accx[i] = (int16_t)(accx[i] + R->react[k].tx[i] * v / 255);
                accy[i] = (int16_t)(accy[i] + R->react[k].ty[i] * v / 255);
            }
            dip += R->react[k].dip * v;
        }
        for (k = 0; k < 2u; k++)                  /* the arm reaches along its instrument for the note */
            if (R->arm[k] >= 0)
                acc[R->arm[k]] = (int16_t)(acc[R->arm[k]] + pu.aim[k]);
        /* the head with the beat (pm_rig_t.bob, one per animation, from moves.json): a sine each beat and one over
         * the bar, each scaled into the head's turn, the neck's turn, and the head pushed sideways and up and down */
        if (R->head >= 0 && pu.s_beat && R->bob[anim % PM_RIG_NANIM].on) {
            const pm_rbob_t *b = &R->bob[anim % PM_RIG_NANIM];
            int32_t sb = sine_i((uint32_t)pm_snap.bar_q16 << 18), sr = sine_i((uint32_t)pm_snap.bar_q16 << 16);
            acc[R->head] = (int16_t)(acc[R->head] + ((sb * b->head[0] + sr * b->head[1]) >> 15));
            accx[R->head] = (int16_t)(accx[R->head] + ((sb * b->x[0] + sr * b->x[1]) >> 15));
            accy[R->head] = (int16_t)(accy[R->head] + ((sb * b->y[0] + sr * b->y[1]) >> 15));
            if (R->neck >= 0)
                acc[R->neck] = (int16_t)(acc[R->neck] + ((sb * b->neck[0] + sr * b->neck[1]) >> 15));
        }
        for (k = 0; k < R->npart; k++) {
            add[k] = (int8_t)clamp(acc[k], -127, 127);
            addx[k] = (int8_t)clamp(accx[k], -127, 127);
            addy[k] = (int8_t)clamp(accy[k], -127, 127);
        }
        /* the face: the expression's picture for each layer, then the layers that answer on their own: an ear
         * flicks on the side that just played, the nose scrunches at a snare */
        e = pm_face(now, party, playing) % PM_RIG_NEXPR;
        {   /* the expression's own animation, on its own clock from when the face was put on: its key's picture
             * for each layer, the ears turned (blended towards the next key) on top of what the body gives them */
            const pm_rfkey_t *K = R->face[e].key, *k0, *k1;
            uint32_t n = R->face[e].n, total = 0, t, i, f;
            for (i = 0; i < n; i++)
                total += K[i].ms;
            t = (now - pu.face_t) % (total ? total : 1u);
            for (i = 0; i + 1u < n && t >= K[i].ms; i++)
                t -= K[i].ms;
            k0 = &K[i];
            k1 = &K[(i + 1u) % n];
            f = k0->ms ? t * 256u / k0->ms : 0u;
            f = (f * f * (768u - 2u * f)) >> 16;
            for (k = 0; k < PM_RIG_NLAYER; k++)
                var[k] = k0->var[k];
            for (k = 0; k < 2u; k++)
                if (R->layer[PM_RL_EAR_L + k] >= 0) {
                    int32_t p = R->layer[PM_RL_EAR_L + k];
                    add[p] = (int8_t)clamp(add[p] + k0->ear[k] + (((int8_t)(k1->ear[k] - k0->ear[k]) * (int32_t)f) >> 8), -127, 127);
                }
        }
        if (hop > 0)
            pu.s_hop = (int8_t)(hop > 2 ? hop - 2 : 0);
        pm_rig_tick(R, anim, var, now, ph, 120, PM_GROUND + dip / 255, PM_GROUND, add, addx, addy);
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

static const char *const PM_KNOB_NAME[PM_NKNOB] = {"SPEED", "BUSY", "BOUNCE", "SQUISH", "SOUND", "TONE", "WOBBLE", "SPACE", "LENGTH", "WORLD"};
/* the planet's ring: 20 points round an ellipse (28 x 9) */
static const int8_t PM_RING[20][2] = {{28, 0}, {27, 3}, {23, 5}, {17, 7}, {9, 9}, {0, 9}, {-9, 9}, {-17, 7}, {-23, 5}, {-27, 3},
                                       {-28, 0}, {-27, -3}, {-23, -5}, {-17, -7}, {-9, -9}, {0, -9}, {9, -9}, {17, -7}, {23, -5}, {27, -3}};
static const uint8_t PM_WEED_X[4] = {28, 62, 186, 214};
#define PM_SUN_X 196
#define PM_SUN_Y 46

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
    if (pu.world->ground == PMG_MOON && y0 + h > 160) {   /* craters on the moon's surface */
        static const uint8_t CR[5][3] = {{30, 211, 6}, {90, 216, 4}, {158, 213, 7}, {205, 217, 4}, {120, 219, 3}};
        for (i = 0; i < 5; i++) {
            pg_disc(CR[i][0], CR[i][1], CR[i][2], CR[i][2] - 2, 8, pm_mix(pu.hill_near, PM_C_CREAM, 50));
            pg_disc(CR[i][0] + 1, CR[i][1] + 1, CR[i][2] - 2, 0, 8, pm_mix(pu.hill_near, pu.world->top, 90));
        }
    }
    if (pu.world->decor == PMD_SEAWEED && y0 + h > 140) {   /* seaweed: four strands swaying up from the sand */
        for (i = 0; i < 4; i++) {
            int32_t top = 150 + (int32_t)(i & 1u) * 12;
            for (y = top; y < 212; y++) {
                int32_t sx = PM_WEED_X[i] + ((sine_i(((uint32_t)y << 25) + (now << 17) + ((uint32_t)i << 29)) * (212 - y)) >> 18);
                pg_px(sx, y, pm_mix(pu.world->accent, pu.hill_near, 110));
                pg_px(sx + 1, y, pm_mix(pu.world->accent, pu.hill_near, 150));
            }
        }
    }
    if (y0 < 130 && pu.world->decor == PMD_SUN) {           /* a sun with a soft halo */
        pg_disc(PM_SUN_X, PM_SUN_Y, 18, 0, 3, pu.world->accent);
        pg_disc(PM_SUN_X, PM_SUN_Y, 13, 0, 8, pm_mix(pu.world->accent, PM_C_CREAM, 120));
    }
    if (y0 < 130 && pu.world->decor == PMD_PLANET) {        /* a ringed planet */
        pg_disc(PM_SUN_X - 8, PM_SUN_Y + 4, 12, 0, 8, pm_mix(pu.world->accent, PM_C_DOT[0], 70));
        pg_disc(PM_SUN_X - 12, PM_SUN_Y, 5, 0, 8, pm_mix(pu.world->accent, PM_C_CREAM, 60));
        for (i = 0; i < 20; i++)
            if (PM_RING[i][1] >= 0 || PM_RING[i][0] * PM_RING[i][0] > 170)   /* (the ring passes behind the planet) */
                pg_disc(PM_SUN_X - 8 + PM_RING[i][0], PM_SUN_Y + 4 + PM_RING[i][1], 1, 0, 8, PM_C_CREAM);
    }
    if (y0 < 130 && (pu.world->decor == PMD_MOONSTARS || pu.world->decor == PMD_STARS || pu.world->decor == PMD_PLANET)) {   /* the stars, the moon */
        for (i = 0; i < 12; i++) {
            int lit = pu.star_t0 && i == pu.star_i;
            if (lit)
                pg_disc(PM_STAR[i][0], PM_STAR[i][1], 3, 0, 8, PM_C_CREAM);
            else
                pg_px(PM_STAR[i][0], PM_STAR[i][1], PM_C_STAR);
        }
        for (y = PM_MOON_Y - 13; !pu.title_on && pu.world->decor == PMD_MOONSTARS && y <= PM_MOON_Y + 13; y++)   /* a crescent: a disc less a disc */
            for (x = PM_MOON_X - 13; x <= PM_MOON_X + 13; x++) {
                int32_t a = (x - PM_MOON_X) * (x - PM_MOON_X) + (y - PM_MOON_Y) * (y - PM_MOON_Y);
                int32_t b = (x - PM_MOON_X - 7) * (x - PM_MOON_X - 7) + (y - PM_MOON_Y + 3) * (y - PM_MOON_Y + 3);
                if (a <= 169 && b > 150)
                    pg_px(x, y, PM_C_CREAM);
            }
    }
    for (i = 0; i < (int32_t)PM_NPART; i++) {     /* behind the pet: the world's movers, the ripples */
        int32_t px, py, r;
        const pm_part_t *p = &pu.part[i];
        if (p->type < PMP_RIPPLE || !pm_part_at(p, now, &px, &py, &r))
            continue;
        switch (p->type) {
        case PMP_RIPPLE: pg_disc(px, py, r, r - 3, 2, p->col); break;
        case PMP_CLOUD:                            /* three soft discs */
            pg_disc(px - r, py + r / 3, r * 2 / 3, 0, 8, p->col);
            pg_disc(px + r * 2 / 3, py + r / 4, r * 3 / 4, 0, 8, p->col);
            pg_disc(px, py, r, 0, 8, p->col);
            break;
        case PMP_BAND: {                           /* a wavy streak of soft discs, a glow with no edge */
            int32_t k;
            for (k = 0; k < 9; k++)
                pg_disc(px + k * 14, py + ((sine_i(((uint32_t)k << 28) + (now << 18)) * 4) >> 15), r, 0, 3, p->col);
            break; }
        case PMP_FIREFLY: pg_disc(px, py, r, 0, 8, r > 1 ? p->col : pm_mix(p->col, pu.sky[py < 240 && py >= 0 ? py : 0], 128)); break;
        case PMP_FISH: {                           /* a body, a tail, an eye */
            int32_t d = (p->r & 1u) ? 1 : -1;
            pg_disc(px, py, r, 0, 8, p->col);
            pg_disc(px + d * (r + 2), py, r / 2 + 1, 0, 8, p->col);
            pg_px(px - d * (r / 2), py - 1, PM_C_CREAM);
            break; }
        case PMP_SHOOT: {                          /* a streak behind the head */
            int32_t k;
            for (k = 0; k < 12; k++)
                pg_px(px - k, py - k * 3 / 8, k < 4 ? PM_C_CREAM : pm_mix(PM_C_CREAM, pu.sky[py < 240 && py >= 0 ? py : 0], 60 + k * 14));
            break; }
        case PMP_BUTTERFLY:                        /* two wings, flapping */
            pg_disc(px - r + 1, py, r, 0, 8, p->col);
            pg_disc(px + r - 1, py, r, 0, 8, p->col);
            pg_px(px, py, PM_C_CREAM);
            break;
        default: pg_disc(px, py, r, 0, 8, p->col); break;   /* a flake */
        }
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
        switch (pu.part[i].type) {
        case PMP_BUBBLE:
            pg_disc(px, py, r, r - 2, 8, pu.part[i].col);
            pg_px(px - r / 3, py - r / 3, PM_C_CREAM);
            break;
        case PMP_AIR:
            pg_disc(px, py, r, r - 1, 8, pm_mix(pu.part[i].col, PM_C_CREAM, 120));
            pg_px(px - 1, py - 1, PM_C_CREAM);
            break;
        case PMP_BALLOON: {                        /* an oval on a string */
            int32_t k;
            pg_disc(px, py, r, 0, 8, pu.part[i].col);
            pg_disc(px, py + 2, r - 1, 0, 8, pu.part[i].col);
            pg_px(px - r / 3, py - r / 3, PM_C_CREAM);
            for (k = 1; k <= r + 6; k++)
                pg_px(px + ((k & 2) ? 1 : 0), py + r + k, pm_mix(pu.part[i].col, PM_C_CREAM, 100));
            break; }
        case PMP_STAR:                             /* a four-point twinkle */
            pg_px(px, py, PM_C_CREAM);
            pg_px(px - r, py, pu.part[i].col); pg_px(px + r, py, pu.part[i].col);
            pg_px(px, py - r, pu.part[i].col); pg_px(px, py + r, pu.part[i].col);
            if (r > 2) { pg_px(px - 1, py, pu.part[i].col); pg_px(px + 1, py, pu.part[i].col); pg_px(px, py - 1, pu.part[i].col); pg_px(px, py + 1, pu.part[i].col); }
            break;
        case PMP_FLOWER: {                         /* a stem up from the ground, a head of petals */
            int32_t k, top = py - 2 * r - 2;
            for (k = py; k > top; k--)
                pg_px(px, k, PM_RGB(80, 170, 70));
            pg_disc(px - r / 2 - 1, top, r / 2 + 1, 0, 8, pu.part[i].col);
            pg_disc(px + r / 2 + 1, top, r / 2 + 1, 0, 8, pu.part[i].col);
            pg_disc(px, top - r / 2 - 1, r / 2 + 1, 0, 8, pu.part[i].col);
            pg_disc(px, top + r / 2 + 1, r / 2 + 1, 0, 8, pu.part[i].col);
            pg_disc(px, top, r / 2, 0, 8, PM_C_CREAM);
            break; }
        case PMP_PUFF:
            pg_disc(px, py, r, 0, 3, pm_mix(pu.part[i].col, PM_C_CREAM, 160));
            break;
        case PMP_FIREWORK: {                       /* eight sparks round the centre, two colours, a tail each */
            int32_t k, age = (int32_t)(now - pu.part[i].t0);
            for (k = 0; k < 8; k++) {
                int32_t sx = px + ((sine_i(((uint32_t)k << 29) + 0x40000000u) * r) >> 15), sy = py + ((sine_i((uint32_t)k << 29) * r) >> 15);
                uint16_t c = (k & 1) ? pu.part[i].col : PM_C_NOTE[(k >> 1) % 5u];
                pg_disc(sx, sy, age < 400 ? 2 : 1, 0, 8, c);
                pg_px(sx - ((sine_i(((uint32_t)k << 29) + 0x40000000u) * 3) >> 15), sy - ((sine_i((uint32_t)k << 29) * 3) >> 15), pm_mix(c, pu.sky[py < 240 && py >= 0 ? py : 0], 128));
            }
            break; }
        default:
            pg_disc(px, py, r, 0, 8, pu.part[i].col);
            break;
        }
    }
    if (y0 < PM_HUD_H + 4) {                      /* the mode's word, the beat's triangle */
        const char *m = pu.s_mode == PM_DRUMS ? "DRUMS" : pu.s_style == PM_ST_TUNE ? PM_TUNE[pm_tune_of(pu.s_pet, pu.s_world, pu.s_tune)].name : pu.s_style == PM_ST_TALK ? "TALK" : "SYNTH";
        uint16_t c = pu.s_mode == PM_DRUMS ? PM_C_CORAL : pu.s_style == PM_ST_TUNE ? PM_C_NOTE[0] : pu.s_style == PM_ST_TALK ? PM_C_NOTE[2] : PM_C_TEAL, under = pu.sky[10];
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
            cv_text_on(10 - x0, PM_DOTS_Y - 9 - y0, &AF_M, pu.gauge_id == PM_K_SOUND ? PM_PRESET_NAME[pu.knob[PM_K_SOUND] % PM_NPRESET]
                       : pu.gauge_id == PM_K_WORLD ? PM_WORLDS[pu.knob[PM_K_WORLD] % PM_NWORLD].name : PM_KNOB_NAME[pu.gauge_id], PM_C_CREAM, pu.hill_near);
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
    if (pu.say_ch && y0 < PM_LETTER_Y + 60 && y0 + h > PM_LETTER_Y) {   /* TALK: the letter said, big */
        char t[2] = {pu.say_ch, 0};
        cv_text_c(120 - x0, PM_LETTER_Y - y0, &AF_L, t, PM_C_CREAM, pu.sky[PM_LETTER_Y + 20]);
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
    pm_led(pm_snap.style == PM_ST_KEYS ? lit : dim, panel.btn[B_ENV]);
    pm_led(pm_snap.style == PM_ST_TUNE ? lit : dim, panel.btn[B_LFO]);
    pm_led(pm_snap.style == PM_ST_TALK ? lit : dim, panel.btn[B_EDIT]);
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
    if (PM_KNOB[id].wrap) {                       /* round and round */
        int32_t n = PM_KNOB[id].max - PM_KNOB[id].min + 1;
        v = ((pu.knob[id] + d - PM_KNOB[id].min) % n + n) % n + PM_KNOB[id].min;
    } else
        v = clamp(pu.knob[id] + d, PM_KNOB[id].min, PM_KNOB[id].max);
    pu.gauge_id = (uint8_t)id;                    /* (at a stop too: the knob answers) */
    pu.gauge_t0 = fm1_ms | 1u;
    if (v != pu.knob[id] && pm_post(PME_KNOB, id, v))
        pu.knob[id] = (int8_t)v;
}

static void pm_ui_input(void)
{
    static const uint8_t ENC_KNOB[3][NE] = {   /* by mode: the four KNOBs are the beat's in DRUMS, the sound's in SYNTH
                                                * (in TUNE, KNOB 1 is SPEED: the tune's pace while a key is held) */
        [PM_SYNTH] = {[EN_PRESET] = PM_K_SOUND, [EN_ALGO] = PM_K_WORLD, [EN_K1] = PM_K_TONE, [EN_K2] = PM_K_WOBBLE, [EN_K3] = PM_K_SPACE, [EN_K4] = PM_K_LENGTH},
        [PM_DRUMS] = {[EN_PRESET] = PM_K_SOUND, [EN_ALGO] = PM_K_WORLD, [EN_K1] = PM_K_SPEED, [EN_K2] = PM_K_BUSY, [EN_K3] = PM_K_BOUNCE, [EN_K4] = PM_K_SQUISH},
        [2] = {[EN_PRESET] = PM_K_SOUND, [EN_ALGO] = PM_K_WORLD, [EN_K1] = PM_K_SPEED, [EN_K2] = PM_K_WOBBLE, [EN_K3] = PM_K_SPACE, [EN_K4] = PM_K_LENGTH}};
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
    if ((pe >> panel.btn[B_ENV]) & 1u)
        if (pm_post(PME_STYLE, PM_ST_KEYS, 0))
            pu.style = PM_ST_KEYS;
    if ((pe >> panel.btn[B_LFO]) & 1u)
        if (pm_post(PME_STYLE, PM_ST_TUNE, 0))
            pu.style = PM_ST_TUNE;
    if ((pe >> panel.btn[B_EDIT]) & 1u)
        if (pm_post(PME_STYLE, PM_ST_TALK, 0))
            pu.style = PM_ST_TALK;
    if ((pe >> panel.btn[B_HOME]) & 1u)
        if (pm_post(PME_HOME, 0, 0)) {
            for (k = 0; k < PM_NKNOB; k++)
                if (k != PM_K_WORLD)
                    pu.knob[k] = (int8_t)pm_knob_def(k, pu.pet);
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
        if (pm_post(PME_PET, pet, 0)) {
            pu.pet = (uint8_t)pet;
            pu.knob[PM_K_SOUND] = (int8_t)pm_preset_home(pet);   /* (the engine does the same; the world stays) */
            pu.name_due = (fm1_ms + PM_NAME_SETTLE_MS) | 1u;      /* its name, once the turning stops */
        }
    }
    for (k = EN_ALGO; k < NE; k++)
        pm_knob_turn(ENC_KNOB[pu.mode == PM_DRUMS ? PM_DRUMS : pu.style == PM_ST_TUNE ? 2 : PM_SYNTH][k], panel_enc(k));
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
        pu.knob[i] = (int8_t)pm_knob_def(i, pu.pet);
    pm_sound_apply(pu.pet, pu.knob[PM_K_SOUND], pu.knob[PM_K_TONE], pu.knob[PM_K_WOBBLE], pu.knob[PM_K_SPACE], pu.knob[PM_K_LENGTH]);
    pu.s_pet = pu.pet;
    pu.s_mode = pu.mode;
    pu.s_world = (uint8_t)pu.knob[PM_K_WORLD];
    pu.a_vs = 256;
    pu.seed = 12345u;
    pm_world(pu.s_world);
    pu.title_on = 1;                              /* power-on greets with the pet's name too */
    pu.name_due = (fm1_ms + 1200u) | 1u;          /* .. and says it, once the name has shown */
    pu.title_t0 = pu.hello_t = pu.awake_t = fm1_ms;
    pm_dirty_all();
}

static void pm_ui_frame(void)
{
    uint32_t now = fm1_ms, i, sounds = 0;
    int32_t note_side = -1, aim = 0;              /* a synth note: which hand plays it, and how far along the bars */
    int32_t hop = 0;
    pm_snapshot();
    pm_sound_apply(pm_snap.pet, pm_snap.knob[PM_K_SOUND], pm_snap.knob[PM_K_TONE], pm_snap.knob[PM_K_WOBBLE], pm_snap.knob[PM_K_SPACE], pm_snap.knob[PM_K_LENGTH]);
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
        pu.title_on = 1;
        pu.title_fade = 0;
        pu.title_t0 = now;
        pm_dirty_all();
    }
    if ((uint8_t)pm_snap.knob[PM_K_WORLD] != pu.s_world) {   /* a new world */
        pu.s_world = (uint8_t)pm_snap.knob[PM_K_WORLD];
        pm_world(pu.s_world);
        pm_dirty_all();
    }
    if (pu.name_due && (int32_t)(now - pu.name_due) >= 0) {   /* the pet's name, the turning over */
        pu.name_due = 0;
        if (pm_post(PME_SAY, PM_W_MONKEY + (pu.pet % PM_NPET), 0))
            ;
    }
    if (pm_snap.n_say != pu.n_say) {              /* a letter said: shown big for a moment; the face sings it */
        char ch = pm_speech_letter(pm_snap.last_say);
        pu.n_say = pm_snap.n_say;
        if (ch) {
            pu.say_ch = ch;
            pu.say_t0 = now | 1u;
            pm_dirty(60, PM_LETTER_Y - 2, 120, 64);
        }
    }
    if (pu.say_ch && now - pu.say_t0 >= PM_LETTER_MS) {
        pu.say_ch = 0;
        pm_dirty(60, PM_LETTER_Y - 2, 120, 64);
    }
    pu.s_talking = pm_snap.talking;
    if (pm_snap.style != pu.s_style || pm_snap.tune_i != pu.s_tune) {   /* the keys' style, the tune: the HUD's word */
        pu.s_style = pm_snap.style;
        pu.s_tune = pm_snap.tune_i;
        pm_dirty(0, 0, 240, PM_HUD_H + 4);
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
        if (pu.world->note == PMP_FLOWER)        /* a flower grows at the ground, beside the pet */
            pm_spawn(PMP_FLOWER, 40 + ((n * 11) % 50) + (pu.side ? 110 : 0), PM_GROUND + 2 + (n % 5), 4u + (uint32_t)(n % 3), PM_C_NOTE[n % 5], now);
        else if (pu.world->note == PMP_BALLOON)
            pm_spawn(PMP_BALLOON, x, 205, 6u + (uint32_t)(n % 3), PM_C_NOTE[n % 5], now);
        else if (pu.world->note == PMP_STAR) {   /* space: a shooting star streaks from the note's side, and a twinkle */
            pm_spawn(PMP_SHOOT, pu.side ? 20 : 130, 40 + (n % 7) * 9, 2, PM_C_NOTE[n % 5], now);
            pm_spawn(PMP_STAR, x, 150 - (n % 12) * 3, 3, PM_C_NOTE[n % 5], now);
        } else
            pm_spawn(pu.world->note, x, 176 - (n % 12) * 3, 6u + (uint32_t)(n % 3), PM_C_NOTE[n % 5], now);
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
            hop = 8;                              /* a snare: the big hop, and the world's own burst */
            pu.rx_t[2] = now | 1u;
            switch (pu.world->note) {
            case PMP_STAR: pm_spawn(PMP_FIREWORK, 60 + (pu.side ? 120 : 0), 70 + (int32_t)(pu.a_accent % 3u) * 14, 26, PM_C_NOTE[pu.a_accent % 5u], now); break;   /* space: fireworks */
            case PMP_BALLOON: pm_spawn(PMP_FIREWORK, 120, 90, 30, PM_C_DOT[pu.a_accent % 4u], now); break;                  /* balloons: confetti */
            case PMP_AIR: pm_spawn(PMP_AIR, 100, 190, 8, PM_C_NOTE[1], now); pm_spawn(PMP_AIR, 140, 192, 9, PM_C_NOTE[1], now); break;   /* sea: bubbles up */
            case PMP_PUFF: pm_spawn(PMP_PUFF, 90, 150, 6, PM_C_CREAM, now); pm_spawn(PMP_PUFF, 150, 150, 6, PM_C_CREAM, now); break;    /* snow: puffs */
            case PMP_FLOWER: pm_spawn(PMP_FIREWORK, 120, 110, 18, PM_C_NOTE[2], now); break;                                             /* meadow: petals */
            default: break;
            }
            pu.side ^= 1u;
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
        pu.rx_t[pu.strike_i] = now | 1u;
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
        pm_world_movers(now);
        if (pu.world->decor == PMD_SEAWEED && (pu.frames & 3u) == 0u)   /* the seaweed sways: its columns, now and then */
            for (i = 0; i < 4; i++)
                pm_dirty(PM_WEED_X[i] - 10, 148, 22, 66);
        if (now - pu.drift_t >= 320u) {           /* the sky drifts: one tile row recoloured a tick */
            uint32_t y, ty = pu.drift_row;
            pu.drift_t = now;
            if (!ty)
                pu.drift_ph += 1u << 25;
            for (y = ty * PM_TILE; y < (ty + 1u) * PM_TILE && y < 240u; y++)
                pu.sky[y] = pm_sky_at(y);
            pm_dirty(0, (int32_t)(ty * PM_TILE), 240, PM_TILE);
            pu.drift_row = (uint8_t)((ty + 1u) % PM_NT);
        }
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
