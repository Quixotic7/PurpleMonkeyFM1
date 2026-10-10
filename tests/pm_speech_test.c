/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 PurpleMonkey FM-1 contributors */
/* the voice on a host: every word rendered, bounded, ending in silence; a word cut by another leaves no step; any
 * bits at all are a word that ends. All the words go to a WAV (argv[1]) for listening: no test here hears whether
 * a word can be understood */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/pm_speech.c"
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void put32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc(v >> 24, f); }
int main(int argc, char **argv)
{
    static int16_t wav[PM_SPEECH_FS * 40];
    uint32_t n = 0, w;
    pm_speech_init();
    for (w = 0; w < PM_NWORD; w++) {
        int32_t buf[128], peak = 0, tail = 0;
        uint32_t start = n, len = 0, blocks = 0, i;
        pm_speech_say(w);
        CHECK(pm_speech_busy(), "%s: not busy after say", PM_WORD_NAME[w]);
        CHECK(w >= 26u ? pm_speech_letter(w) == 0 : pm_speech_letter(w) == (char)('A' + w), "%s: letter", PM_WORD_NAME[w]);
        while (pm_speech_busy() && blocks < 2000u) {
            memset(buf, 0, sizeof buf);
            pm_speech_render(buf, 128);
            for (i = 0; i < 128u; i++) {
                int32_t a = buf[i] < 0 ? -buf[i] : buf[i];
                if (a > peak) peak = a;
                wav[n++] = (int16_t)(buf[i] > 32767 ? 32767 : buf[i] < -32768 ? -32768 : buf[i]);
            }
            blocks++;
        }
        len = n - start;
        for (i = 0; i < 64u && len > 64u; i++) {
            int32_t a = wav[n - 1 - i] < 0 ? -wav[n - 1 - i] : wav[n - 1 - i];
            if (a > tail) tail = a;
        }
        printf("  %-7s %5u ms  peak %5d  tail %3d\n", PM_WORD_NAME[w], len * 1000u / PM_SPEECH_FS, peak, tail);
        CHECK(!pm_speech_busy(), "%s: still going after 2000 blocks", PM_WORD_NAME[w]);
        CHECK(peak >= 3000 && peak <= 30000, "%s: peak %d", PM_WORD_NAME[w], peak);
        CHECK(tail < 400, "%s: ends with a step of %d", PM_WORD_NAME[w], tail);
        CHECK(len >= PM_SPEECH_FS / 5u && len <= PM_SPEECH_FS * 2u, "%s: %u samples", PM_WORD_NAME[w], len);
        for (i = 0; i < PM_SPEECH_FS / 4u; i++)
            wav[n++] = 0;                        /* a quarter second between words */
    }
    {   /* a word cut by another, wherever in it: no step at the cut (the cut word dies away under the new one) */
        int32_t buf[128], last, worst = 0; uint32_t i, at;
        for (at = 1; at < 60u; at++) {
            pm_speech_say(PM_W_MONKEY);
            for (i = 0; i < at; i++) { memset(buf, 0, sizeof buf); pm_speech_render(buf, 128); }
            last = buf[127];
            pm_speech_say(PM_W_A + at % 26u);
            memset(buf, 0, sizeof buf); pm_speech_render(buf, 128);
            if (abs(buf[0] - last) > worst) worst = abs(buf[0] - last);
            while (pm_speech_busy()) { memset(buf, 0, sizeof buf); pm_speech_render(buf, 128); }
        }
        printf("  a word cut by another: the largest step at the cut %d\n", worst);
        CHECK(worst < 3000, "a cut word left a step of %d", worst);
    }
    {   /* any byte stream is a word that ends, bounded (the lattice is stable for every frame there can be) */
        uint32_t seed = 1u, i, k, blocks; int32_t buf[128], peak = 0;
        for (k = 0; k < 200u; k++) {
            pm_speech_say(k % PM_NWORD);
            seed = seed * 1664525u + 1013904223u;
            spk.bit = (seed >> 8) % (uint32_t)(sizeof PM_LPC_DATA * 8u);        /* (from anywhere, not a frame's start) */
            for (blocks = 0; pm_speech_busy() && blocks < 40000u; blocks++) {
                memset(buf, 0, sizeof buf); pm_speech_render(buf, 128);
                for (i = 0; i < 128u; i++) if (abs(buf[i]) > peak) peak = abs(buf[i]);
            }
            CHECK(!pm_speech_busy(), "a stream from bit %u never ended", (seed >> 8) % (uint32_t)(sizeof PM_LPC_DATA * 8u));
        }
        CHECK(peak <= PM_SPEECH_CEIL, "a stream from anywhere peaked at %d", peak);
    }
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        if (f) {
            fwrite("RIFF", 1, 4, f); put32(f, 36 + n * 2); fwrite("WAVEfmt ", 1, 8, f); put32(f, 16);
            fputc(1, f); fputc(0, f); fputc(1, f); fputc(0, f); put32(f, PM_SPEECH_FS); put32(f, PM_SPEECH_FS * 2);
            fputc(2, f); fputc(0, f); fputc(16, f); fputc(0, f); fwrite("data", 1, 4, f); put32(f, n * 2);
            fwrite(wav, 2, n, f); fclose(f);
            printf("  wrote %s (%u s)\n", argv[1], n / PM_SPEECH_FS);
        }
    }
    printf(fails ? "FAIL: %d\n" : "PASS: the voice (%d words)\n", fails ? fails : (int)PM_NWORD);
    return fails != 0;
}
