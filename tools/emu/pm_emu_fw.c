/* SPDX-License-Identifier: GPL-3.0-only */
/* The firmware side of the PurpleMonkey emulator: the firmware sources (pm_firmware.h) and the hooks of
 * emu_hooks.h. emu_fw.c (ChoralRoot's) with PurpleMonkey's power-on, frame and dump; the tick, the audio hook and
 * the MIDI queues are as there. Nothing is stored: there is no flash file here (PurpleMonkey saves nothing). */
#include <os/lock.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "emu_hooks.h"

emu_hal_t emu_hal;
/* hal/fm1_input.h FM1_KEYMAP: key id at (physical column, packed row bit), -1 = none */
const int8_t emu_keymap[6][EMU_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},          /* PA0: encoders */
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},          /* PA5 */
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},          /* PA6 */
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},          /* PA7 */
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},          /* PA8 */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},          /* PB7: encoder 6 */
};

#include "pm_firmware.h"

void emu_fw_options(const char *flash_path, int no_flash, int save_on_exit, int headless)
{
    (void)flash_path; (void)no_flash; (void)save_on_exit; (void)headless;
    emu_flash_path[0] = 0;                        /* (RAM only: nothing of PurpleMonkey's reads or writes it) */
}
int emu_fw_boot_options(int fail, const char *reason, int stage)
{
    (void)fail; (void)reason; (void)stage;        /* (the boot guard is main.c's: the device unit, not here) */
    return 1;
}

#define EMU_RELEASE_MS 9u            /* hal/fm1_input.h: a release needs ~9 ms open (FM1_DEB_RELEASE) */
static uint32_t in_btn, in_key;      /* the debounced state the tick delivers */
static uint32_t btn_ms[32], key_ms[32];/* ms each went down */
static int32_t master_knob = 512 * 16;

void emu_fw_init(int demo)
{
    struct timespec ts;
    uint32_t i;
    (void)demo;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    emu_t0_ns = (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
    if (emu_hal.ready == 2u)                      /* --bench: simulated time (set before init) */
        emu_sim = 1;
    emu_flash_open();
    palette_set(PM_PALETTE);                      /* (main.c settings_init on the device) */
    panel_init();
    pm_ui_init();                                 /* the sounds, the engine, the scene */
    if (getenv("PM_RIG_DEMO") && PM_RIG_PLACEHOLDER >= 0)   /* the placeholder doll on the Monkey: only when pm_rig.h
                                                             * was generated with it (tools/gen_pm_rig.py DIR ..) */
        pm_rig_ovr[PM_MONKEY] = PM_RIG_PLACEHOLDER;
    usb.up = 1;
    usb.config = 1;
    for (i = 0; i < EMU_NB; i++)
        emu_hal.btn_id[i] = panel.btn[i];
    for (i = 0; i < EMU_NE - 1u; i++) {
        emu_hal.enc_id[i] = panel.enc[i];
        emu_hal.enc_dir[i] = panel.dir[i];
    }
    emu_hal.led_play_green = LED_PLAY_GREEN;
    if (!emu_hal.master)
        emu_hal.master = 724;                     /* -> master_q12 2047, as felucca_init */
    master_knob = emu_hal.master * 16;
    lcd_fill(0, 0, 240, 240, 0);
    emu_hal.ready = 1;
}

/* the 1 ms timer: what fm1_timer5_irq / fm1_input_tick give the main loop */
void emu_fw_tick(uint32_t ms)
{
    uint32_t b, k, i, down;
    fm1_irq_off();
    if (emu_sim)
        emu_sim_us = ms * 1000u;
    if ((int32_t)(ms - fm1_ms) > 0)
        fm1_ms = ms;
    {   /* PM_EMU_MIDI=1 (tools/emu/test_pm.sh): a MIDI keyboard without one: C4 E4 on channel 1 at 1.0 s, off at
         * 1.5 s; a GM kick and a closed hat on channel 10 at 2.0 s; a note left on at 2.5 s, then CC 123 at 3.0 s */
        static int on = -1;
        static uint32_t last;
        if (on < 0)
            on = getenv("PM_EMU_MIDI") != 0;
        if (on && ms != last) {
            last = ms;
            if (ms == 1000u) { midi_in_event(0x09u | 0x90u << 8 | 60u << 16 | 100u << 24); midi_in_event(0x09u | 0x90u << 8 | 64u << 16 | 90u << 24); }
            if (ms == 1500u) { midi_in_event(0x08u | 0x80u << 8 | 60u << 16); midi_in_event(0x09u | 0x90u << 8 | 64u << 16); }
            if (ms == 2000u) { midi_in_event(0x09u | 0x99u << 8 | 36u << 16 | 110u << 24); midi_in_event(0x09u | 0x99u << 8 | 42u << 16 | 80u << 24); }
            if (ms == 2500u) midi_in_event(0x09u | 0x90u << 8 | 67u << 16 | 100u << 24);
            if (ms == 3000u) midi_in_event(0x0Bu | 0xB0u << 8 | 123u << 16);
        }
    }
    b = emu_hal.buttons | __atomic_exchange_n(&emu_hal.buttons_tap, 0u, __ATOMIC_SEQ_CST);
    k = emu_hal.keys | __atomic_exchange_n(&emu_hal.keys_tap, 0u, __ATOMIC_SEQ_CST);
    down = b & ~in_btn;
    for (i = 0; i < EMU_NBTN; i++) {
        uint32_t m = 1u << i;
        if (down & m)
            btn_ms[i] = ms;
        else if ((in_btn & m) && !(b & m) && ms - btn_ms[i] < EMU_RELEASE_MS)
            b |= m;                               /* (held at least the debounce time) */
    }
    in_btn = b;
    if (down)
        __atomic_fetch_or(&host_pressed, down, __ATOMIC_SEQ_CST);
    down = k & ~in_key;
    for (i = 0; i < EMU_NKEY; i++) {
        uint32_t m = 1u << i;
        if (down & m)
            key_ms[i] = ms;
        else if ((in_key & m) && !(k & m) && ms - key_ms[i] < EMU_RELEASE_MS)
            k |= m;
    }
    in_key = k;
    if (down)
        __atomic_fetch_or(&host_notes, down, __ATOMIC_SEQ_CST);
    fm1_in.buttons = in_btn;
    fm1_in.notes = in_key;
    for (i = 0; i < EMU_NENC; i++) {
        int32_t s = __atomic_exchange_n(&emu_hal.enc[i], 0, __ATOMIC_SEQ_CST);
        if (s)
            __atomic_fetch_add(&host_enc[i], s, __ATOMIC_SEQ_CST);
    }
    fm1_irq_on();
}

static void master_pot(void)                      /* main.c: the MASTER pot (ADC) through its IIR */
{
    int32_t a = emu_hal.master;
    uint32_t k10;
    a = a < 0 ? 0 : a > 1023 ? 1023 : a;
    master_knob += (a * 16 - master_knob) / 8;
    k10 = (uint32_t)(master_knob / 16);
    song.master_q12 = (k10 * k10) >> 8;
}

static uint32_t real_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(((uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec) / 1000u);
}

void emu_fw_frame(void)                           /* main.c's loop body (purplemonkey.c: the same calls) */
{
    uint32_t t0, d;
    fm1_irq_off();
    t0 = real_us();
    master_pot();
    pm_ui_input();
    pm_ui_frame();                                /* the engine's snapshot, the sounds, the LEDs */
    d = real_us() - t0;
    fm1_irq_on();
    if (d > emu_hal.ui_lock_max_us)
        emu_hal.ui_lock_max_us = d;
    pm_ui_draw();
}

void emu_fw_idle(void)                            /* main.c: the input scan while it waits for the next frame */
{
    fm1_irq_off();
    pm_ui_input();
    fm1_irq_on();
}

void emu_fw_audio(int16_t *out, uint32_t frames)  /* the ALNK0 ISR, one half buffer per EMU_BLOCK */
{
    uint32_t f, i;
    for (f = 0; f + HALF_FRAMES <= frames; f += HALF_FRAMES) {
        const int32_t *h;
        if (!emu_cpu_try()) {   /* the UI (or the timer) is inside a critical section */
            uint32_t t0 = real_us(), d;
            fm1_irq_off();
            d = real_us() - t0;
            if (d > emu_hal.audio_wait_max_us)
                emu_hal.audio_wait_max_us = d;
        }
        emu_half ^= 1u;
        fm1_alnk0_irq();
        h = &abuf[emu_half * HALF_WORDS];
        fm1_irq_on();
        for (i = 0; i < HALF_WORDS; i++) {        /* 24-bit (Q15 << OUT_SHIFT) -> Q15 = 0 dBFS */
            int32_t v = h[i] >> OUT_SHIFT;
            out[2u * f + i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
    }
}

int emu_fw_midi_in(uint32_t pkt)
{
    int ok = 0;
    fm1_irq_off();
    if (MQ - (mi_w - mi_r) >= 8u) {               /* (usb.c ep1_take: room kept for TRS MIDI) */
        midi_in_event(pkt);
        ok = 1;
    }
    fm1_irq_on();
    return ok;
}

int emu_fw_midi_out_take(uint32_t *pkt)
{
    int ok = 0;
    fm1_irq_off();
    if (mo_r != mo_w) {                           /* usb.c ep1_tx */
        *pkt = midi_out_q[mo_r % MQ];
        mo_r++;
        ok = 1;
    }
    fm1_irq_on();
    return ok;
}

void emu_fw_dump(void)
{
    static const char *const PET[PM_NPET] = {"monkey", "cat", "dog", "llama"};
    uint32_t c;
    fm1_irq_off();
    printf("fm1_in: notes %07X buttons %04X  fm1_ms %u\n", (unsigned)fm1_in.notes, (unsigned)fm1_in.buttons,
           (unsigned)fm1_ms);
    printf("  leds:");
    for (c = 0; c < FM1_NCOL; c++)
        printf(" %02X/%02X", fm1_led[c], fm1_led_dim[c]);
    printf("  (lit/dim per column)\n");
    printf("  pm: mode %s pet %s beat %u step %u bpm %u style %u knobs speed %d busy %d bounce %d squish %d sound %d tone %d wobble %d space %d length %d world %d\n",
           pm.mode == PM_DRUMS ? "drums" : "synth", PET[pm.pet % PM_NPET], (unsigned)pm.beat, (unsigned)pm.step,
           (unsigned)pm_bpm(&pm), (unsigned)pm.style, pm.knob[0], pm.knob[1], pm.knob[2], pm.knob[3], pm.knob[4], pm.knob[5], pm.knob[6], pm.knob[7], pm.knob[8], pm.knob[9]);
    {
        uint32_t i, sv = 0, dv = 0;
        for (i = 0; i < NVOICE; i++)
            sv += trk[PM_PART].v[i].active;
        for (i = 0; i < PM_NDRUM; i++)
            dv += pmd.on[i];
        printf("  pm: held %07X voiced %07X notes-on %u (engine holds %u) phrase %u  synth voices %u drum voices %u\n",
               (unsigned)pm.held, (unsigned)pm.voiced, (unsigned)pm.n_note_on, (unsigned)pm_notes_on(&pm),
               (unsigned)(pm.ph_note[0] | pm.ph_note[1] | pm.ph_bass), (unsigned)sv, (unsigned)dv);
    }
    printf("  pm: drum hits %u (voices taken %u) steps %u  events lost %u  ui: frames %u tiles %u (most in a frame %u)\n",
           (unsigned)pm.n_drum, (unsigned)pmd.steals, (unsigned)pm.n_step, (unsigned)pm_evq_lost, (unsigned)pu.frames,
           (unsigned)pu.tiles, (unsigned)pu.tiles_max);
    printf("  pet: rig %d face %u anim %u (0 neutral 1 blink 2 happy 3 sing 4 surprised 5 sleepy; rig -1: the cut-out)\n",
           pm_rig_for(pu.s_pet) ? (int)(pm_rig_for(pu.s_pet) - PM_RIGS) : -1, (unsigned)pu.face, (unsigned)rg.anim);
    printf("  pet: layers eyes %u mouth %u nose %u ears %u %u head %u, parts %u\n", rg.pose.var[0], rg.pose.var[1], rg.pose.var[2],
           rg.pose.var[3], rg.pose.var[4], rg.pose.var[5], rg.R ? (unsigned)rg.R->npart : 0u);
    printf("  master_q12 %u cpu %u%%  voices given up %u (overload sheds %u), stolen %u\n", (unsigned)song.master_q12,
           (unsigned)(song.cpu_q8 * 100u / 256u), (unsigned)voice_kills, (unsigned)shed_count, (unsigned)voice_steals);
    fm1_irq_on();
}

void emu_fw_ui_info(char *buf, uint32_t n)
{
    snprintf(buf, n, "pm pet %u pose %u hop %d tiles %u", (unsigned)pu.s_pet, (unsigned)pu.s_pose, (int)pu.s_hop,
             (unsigned)pu.tiles);
}
void emu_fw_stats(uint32_t *shed, uint32_t *cpu_pct)
{
    *shed = shed_count;
    *cpu_pct = song.cpu_q8 * 100u / 256u;
}
