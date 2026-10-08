/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors (on Felucca by Leo Kuroshita, Hügelton Instruments, through
 * ChoralRoot FM-1) */
/* PurpleMonkey FM-1: the engine (pm_engine.c) on Felucca's sound side, as ChoralRoot's cr_out.c puts its own there
 * (docs/PURPLEMONKEY.md, Architecture).
 *
 *   part 0 SYNTH  <- the engine's notes: FM6 with the pet's patch (pm_sound.c), voice.c's trk_note_on / off
 *   the drums     <- the engine's hits: SLOOP's synthesised models (pm_drum_synth.c), their own PM_NDRUM voices
 *                    outside the parts' voice budget, mixed into fx.c's dry mix and reverb send
 *
 * Everything that touches the engine runs in the audio ISR: pm_audio_block() at the top of every CTL-sample block
 * (the mix_block shim of purplemonkey.c / tools/emu/pm_firmware.h) drains what the UI posted (pm_post: a lock-free
 * single-producer ring) and ticks the engine with the sample clock; events_block() (fx.c calls it once its mix
 * buffers are cleared and before the parts render) does voice.c's engine switches and adds the drums. The UI never
 * calls a note path; it reads pm_snap (pm_snapshot, IRQ off).
 * Included after voice.c, fx.c, usb.c and pm_engine.c.
 *
 * As cr_out.c with FELUCCA_SEQ 0: Felucca's sequencer is not in the unit; the names the kept files call of it are
 * here (events_block, panic_req, chain_defaults, kb_out_tick). MIDI in plays the sounds (pm_midi_in below); nothing
 * is sent to MIDI out. */

/* ---------------------------------------------- what seq.c provided --- */
static volatile uint8_t panic_req;               /* bit per part: its notes released (M-UPGRADE) */
#define chain_defaults(c) ((void)0)              /* (main.c felucca_init: no song chain) */
#ifdef FM1_INPUT_LAT
static uint32_t kb_out_tick;                     /* (audio.c: seq.c's key latency stamp; nothing reads it here) */
#endif

/* ------------------------------------------------------------ the drums --- */
#define PM_NDRUM 6u
#include "pm_drum_synth.c"
#define PM_PART 0u                               /* the synth part */
#ifndef PM_DRUM_LEVEL
#define PM_DRUM_LEVEL 17000                      /* Q15: the kit against the synth part */
#endif
#ifndef PM_DRUM_SEND
#define PM_DRUM_SEND 2600                        /* Q15: a little of the room */
#endif
/* a pet's kit (tools/build.py: pm_drumkits.h = VINTAGE, LATIN, 808, JAZZ) */
static const uint8_t PM_PET_KIT[PM_NPET] = {[PM_MONKEY] = 1, [PM_CAT] = 0, [PM_DOG] = 2, [PM_LLAMA] = 3};
static struct {
    dsv_t ds[PM_NDRUM];
    uint8_t on[PM_NDRUM], lane[PM_NDRUM];
    uint32_t age[PM_NDRUM], clock;
    int32_t last[PM_NDRUM];                      /* a voice's last output: a cut voice fades from it (no step) */
    int32_t tail;
    int32_t peak;                                /* largest |output| since the UI last looked */
    uint32_t steals;                             /* hits that took a sounding voice (diagnostics) */
} pmd;
static int32_t pmd_buf[CTL];

static void pmd_cut(uint32_t i)
{
    pmd.on[i] = 0;
    pmd.tail += pmd.last[i];
    pmd.last[i] = 0;
}
static void pmd_hit(uint32_t kit, uint32_t lane, int32_t semi, uint32_t vel, int32_t squish)
{
    uint32_t i, v = 0;
    if (lane == PM_L_CHH)                        /* a closed hat chokes the open one */
        for (i = 0; i < PM_NDRUM; i++)
            if (pmd.on[i] && pmd.lane[i] == PM_L_OHH)
                pmd_cut(i);
    for (i = 0; i < PM_NDRUM; i++) {             /* the same sound again: its own voice (no pile of kicks) */
        if (pmd.on[i] && pmd.lane[i] == lane) {
            v = i;
            break;
        }
    }
    if (i == PM_NDRUM)
        for (i = 0; i < PM_NDRUM; i++) {         /* else a free voice, else the oldest */
            if (!pmd.on[i]) {
                v = i;
                break;
            }
            if (pmd.age[i] < pmd.age[v])
                v = i;
        }
    if (pmd.on[v]) {
        pmd.steals++;
        pmd_cut(v);
    }
    pmd.on[v] = 1;
    pmd.lane[v] = (uint8_t)lane;
    pmd.age[v] = ++pmd.clock;
    ds_on_lane(&pmd.ds[v], &DS_KITS[kit % DS_NKITS], lane, semi, vel, squish);
}
static void pmd_all_off(void)
{
    uint32_t i;
    for (i = 0; i < PM_NDRUM; i++)
        if (pmd.on[i])
            pmd_cut(i);
}
/* into fx.c's dry mix and reverb send (n <= CTL) */
static void pmd_mix(int32_t *ml, int32_t *mr, int32_t *rev, uint32_t n)
{
    uint32_t k, i;
    int32_t pk = pmd.peak;
    for (i = 0; i < n && pmd.tail; i++) {        /* declick, ~0.4 ms */
        ml[i] += pmd.tail;
        mr[i] += pmd.tail;
        pmd.tail -= pmd.tail / 16 + (pmd.tail > 0 ? 1 : -1);
    }
    for (k = 0; k < PM_NDRUM; k++) {
        if (!pmd.on[k])
            continue;
        if (!ds_render(&pmd.ds[k], pmd_buf, n))
            pmd.on[k] = 0;
        for (i = 0; i < n; i++) {
            int32_t s = mulq15(clamp(pmd_buf[i], -60000, 60000), PM_DRUM_LEVEL);
            pmd.last[k] = s;
            if (s > pk || -s > pk)
                pk = s < 0 ? -s : s;
            ml[i] += s;
            mr[i] += s;
            rev[i] += mulq15(s, PM_DRUM_SEND);
        }
        if (!pmd.on[k]) {
            pmd.tail += pmd.last[k];
            pmd.last[k] = 0;
        }
    }
    pmd.peak = pk;
}

/* ------------------------------------------------------------ the engine --- */
static pm_t pm;                                  /* owned by the audio ISR */
static volatile uint8_t pm_ready;                /* initialised (the ISR may run before the UI's power-on) */
static volatile uint32_t pm_samples;             /* audio samples since power-on (wraps) */

static void pm_cb_note_on(void *ud, uint8_t note, uint8_t vel)
{
    (void)ud;
    trk_note_on(&trk[PM_PART], note, vel);
}
static uint8_t pm_midi_on[128];                  /* a MIDI keyboard holds the note (below) */
static void pm_cb_note_off(void *ud, uint8_t note)
{
    (void)ud;
    if (!pm_midi_on[note & 127u])                /* (a MIDI key still holds it) */
        trk_note_off(&trk[PM_PART], note);
}
static void pm_cb_drum(void *ud, uint8_t lane, int8_t semi, uint8_t vel, uint8_t src)
{
    (void)ud;
    (void)src;
    pmd_hit(PM_PET_KIT[pm.pet % PM_NPET], lane, semi, vel, pm.knob[PM_K_SQUISH]);
}
static const pm_out_t PM_OUT = {pm_cb_note_on, pm_cb_note_off, pm_cb_drum};

/* ------------------------------------------------------------- MIDI in --- */
/* A bigger keyboard on USB or the TRS jack plays the same sounds (the audio ISR; usb.c's and midi_uart.c's queue of
 * USB-MIDI event packets). Channel 10: the pet's kit by the General MIDI drum map (pm_drum_synth.c DS_MAP). Any
 * other channel: the pet's synth voice, chromatic, with the key's own velocity; the part's voice cap bounds it
 * (voice.c takes the oldest), CC 120 / 123 and a queue overflow release everything it holds. It changes nothing
 * else: not the mode, the pet, the beat or the knobs; clock, program changes and the other controllers are read and
 * dropped. A note both a panel key and a MIDI key hold ends when both let go. Nothing is sent to MIDI out. */
static void pm_midi_forget(void)
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        if (pm_midi_on[n]) {
            pm_midi_on[n] = 0;
            if (!pm.cnt[n])
                trk_note_off(&trk[PM_PART], n);
        }
}
static void pm_midi_in(void)
{
    uint32_t w = mi_w;
    if (midi_in_overflow) {                      /* a note-off may be lost: let go of everything MIDI holds */
        mi_r = w;
        RING_PUBLISH();
        midi_in_overflow = 0;
        pm_midi_forget();
        return;
    }
    while (mi_r != w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        mi_r++;
        if (st < 0x80u || st >= 0xF0u)
            continue;
        if ((st & 0xF0u) == 0x90u && d2) {
            if ((st & 15u) == 9u) {
                int32_t semi;
                uint32_t lane = ds_lane(d1, &semi);
                pm.n_drum++;
                pm.lane_hits[lane % PM_NLANE]++;
                pmd_hit(PM_PET_KIT[pm.pet % PM_NPET], lane, semi, d2, pm.knob[PM_K_SQUISH]);
            } else {
                pm_midi_on[d1] = 1;
                pm.n_note_on++;
                pm.last_note = (uint8_t)d1;
                trk_note_on(&trk[PM_PART], d1, d2);
            }
        } else if ((st & 0xE0u) == 0x80u) {      /* note off, or note on at velocity 0 */
            if ((st & 15u) != 9u && pm_midi_on[d1]) {
                pm_midi_on[d1] = 0;
                if (!pm.cnt[d1])
                    trk_note_off(&trk[PM_PART], d1);
            }
        } else if ((st & 0xF0u) == 0xB0u && (d1 == 120u || d1 == 123u)) {
            pm_midi_forget();
        }
    }
}

/* the UI's events (main loop -> ISR). A full ring loses the event: a lost key-up would be a stuck note, so the UI
 * retries key events until they fit (pm_ui.c) */
enum { PME_KEY, PME_MODE, PME_BEAT, PME_PET, PME_KNOB, PME_HOME, PME_PANIC };
typedef struct { uint8_t op, a; int16_t v; } pm_ev_t;
#define PM_EVQ 64u                               /* a power of two */
static pm_ev_t pm_evq[PM_EVQ];
static volatile uint32_t pm_evq_w, pm_evq_r;
static volatile uint32_t pm_evq_lost;

static int pm_post(uint32_t op, uint32_t a, int32_t v)
{
    uint32_t w = pm_evq_w;
    if (w - pm_evq_r >= PM_EVQ) {
        pm_evq_lost++;
        return 0;
    }
    pm_evq[w % PM_EVQ].op = (uint8_t)op;
    pm_evq[w % PM_EVQ].a = (uint8_t)a;
    pm_evq[w % PM_EVQ].v = (int16_t)v;
    RING_PUBLISH();
    pm_evq_w = w + 1u;
    return 1;
}
static void pm_apply(const pm_ev_t *e)
{
    switch (e->op) {
    case PME_KEY: pm_key(&pm, e->a, e->v != 0); break;
    case PME_MODE: pm_set_mode(&pm, e->a); break;
    case PME_BEAT: pm_set_beat(&pm, e->a); break;
    case PME_PET: pm_set_pet(&pm, e->a); break;
    case PME_KNOB: pm_set_knob(&pm, e->a, e->v); break;
    case PME_HOME: pm_home(&pm); break;
    case PME_PANIC:
        pm_panic(&pm);
        pm_midi_forget();
        trk_all_off(&trk[PM_PART]);
        pmd_all_off();
        break;
    default: break;
    }
}

/* fx.c's, with the mix buffers cleared and before the parts render: the releases asked for, the engine switches,
 * then the drums into the dry mix */
static void events_block(uint32_t n)
{
    uint32_t i, pr = panic_req;
    panic_req = 0;
    if (pr && pm_ready) {
        pm_panic(&pm);
        pm_midi_forget();
        pmd_all_off();
    }
    for (i = 0; i < NTRK; i++) {
        if ((pr >> i) & 1u)
            trk_all_off(&trk[i]);
        engine_block(&trk[i]);                   /* (voice.c: a part's engine switch) */
    }
    pmd_mix(mix_l, mix_r, send_r, n);
}

static void pm_out_init(void)                    /* power-on (the audio ISR may already run: IRQ off) */
{
    fm1_irq_off();
    pm_ready = 0;
    pm_init(&pm, &PM_OUT, 0);
    memset(&pmd, 0, sizeof pmd);
    memset(pm_midi_on, 0, sizeof pm_midi_on);
    voice_fade_steal = 1;                        /* voice.c: a stolen voice fades out, its new note waits a block */
    fx_smooth = 1;                               /* fx.c: the limiter's gain eased */
    pm_evq_r = pm_evq_w = 0;
    pm_ready = 1;
    fm1_irq_on();
}

/* the audio ISR, once per CTL-sample block, before the block renders */
static void pm_audio_block(uint32_t n)
{
    uint32_t w = pm_evq_w;
    if (!pm_ready)
        return;
    while (pm_evq_r != w) {
        pm_apply(&pm_evq[pm_evq_r % PM_EVQ]);
        pm_evq_r++;
    }
    pm_midi_in();
    pm_samples += n;
    pm_tick(&pm, n);
}

/* ------------------------------------------------------- what the UI reads --- */
typedef struct {
    uint8_t mode, pet, beat, step;
    uint16_t bar_q16;                            /* where the clock is in the four beats (a bar) it is in, / 65536 */
    int8_t knob[PM_NKNOB];
    uint32_t held, voiced;                       /* the keys the engine holds, and those whose note sounds */
    uint32_t n_note_on, n_drum, n_step, samples;
    uint8_t last_note, ph_note;
    uint8_t lane_hits[PM_NLANE];
    uint8_t synth_voices, drum_voices;
    int32_t drum_peak;
} pm_snap_t;
static pm_snap_t pm_snap;

static void pm_snapshot(void)
{
    uint32_t i;
    fm1_irq_off();
    pm_snap.mode = pm.mode;
    pm_snap.pet = pm.pet;
    pm_snap.beat = pm.beat;
    pm_snap.step = pm.step;
    pm_snap.bar_q16 = (uint16_t)(((pm.step % 16u) * 256u + pm.pos / (pm_step_len(&pm, pm.step) / 256u + 1u)) * 16u);
    memcpy(pm_snap.knob, pm.knob, sizeof pm_snap.knob);
    pm_snap.held = pm.held;
    pm_snap.voiced = pm.voiced;
    pm_snap.n_note_on = pm.n_note_on;
    pm_snap.n_drum = pm.n_drum;
    pm_snap.n_step = pm.n_step;
    pm_snap.samples = pm_samples;
    pm_snap.last_note = pm.last_note;
    pm_snap.ph_note = pm.ph_note[0] | pm.ph_note[1] | pm.ph_bass;
    memcpy(pm_snap.lane_hits, pm.lane_hits, sizeof pm_snap.lane_hits);
    pm_snap.synth_voices = pm_snap.drum_voices = 0;
    for (i = 0; i < NVOICE; i++)
        pm_snap.synth_voices += trk[PM_PART].v[i].active;
    for (i = 0; i < PM_NDRUM; i++)
        pm_snap.drum_voices += pmd.on[i];
    pm_snap.drum_peak = pmd.peak;
    pmd.peak = 0;
    fm1_irq_on();
}
