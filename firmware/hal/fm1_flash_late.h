/* SPDX-License-Identifier: GPL-3.0-only */
/* A late erase or page program is a success (the owner's decision, 2026-11-08): fl_wait_issued (fm1_flash_wait.h,
 * felucca-bsp's, unchanged) returns FL_ERR_LATE_READY (-4) for a part that reported idle after the normal bound
 * (500 ms erase, 10 ms page) but before the extended one (2 s, 50 ms). That part has finished: XIP comes back on an
 * idle part, and a slow finish says nothing against the data (the update loader still compares every sector it
 * writes). So the driver answers 0 and counts it in fl_late (console `cpu`: flash_late). A part
 * still busy at the extended bound is reset, as before: fl_reset_busy does not return.
 *
 * The includer supplies fm1_flash_wait.h (RAMINL, fl_rdsr, T4_CNT, fl_reset_busy) first. fl_late lives in RAM
 * (.bss), so the RAM driver may count with XIP off. No device code here: the host test includes it as it is
 * (tests/flash_late_test.c). */
#pragma once
#include <stdint.h>

static volatile uint32_t fl_late;                 /* erases / page programs that finished late (and counted as done) */

RAMINL int fl_wait_done(uint32_t issued, uint32_t normal, uint32_t extended, uint32_t *took_us)
{
    int rc = fl_wait_issued(issued, normal, extended, took_us);
    if (rc == FL_ERR_LATE_READY) {
        fl_late = fl_late + 1u;
        rc = 0;
    }
    return rc;
}
