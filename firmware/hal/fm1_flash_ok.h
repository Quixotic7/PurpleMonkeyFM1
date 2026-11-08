/* SPDX-License-Identifier: GPL-3.0-only */
/* The flash chips the storage accepts: the JEDEC id read with 0x9F at power-on (fm1_flash.h fl_jedec_ram) in
 * persist_boot (project.c, cr_shim.c, pm_shim.c). Any other answer: RAM-only, nothing read or written, and main.c
 * skips the update session (docs/INSTALL-COMPAT.md "A second flash chip").
 *
 * 0x856014: Puya P25Q80, 1 MiB, in the FM-1s up to 2026.
 * 0xB36014: another maker's 1 MiB part (same type and capacity bytes, 60 14), in the FM-1s sold late 2026 with the
 *           M-VAVE logo printed on the front. Taken from ChoralRoot 0.29.1 (its HISTORY 3y222). The driver uses only
 *           generic SPI NOR commands (9F, 05/35, 06, 20, 02, 0B), which the SDK firmware those units ship with uses
 *           on the same chip. Not tried on such a unit here. Its erase may be slower: fm1_flash.h waits it out
 *           (up to 2 s, fm1_flash_wait.h) instead of going back to XIP under a busy part.
 *
 * A function, not a macro: the id is read once (fl_jedec_ram is an SPI transaction). No device code in this header,
 * so the host tests include it as it is. */
#pragma once
#include <stdint.h>

static inline int fl_jedec_ok(uint32_t id) { return id == 0x856014u || id == 0xB36014u; }
