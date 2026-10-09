/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* PurpleMonkey FM-1: rigged pets. A pet drawn as separate body parts joined at pivots (tools/gen_pm_rig.py:
 * pm_rig.h; the art: docs/RIG-ART-SPEC.md, assets/purplemonkey/rig-64-modular/README.md), posed by turning each
 * part about its joint and played from a few key poses an animation, blended between. A pose is an angle a part
 * and an offset, so motion costs almost no flash; the parts are small, so a frame costs little to paint.
 *
 *   pm_rig_tick()   once an animation tick: the animation and its time -> the pose (each part's place and angle on
 *                   the screen, each layer's picture); repaints only when the pose changed
 *   pm_rig_paint()  into the tile canvas (pm_ui.c pg): the parts back to front, each rotated about its pivot
 *
 * Up to PM_RIG_MAXP (24) parts: the articulated pets have 20 to 22 (upper and lower limbs, hands, feet, a head
 * of layers). Which pet has which rig: PM_RIG_OF_PET (pm_rig.h: by the art folder's name); a pet with none is not
 * drawn (the exporter refuses to leave one out). pm_rig_ovr overrides it per pet (the emulator's PM_RIG_DEMO).
 *
 * Layers (PM_RL_*): eyes, mouth, nose and each ear are parts like any other, each with a few pictures of one size
 * and pivot (pm_rig_t.tex). The pose names a picture per layer, independently; painting such a part reads that
 * picture instead of its own. Nothing else of the rig knows: joint, parent, place in the order stay. (The
 * earlier art sets have one head with six whole faces: the HEAD layer.) Each expression has its own animation of
 * the face (pm_rig_t.face: keys of a picture per layer and the ears' angles), played apart from the body's by the
 * caller, who may then change single layers on top (an ear's flick at a note).
 *
 * Planted legs (pm_rig_t.leg): the foot stays on its spot whatever the body does. A leg of upper, lower and foot
 * bends at the knee to do it (two-bone inverse kinematics: the hip is where the body has it, the ankle where it
 * stands at rest, the knee to the side pm_rig_t.bend says), so the body can sink, rise a little, sway and lean
 * with both feet flat on the ground; a one-piece leg (the earlier sets) is aimed at its spot from the hip. A key
 * may move a planted foot from its rest spot (pm_rkey_t.fx, fy: a step, a kick) and turn it (the foot's angle).
 * Props: a part with parent -2 (the cat's xylophone) is a thing on the ground: it stays where the rest pose puts
 * it, upright.
 * On top of an animation the caller may turn any part further (pm_rig_tick's add): how the pets answer the music
 * note by note without a key pose for every case.
 *
 * Size: the parts are painted PM_RIG_ZOOM / 256 times as drawn, by the same nearest-pixel lookup that turns them.
 * Angles: 1/256 turn, clockwise on the screen. Included by pm_ui.c after its canvas helpers. */
#include "pm_rig.h"
#ifndef PM_RIG_ZOOM
#define PM_RIG_ZOOM 256          /* as drawn: the 64 px sets make a pet about 110 px tall */
#endif
#define PM_RZ(v) (((v) * PM_RIG_ZOOM) >> 8)

enum { PM_RA_IDLE, PM_RA_PLAY, PM_RA_DANCE };
enum { PM_RF_NEUTRAL, PM_RF_BLINK, PM_RF_HAPPY, PM_RF_SING, PM_RF_SURPRISED, PM_RF_SLEEPY };   /* gen_pm_rig.py EXPRS */
enum { PM_RL_EYES, PM_RL_MOUTH, PM_RL_NOSE, PM_RL_EAR_L, PM_RL_EAR_R, PM_RL_HEAD };             /* .. LAYERS */
enum { PM_RR_NOTE_L, PM_RR_NOTE_R, PM_RR_SNARE };                                               /* .. REACTS */
enum { PM_RV_EYES_BLINK = 1, PM_RV_NOSE_SCRUNCH = 1, PM_RV_EAR_FLICK = 3 };                     /* .. their variants */
#define PM_RIG_AUTO (-2)
static int8_t pm_rig_ovr[PM_NPET] = {PM_RIG_AUTO, PM_RIG_AUTO, PM_RIG_AUTO, PM_RIG_AUTO};

typedef struct {
    int16_t x[PM_RIG_MAXP], y[PM_RIG_MAXP];       /* each part's pivot on the screen */
    uint8_t a[PM_RIG_MAXP];                       /* its angle */
    uint8_t var[PM_RIG_NLAYER];                   /* each layer's picture */
} pm_rpose_t;
static struct {
    pm_rpose_t pose;
    uint8_t valid, anim;
    uint32_t t0;                                  /* when the animation began */
    const pm_rig_t *R;                            /* whose pose it is */
} rg;

static const pm_rig_t *pm_rig_for(uint32_t pet)
{
    int32_t i = pm_rig_ovr[pet % PM_NPET];
    if (i == PM_RIG_AUTO)
        i = PM_RIG_OF_PET[pet % PM_NPET];
    return i >= 0 && (uint32_t)i < PM_NRIG ? &PM_RIGS[i] : 0;
}
static int32_t pm_rsin(uint32_t a) { return sine_i(a << 24); }            /* Q15 */
static int32_t pm_rcos(uint32_t a) { return sine_i((a + 64u) << 24); }
/* atan2(y, x) in 1/256 turn (-128 .. 128), by x / (1 + 0.28 x^2) in the first octant: good to a degree */
static int32_t pm_ratan2(int32_t y, int32_t x)
{
    int32_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, t, a;
    if (!ax && !ay)
        return 0;
    t = ay <= ax ? ay * 256 / ax : ax * 256 / ay;                         /* 0 .. 256 */
    a = (t * 256 / (256 + ((72 * t / 256) * t) / 256)) * 163 / 1024;      /* 0 .. 32 */
    if (ay > ax)
        a = 64 - a;
    if (x < 0)
        a = 128 - a;
    return y < 0 ? -a : a;
}
/* the angle of a part that hangs down from its pivot when it points along (dx, dy) instead */
static int32_t pm_rang(int32_t dx, int32_t dy) { return pm_ratan2(-dx, dy); }
static uint32_t pm_rsqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    for (; b > v; b >>= 2)
        ;
    for (; b; b >>= 2) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
    }
    return r;
}
static void pm_rturn(int32_t vx, int32_t vy, uint32_t a, int32_t *ox, int32_t *oy)   /* (vx, vy) turned by a */
{
    int32_t c = pm_rcos(a), s = pm_rsin(a);
    *ox = (vx * c - vy * s + 16384) >> 15;
    *oy = (vx * s + vy * c + 16384) >> 15;
}

/* a planted leg of three parts: hip (hx, hy) where the body has it, ankle at (fx, fy); bend: the knee's side */
static void pm_rig_leg(const pm_rig_t *R, const int8_t *leg, int32_t bend, int32_t fx, int32_t fy, pm_rpose_t *P)
{
    const pm_rpart_t *up = &R->part[leg[0]], *lo = &R->part[leg[1]], *ft = &R->part[leg[2]];
    int32_t ux = PM_RZ((int32_t)lo->ax - up->px), uy = PM_RZ((int32_t)lo->ay - up->py);   /* hip -> knee, at rest */
    int32_t vx = PM_RZ((int32_t)ft->ax - lo->px), vy = PM_RZ((int32_t)ft->ay - lo->py);   /* knee -> ankle */
    int32_t l1 = (int32_t)pm_rsqrt((uint32_t)(ux * ux + uy * uy)), l2 = (int32_t)pm_rsqrt((uint32_t)(vx * vx + vy * vy));
    int32_t hx = P->x[leg[0]], hy = P->y[leg[0]], dx = fx - hx, dy = fy - hy;
    int32_t d = (int32_t)pm_rsqrt((uint32_t)(dx * dx + dy * dy)), c, phi, dir, kx, ky;
    if (l1 < 1) l1 = 1;
    if (l2 < 1) l2 = 1;
    d = clamp(d, (l1 > l2 ? l1 - l2 : l2 - l1) + 1, l1 + l2);             /* out of reach: as far as it goes */
    c = clamp((l1 * l1 + d * d - l2 * l2) * 1024 / (2 * l1 * d), -1024, 1024);          /* the hip's corner, cos Q10 */
    phi = pm_ratan2((int32_t)pm_rsqrt((uint32_t)(1024 * 1024 - c * c)), c);
    dir = pm_rang(dx, dy) + bend * phi;                                    /* where the thigh points */
    P->a[leg[0]] = (uint8_t)(dir - pm_rang(ux, uy));
    kx = hx + ((-l1 * pm_rsin((uint32_t)dir & 255u) + 16384) >> 15);
    ky = hy + ((l1 * pm_rcos((uint32_t)dir & 255u) + 16384) >> 15);
    P->x[leg[1]] = (int16_t)kx;
    P->y[leg[1]] = (int16_t)ky;
    P->a[leg[1]] = (uint8_t)(pm_rang(fx - kx, fy - ky) - pm_rang(vx, vy));
    P->x[leg[2]] = (int16_t)fx;                                            /* the foot: on its spot, flat */
    P->y[leg[2]] = (int16_t)fy;
    P->a[leg[2]] = 0;
}

/* animation anim at t ms into it (or, phase >= 0, at phase / 65536 of its loop: the beat's clock) -> pose.
 * ground: where the root's feet level is now (the caller sinks or lifts the body with it); floor_y: the ground
 * itself, where props and planted feet stay. add: per part, an angle on top of the animation's, 0 = none;
 * addx, addy: per part, screen px it is pushed on top of the key's pose, whichever way its parent is turned (the
 * root: the whole pet moved), 0 = none */
static void pm_rig_solve(const pm_rig_t *R, uint32_t anim, uint32_t t, int32_t phase, int32_t xc, int32_t ground,
                         int32_t floor_y, const int8_t *add, const int8_t *addx, const int8_t *addy, pm_rpose_t *P)
{
    const pm_rkey_t *K = R->anim[anim % PM_RIG_NANIM].key, *k0, *k1;
    const pm_rpart_t *root = &R->part[0];
    uint32_t n = R->anim[anim % PM_RIG_NANIM].n, i, total = 0, f;
    int32_t dx, dy, rx = xc, ry = floor_y - PM_RZ((int32_t)R->feet);      /* the root's pivot at rest */
    int8_t own[PM_RIG_MAXP];                      /* each part's own keyed angle (a planted foot's is its angle) */
    for (i = 0; i < n; i++)
        total += K[i].ms;
    if (phase >= 0)
        t = (uint32_t)(((uint64_t)total * (uint32_t)phase) >> 16);
    t %= total ? total : 1u;
    for (i = 0; i + 1u < n && t >= K[i].ms; i++)
        t -= K[i].ms;
    k0 = &K[i];
    k1 = &K[(i + 1u) % n];
    f = k0->ms ? t * 256u / k0->ms : 0u;
    f = (f * f * (768u - 2u * f)) >> 16;          /* ease in and out: 3f^2 - 2f^3 */
    dx = k0->dx + (((k1->dx - k0->dx) * (int32_t)f) >> 8);
    dy = k0->dy + (((k1->dy - k0->dy) * (int32_t)f) >> 8);
    for (i = 0; i < R->npart; i++) {
        const pm_rpart_t *p = &R->part[i];
        int32_t la = k0->a[i] + ((((int8_t)(k1->a[i] - k0->a[i])) * (int32_t)f) >> 8) + (add ? add[i] : 0);   /* the short way round */
        int32_t sx = k0->tx[i] + (((k1->tx[i] - k0->tx[i]) * (int32_t)f) >> 8);       /* the key may slide it off */
        int32_t sy = k0->ty[i] + (((k1->ty[i] - k0->ty[i]) * (int32_t)f) >> 8);       /* its joint (a prop: its spot) */
        own[i] = (int8_t)la;
        if (p->parent == -2) {                    /* a prop: on the ground where the rest pose has it, moved by the
                                                   * key and the caller (screen px), turned only as they say */
            P->a[i] = (uint8_t)la;
            P->x[i] = (int16_t)(rx + PM_RZ((int32_t)p->ax - root->px + sx) + PM_RZ(addx ? addx[i] : 0));
            P->y[i] = (int16_t)(ry + PM_RZ((int32_t)p->ay - root->py + sy) + PM_RZ(addy ? addy[i] : 0));
        } else if (p->parent < 0) {
            P->a[i] = (uint8_t)la;
            P->x[i] = (int16_t)(xc + PM_RZ(dx + (addx ? addx[i] : 0)));
            P->y[i] = (int16_t)(ground - PM_RZ((int32_t)R->feet) + PM_RZ(dy + (addy ? addy[i] : 0)));
        } else {
            const pm_rpart_t *q = &R->part[p->parent];
            int32_t ox, oy;
            pm_rturn(PM_RZ((int32_t)p->ax - q->px + sx), PM_RZ((int32_t)p->ay - q->py + sy), P->a[p->parent], &ox, &oy);
            P->a[i] = (uint8_t)(P->a[p->parent] + (uint32_t)la);
            P->x[i] = (int16_t)(P->x[p->parent] + ox + PM_RZ(addx ? addx[i] : 0));    /* .. and the caller pushes it, on screen */
            P->y[i] = (int16_t)(P->y[p->parent] + oy + PM_RZ(addy ? addy[i] : 0));
        }
    }
    for (i = 0; i < PM_RIG_NLEG && R->leg[i][0] >= 0; i++) {              /* the planted legs, over what the keys said */
        const int8_t *leg = R->leg[i];
        const pm_rpart_t *up = &R->part[leg[0]];
        int32_t fx = rx + PM_RZ((int32_t)up->ax - root->px), fy = ry + PM_RZ((int32_t)up->ay - root->py);   /* the hip at rest */
        fx += PM_RZ(k0->fx[i] + (((k1->fx[i] - k0->fx[i]) * (int32_t)f) >> 8));          /* .. and where the key has the foot */
        fy += PM_RZ(k0->fy[i] + (((k1->fy[i] - k0->fy[i]) * (int32_t)f) >> 8));
        if (leg[1] >= 0) {
            const pm_rpart_t *lo = &R->part[leg[1]], *ft = &R->part[leg[2]];
            fx += PM_RZ((int32_t)lo->ax - up->px) + PM_RZ((int32_t)ft->ax - lo->px);     /* .. and its ankle */
            fy += PM_RZ((int32_t)lo->ay - up->py) + PM_RZ((int32_t)ft->ay - lo->py);
            pm_rig_leg(R, leg, R->bend[i], fx, fy, P);
            P->a[leg[2]] = (uint8_t)own[leg[2]];  /* the foot: flat unless the key turns it */
        } else {                                  /* one stiff piece: aimed from the hip at where its foot stands */
            fy += PM_RZ((int32_t)up->h - 1 - up->py);
            P->a[leg[0]] = (uint8_t)pm_rang(fx - P->x[leg[0]], fy - P->y[leg[0]]);
        }
    }
}

/* one part into the canvas: every canvas pixel in its reach looks up the pixel of d (the part's picture, w x h
 * palette indices) it shows: turned back by a and shrunk by the zoom, the nearest one */
static void pm_rig_part(const pm_rpart_t *p, const uint8_t *d, int32_t X, int32_t Y, uint32_t a, const uint16_t *pal)
{
    int32_t c = (pm_rcos(a) << 8) / PM_RIG_ZOOM, s = (pm_rsin(a) << 8) / PM_RIG_ZOOM, x, y, r = PM_RZ((int32_t)p->r) + 1;
    int32_t x0 = X - r, x1 = X + r, y0 = Y - r, y1 = Y + r;
    if (x0 < pg.ox) x0 = pg.ox;
    if (y0 < pg.oy) y0 = pg.oy;
    if (x1 >= pg.ox + pg.w) x1 = pg.ox + pg.w - 1;
    if (y1 >= pg.oy + pg.h) y1 = pg.oy + pg.h - 1;
    for (y = y0; y <= y1; y++) {
        uint16_t *row = cv_px + (y - pg.oy) * pg.w - pg.ox;
        int32_t dy = y - Y, dx = x0 - X;
        int32_t u = dx * c + dy * s + ((int32_t)p->px << 15) + 16384;    /* the part's x, Q15: turned back by a */
        int32_t v = -dx * s + dy * c + ((int32_t)p->py << 15) + 16384;
        for (x = x0; x <= x1; x++, u += c, v -= s) {
            uint32_t ui = (uint32_t)(u >> 15), vi = (uint32_t)(v >> 15), k;
            if (u < 0 || v < 0 || ui >= p->w || vi >= p->h)
                continue;
            k = d[vi * p->w + ui];
            if (k)
                row[x] = swap16(pal[k]);
        }
    }
}
static void pm_rig_paint(const pm_rig_t *R)
{
    uint32_t i, l;
    if (!rg.valid)
        return;
    for (i = 0; i < R->npart; i++) {
        uint32_t k = R->order[i];
        const pm_rpart_t *p = &R->part[k];
        const uint8_t *d = p->d;
        int32_t r = PM_RZ((int32_t)p->r) + 1;
        if (rg.pose.x[k] + r < pg.ox || rg.pose.x[k] - r >= pg.ox + pg.w || rg.pose.y[k] + r < pg.oy ||
            rg.pose.y[k] - r >= pg.oy + pg.h)
            continue;                             /* (not in this run of tiles) */
        for (l = 0; l < PM_RIG_NLAYER; l++)       /* a layer's part wears the picture the pose names */
            if (R->layer[l] == (int8_t)k && R->tex[l][rg.pose.var[l] % PM_RIG_NVAR])
                d = R->tex[l][rg.pose.var[l] % PM_RIG_NVAR];
        pm_rig_part(p, d, rg.pose.x[k], rg.pose.y[k], rg.pose.a[k], R->pal);
    }
}
static void pm_rig_dirty1(uint32_t i)             /* one part's reach */
{
    int32_t r = PM_RZ((int32_t)rg.R->part[i].r) + 1;
    pm_dirty(rg.pose.x[i] - r, rg.pose.y[i] - r, 2 * r + 1, 2 * r + 1);
}
static void pm_rig_dirty(void)                    /* each part's own reach (not the box round them all) */
{
    uint32_t i;
    for (i = 0; rg.valid && rg.R && i < rg.R->npart; i++)
        pm_rig_dirty1(i);
}
/* the animation asked for (a change starts it from its first key) and each layer's picture, now; the rest as
 * pm_rig_solve. Repaints the parts that moved, turned or changed picture, where they were and where they are */
static void pm_rig_tick(const pm_rig_t *R, uint32_t anim, const uint8_t *var, uint32_t now, int32_t phase, int32_t xc,
                        int32_t ground, int32_t floor_y, const int8_t *add, const int8_t *addx, const int8_t *addy)
{
    pm_rpose_t P;
    uint32_t i, l, was = rg.valid && rg.R == R;
    memset(&P, 0, sizeof P);
    if (!rg.valid || anim != rg.anim) {
        rg.anim = (uint8_t)anim;
        rg.t0 = now;
    }
    pm_rig_solve(R, anim, now - rg.t0, phase, xc, ground, floor_y, add, addx, addy, &P);
    for (l = 0; l < PM_RIG_NLAYER; l++)
        P.var[l] = (uint8_t)(R->layer[l] >= 0 && R->tex[l][var[l] % PM_RIG_NVAR] ? var[l] % PM_RIG_NVAR : 0u);
    if (!was) {                                   /* another pet's pose (or none): everything */
        pm_rig_dirty();
        rg.pose = P;
        rg.R = R;
        rg.valid = 1;
        pm_rig_dirty();
        return;
    }
    for (i = 0; i < R->npart; i++) {
        uint32_t ch = P.x[i] != rg.pose.x[i] || P.y[i] != rg.pose.y[i] || P.a[i] != rg.pose.a[i];
        for (l = 0; l < PM_RIG_NLAYER; l++)
            ch |= R->layer[l] == (int8_t)i && P.var[l] != rg.pose.var[l];
        if (!ch)
            continue;
        pm_rig_dirty1(i);                         /* where it was .. */
        rg.pose.x[i] = P.x[i];
        rg.pose.y[i] = P.y[i];
        rg.pose.a[i] = P.a[i];
        pm_rig_dirty1(i);                         /* .. and where it is */
    }
    memcpy(rg.pose.var, P.var, sizeof P.var);
}
