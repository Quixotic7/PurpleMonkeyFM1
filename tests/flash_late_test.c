/* SPDX-License-Identifier: GPL-3.0-only */
/* fm1_flash_late.h fl_wait_done, what fm1_flash.h's erase and page program call: a late finish (past the normal bound,
 * before the extended one) answers 0 and counts in fl_late; a quick one answers 0 and does not count; a part still
 * busy at the extended bound is reset (fl_reset_busy), never answered. The same model of the part and TIMER4 as
 * tests/flash_wait_test.c (felucca-bsp's), around the real fm1_flash_wait.h. */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

#define RAMINL static inline
static uint32_t now, ready_at;
static int stuck;
static jmp_buf reset_point;
static uint8_t fl_rdsr(uint8_t cmd)
{
    uint8_t sr = stuck || (int32_t)(ready_at - now) > 0 ? 0x03u : 0x00u;
    (void)cmd;
    now += 24u * 7u;                              /* a status read: 7 us */
    return sr;
}
#define T4_CNT now
static void fl_reset_busy(void) { longjmp(reset_point, 1); }
#include "../firmware/hal/fm1_flash_wait.h"
#include "../firmware/hal/fm1_flash_late.h"

static int fails;
static void check(const char *what, int ok)
{
    printf("flash_late: %-72s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

/* one operation: the part busy for busy_us (or for ever), the wait with the given bounds; -100 = reset */
static int op(uint32_t start, uint32_t busy_us, int never, uint32_t normal, uint32_t extended, uint32_t *took)
{
    uint32_t issued;
    now = start; stuck = never;
    issued = T4_CNT;
    ready_at = issued + 24u * busy_us;
    if (setjmp(reset_point)) return -100;
    return fl_wait_done(issued, normal, extended, took);
}

int main(void)
{
    uint32_t took = 0, before;
    int rc;
    fl_late = 0;
    rc = op(1000u, 45000u, 0, 500000u, 2000000u, &took);
    check("a 45 ms erase: 0, not counted late", rc == 0 && fl_late == 0u && took >= 45000u);
    rc = op(1000u, 700000u, 0, 500000u, 2000000u, &took);
    check("a 700 ms erase (past 500 ms): 0, counted late once", rc == 0 && fl_late == 1u && took >= 700000u);
    rc = op(1000u, 1999000u, 0, 500000u, 2000000u, &took);
    check("a 1.999 s erase: 0, counted late", rc == 0 && fl_late == 2u && took < 2000000u);
    before = fl_late;
    rc = op(1000u, 0, 1, 500000u, 2000000u, &took);
    check("a stuck erase: reset at 2 s, never answered, not counted", rc == -100 && fl_late == before &&
          (now - 1000u) / 24u >= 2000000u);
    rc = op(1000u, 2100000u, 0, 500000u, 2000000u, &took);
    check("an erase that would end at 2.1 s: reset all the same", rc == -100 && fl_late == before);
    rc = op(1000u, 2000u, 0, 10000u, 50000u, &took);
    check("a 2 ms page: 0, not counted", rc == 0 && fl_late == before);
    rc = op(1000u, 30000u, 0, 10000u, 50000u, &took);
    check("a 30 ms page (past 10 ms): 0, counted late", rc == 0 && fl_late == before + 1u);
    rc = op(1000u, 0, 1, 10000u, 50000u, &took);
    check("a stuck page: reset at 50 ms", rc == -100 && fl_late == before + 1u);
    rc = op(0xFFFFFFFFu - 24u * 100000u, 600000u, 0, 500000u, 2000000u, &took);
    check("TIMER4 wraps during a late erase: 0, counted", rc == 0 && fl_late == before + 2u);
    printf("%s\n", fails ? "FLASH LATE TEST FAILED" : "flash late test passed");
    return fails != 0;
}
