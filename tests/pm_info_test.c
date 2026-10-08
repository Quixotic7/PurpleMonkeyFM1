/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* The installers' "which firmware is this?" (firmware/src/pm_info.c): the reply to the backup protocol's INFO, byte
 * for byte, and silence for everything else. tests/run_pm_tests.sh also feeds the version text to the installer's
 * classifier (tools/fm1_install.py classify_firmware). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FELUCCA_OTA 0
#define FELUCCA_VERSION "PurpleMonkey 0.1-dev"
#include "../firmware/src/pm_info.c"

int main(void)
{
    static const uint8_t ask[4] = {0x7D, 0x46, 0x4C, 1}, list[4] = {0x7D, 0x46, 0x4C, 65}, upg[4] = {0x22, 0x24, 0x35, 0x7D};
    static const uint8_t args[5] = {0x7D, 0x46, 0x4C, 1, 0};
    uint8_t out[64];
    uint32_t n = pmi_reply(ask, 4, out), i, fails = 0;
    const char *v = FELUCCA_VERSION;
    if (n != 5u + strlen(v) + 1u + 7u + 1u || out[0] != 0xF0 || out[1] != 0x7D || out[2] != 0x46 || out[3] != 0x4C ||
        out[4] != 1 || memcmp(out + 5, v, strlen(v)) || out[n - 1u] != 0xF7)
        fails++, printf("  FAIL the INFO reply's frame (%u bytes)\n", (unsigned)n);
    for (i = 5u + (uint32_t)strlen(v); i + 1u < n; i++)
        if (out[i])
            fails++, printf("  FAIL byte %u after the text is %u, not 0\n", (unsigned)i, out[i]);
    for (i = 1; i + 1u < n; i++)
        if (out[i] & 0x80)
            fails++, printf("  FAIL a data byte above 7 bits\n");
    if (pmi_reply(list, 4, out) || pmi_reply(upg, 4, out) || pmi_reply(args, 5, out) || pmi_reply(ask, 3, out))
        fails++, printf("  FAIL an answer to something that is not INFO\n");
    ed_service();
    printf("%s: the INFO reply (\"%s\")\n", fails ? "FAIL" : "PASS", v);
    return fails != 0;
}
