/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors (after ChoralRoot's cr_shim.c) */
/* PurpleMonkey FM-1: the names main.c calls of Felucca's dropped UI (ui.c, ui_input.c, ui_draw.c, project.c,
 * editor.c), with PurpleMonkey's meaning. Device unit only (purplemonkey.c); the emulator calls pm_ui_* itself
 * (tools/emu/pm_emu_fw.c).
 *
 *   felucca_init (main.c's power-on): the parts get FM6 (pm_ui_init sets everything at the first frame); no steps,
 *     no patterns, no undo
 *   the main loop: ui_input -> pm_ui_input (the first call: pm_ui_init), ui_leds -> pm_ui_frame, ui_draw ->
 *     pm_ui_draw; OCT- + OCT+ held 2..5 s (main.c: the platform's update mode) shows its countdown over the scene
 *   persist_boot: the flash part is identified (the update needs it); nothing is read from it
 *   settings_poll / settings_save / ed_service / panel_setup: nothing (no settings, no SysEx service, no panel
 *     calibration: panel.c's measured default table) */

static struct {
    uint8_t home, force, uboot, page;             /* (main.c's fields of ui.c's state) */
} ui;
static uint32_t undo_depth;
static uint32_t pat_sig[NTRK];
static uint8_t pat_last[NTRK];
static uint8_t pm_booted;
static uint8_t pm_uboot_shown;

static void ui_message(const char *s) { (void)s; }           /* (main.c: "UPDATE CANCELLED": the scene comes back) */
static void track_defaults(track_t *t) { (void)t; }
static void track_defaults_steps(track_t *t) { (void)t; }
static void set_engine_of(track_t *t, uint32_t ei) { (void)ei; t->eng_req = ENGI_FM6; }
static void apply_preset_to(track_t *t, uint32_t pi) { (void)t; (void)pi; }
static void load_pat16(track_t *t, const uint8_t *note, const uint8_t *flags) { (void)t; (void)note; (void)flags; }
static uint32_t steps_sig(const track_t *t) { (void)t; return 0; }
static void settings_poll(void) {}
static void settings_save(void) {}
static void panel_setup(void) {}

static void persist_boot(void)         /* as cr_shim.c's, without the stores */
{
#if FELUCCA_FLASH
    uint32_t f = irq_save();
    CR_STAGE(BS_FLASH);
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;       /* the expected 1 MiB part, else no update entry */
    irq_restore(f);
    if (flash_ok)
        fl_plain_window_init();
#endif
}

/* main.c's splash(): the pet that greets, on its night (the IRQs are still off: a still picture) */
#define PM_SPLASH() pm_splash()
static void pm_splash(void)
{
    palette_set(PM_PALETTE);
    lcd_fill(0, 0, 240, 240, PM_WORLDS[pm_world_home(PM_CAT)].top);   /* the Cat's own world: its sky */
    draw_text_box(0, 100, 240, &AF_M, "PURPLEMONKEY", PM_C_CREAM, 1);
    draw_text_box(0, 130, 240, &AF_S, FELUCCA_VERSION, PM_C_STAR, 1);
}

static void ui_input(void)
{
    if (!pm_booted) {                  /* the first scan: PurpleMonkey's power-on */
        pm_ui_init();
        pm_booted = 1;
    }
    pm_ui_input();
}
static void ui_leds(void)
{
    if (pm_booted)
        pm_ui_frame();
}
static void ui_draw(void)
{
    if (!pm_booted)
        return;
    if (ui.uboot) {                    /* OCT- + OCT+ held 2..5 s (main.c): the update countdown, let go to cancel */
        if (ui.uboot != pm_uboot_shown) {
            char n[2] = {(char)('0' + ui.uboot % 10u), 0};
            pm_uboot_shown = ui.uboot;
            lcd_fill(0, 0, 240, 240, 0);
            draw_text_box(0, 80, 240, &AF_M, "UPDATE MODE", PM_C_CREAM, 1);
            draw_text_box(0, 110, 240, &AF_L, n, PM_C_CREAM, 1);
            draw_text_box(0, 160, 240, &AF_S, "let go to cancel", PM_C_STAR, 1);
        }
        ui.force = 0;
        return;
    }
    if (ui.force || pm_uboot_shown) {
        ui.force = 0;
        pm_uboot_shown = 0;
        pm_dirty_all();
    }
    pm_ui_draw();
}
