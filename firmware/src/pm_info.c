/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors (the INFO reply's layout: Felucca's, as ChoralRoot's cr_backup.c
 * sends it) */
/* PurpleMonkey FM-1: "which firmware is this?" over USB-MIDI SysEx. Every Felucca-based firmware reports an
 * FM-1_9xx identity on the update handshake, which does not tell them apart; the installers (tools/fm1_install.py
 * classify_firmware, web/fm1ota.js classifyFirmware) therefore ask the running firmware for its version text with
 * the backup protocol's INFO and refuse to install over one that does not say what it is (docs/INSTALL-COMPAT.md).
 * This is that answer and nothing more of the protocol:
 *   F0 7D 46 4C 01 F7  ->  F0 7D 46 4C 01 "PurpleMonkey <version>" 00  00 x 7  F7
 * (no capability tags after the seven zero counts: PurpleMonkey stores nothing, so there is no backup to read or
 * restore, and the installers skip theirs). Any other frame is left for ota_service (M-UPGRADE).
 * main.c's main loop calls ed_service. Included after ota.c. */
#define PMI_HDR0 0x7Du
#define PMI_HDR1 0x46u
#define PMI_HDR2 0x4Cu
#define PMI_INFO 1u

/* f: the bytes between F0 and F7; out: the reply, F0 .. F7 (<= 40 bytes); returns its length, 0 = not ours */
static uint32_t pmi_reply(const uint8_t *f, uint32_t n, uint8_t *out)
{
    const char *v = FELUCCA_VERSION;
    uint32_t i, k = 0;
    if (n != 4u || f[0] != PMI_HDR0 || f[1] != PMI_HDR1 || f[2] != PMI_HDR2 || f[3] != PMI_INFO)
        return 0;
    out[k++] = 0xF0;
    out[k++] = PMI_HDR0;
    out[k++] = PMI_HDR1;
    out[k++] = PMI_HDR2;
    out[k++] = PMI_INFO;
    for (i = 0; v[i] && i < 24u; i++)
        out[k++] = (uint8_t)(v[i] & 0x7F);
    out[k++] = 0;
    for (i = 0; i < 7u; i++)
        out[k++] = 0;                             /* NENGINES P_COUNT G_COUNT NSTEP P_E0 NTRK CHAIN_ROWS: no editor */
    out[k++] = 0xF7;
    return k;
}

#if FELUCCA_OTA
static void ed_service(void)
{
    static uint8_t out[40];                       /* (static: ota_wire_send may still read it after we return) */
    const uint8_t *p;
    uint32_t n, k;
    if (!ota_frame_get(&p, &n) || n < 4u || p[0] != PMI_HDR0 || p[1] != PMI_HDR1 || p[2] != PMI_HDR2)
        return;
    k = pmi_reply(p, n, out);
    if (k)
        ota_wire_send(out, k);
    ota_frame_done();
}
#else
static void ed_service(void) {}
#endif
