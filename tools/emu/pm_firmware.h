/* SPDX-License-Identifier: GPL-3.0-only */
/* The firmware the PurpleMonkey emulator runs: the ONE include list (included once, by pm_emu_fw.c), in the order
 * of firmware/src/purplemonkey.c. As emu_firmware.h (ChoralRoot's), with PurpleMonkey's instrument in place of
 * ChoralRoot's: the sound side through tests/hostsim.c (engines, voices, FX, usb.c's MIDI queues), the emulator's
 * HAL (emu_hal_fw.h), the engine (pm_engine.c) ticking in the audio ISR through the mix_block shim below
 * (pm_out.c pm_audio_block, then fx.c's mix, which calls pm_out.c's events_block for the drums), the audio ISR
 * (audio.c), then the panel and the screen (pm_ui.c on gfx.c). */
#include <stddef.h>
#include <stdint.h>
#define main hostsim_main
#define FELUCCA_SEQ 0                                 /* as purplemonkey.c: no Felucca sequencer */
#define FELUCCA_SLICE 0
#define FELUCCA_VA 0                                  /* FM6 is the synth: no VA, no CZ-1 */
#define FELUCCA_CZ 0
#define FELUCCA_SAMPLE 0
#define FELUCCA_GRAIN 0
#define FELUCCA_DRUM 0                                /* (the drums are pm_drum_synth.c's, not the DRUM engine) */
#define FELUCCA_SLICER 0
#define FM6_POLY 8                                    /* as purplemonkey.c: FM6 capped at 8 voices */
#define mix_block fx_mix_block                        /* fx.c's mix; audio.c gets PurpleMonkey's below */
#include "../../tests/hostsim.c"
#undef mix_block
#undef main

#include "emu_hal_fw.h"
#include "../../firmware/src/pm_engine.c"
#include "../../firmware/src/pm_out.c"                /* the engine on the parts, the drums, events_block */
static void mix_block(int32_t *out, uint32_t n)        /* the audio ISR's block: the engine first, then the mix */
{
    pm_audio_block(n);
    fx_mix_block(out, n);
}
#include "../../firmware/src/audio.c"                 /* fm1_alnk0_irq: the audio ISR, called per block */

#define FELUCCA_KEYCAPS 0
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "EMU"
#endif
#include "../../firmware/src/gfx.c"
#include "../../firmware/src/panel.c"
#include "../../firmware/src/pm_sound.c"
#include "../../firmware/src/pm_ui.c"
