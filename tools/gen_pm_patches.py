#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""PurpleMonkey's FM6 sounds: a bank of PM_NPRESET voices (firmware/src/pm_sound.c) -> a C header.

  tools/gen_pm_patches.py OUT.h

The bank is four groups of four, one group a pet, the pet's own sound first in its group (pm_engine.h
pm_preset_home: pet * 4): SOUND (the PRESETS knob) steps through all sixteen, HOME and a pet change go to the
pet's own (the environment is WORLD's, the ALGORITHM knob: pm_ui.c). Each preset carries
  a voice   written here as operator settings with gen_fm6_patches.py's helpers (no factory ROM data of any
            instrument), voiced for small hands and a small speaker: an instant attack that says "you pressed a
            key", a mellow body, nothing harsh when eight sound at once;
  a morph   what TONE (KNOB 1 in SYNTH, -8 .. 8) does to it: per detent, how far each of FM6's own macros
            moves (fb feedback, mlvl modulator level = brightness, mrat modulator ratio, meg modulator envelope
            rate, vmod velocity sensitivity, dtun carrier detune spread) and len, the envelopes' decay and
            release (pm_sound.c). Bounded: at the knob's ends the sound is still the sound;
            and an accent colour the sky drifts towards.
Algorithm 5 = three pairs (1<-2, 3<-4, 5<-6 with the feedback); 32 = six carriers.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from gen_fm6_patches import op, voice, pack  # noqa: E402

PETS = ["MONKEY", "CAT", "DOG", "LLAMA"]
MORPH_KEYS = ["fb", "mlvl", "mrat", "meg", "vmod", "dtun", "len"]


def morph(**w):
    """per detent of MORPH: how far each macro moves (fb 1/8 steps, mlvl 1, mrat 1/8, meg 1, vmod 1/8, dtun 1, len 1)"""
    return [int(w.get(k, 0)) for k in MORPH_KEYS]


# pleasant plucks and bells share these envelope shapes (ms-ish rates on the DX7 scale)
BANK = [
    # ---- MONKEY: wood and thumbs ----
    ("KALIMBA", "a round thumb-piano pluck with a little bass in it",
     voice("PM KALIMBA", 5, [
         op(r=(99, 46, 32, 38), l=(99, 62, 0, 0), ol=99, kvs=2, rs=2),
         op(r=(99, 68, 44, 50), l=(99, 34, 0, 0), ol=66, fc=1, kvs=4, rs=3),
         op(r=(99, 54, 36, 40), l=(99, 40, 0, 0), ol=88, fc=2, kvs=2, rs=3, det=8),
         op(r=(99, 80, 60, 60), l=(99, 0, 0, 0), ol=58, fc=7, kvs=5, rs=4),
         op(r=(99, 40, 30, 36), l=(99, 70, 0, 0), ol=86, fc=0, kvs=1, rs=1),
         op(r=(99, 60, 40, 46), l=(99, 30, 0, 0), ol=48, fc=1, kvs=3)],
         fb=2),
     morph(mlvl=3, len=1)),   # soft and short .. bright and ringing
    ("MARIMBA", "a hollow wooden bar, warm and quick",
     voice("PM MARIMBA", 5, [
         op(r=(99, 50, 34, 42), l=(99, 58, 0, 0), ol=99, kvs=2, rs=3),
         op(r=(99, 82, 60, 60), l=(99, 20, 0, 0), ol=72, fc=4, kvs=4, rs=4),
         op(r=(99, 56, 38, 44), l=(99, 44, 0, 0), ol=86, fc=1, kvs=2, rs=3, det=6),
         op(r=(99, 90, 70, 70), l=(99, 0, 0, 0), ol=52, fc=10, kvs=5, rs=5),
         op(r=(99, 44, 32, 40), l=(99, 60, 0, 0), ol=80, fc=0, kvs=1, rs=2),
         op(r=(99, 70, 50, 50), l=(99, 24, 0, 0), ol=40, fc=3, kvs=3)],
         fb=1),
     morph(mlvl=2, meg=-2, len=1)),   # mellow .. hard mallets
    ("TOYPIANO", "a tiny upright with tines, bright and a little tinny",
     voice("PM TOYPNO", 5, [
         op(r=(99, 42, 28, 40), l=(99, 66, 0, 0), ol=99, kvs=2, rs=3),
         op(r=(99, 60, 40, 48), l=(99, 48, 0, 0), ol=74, fc=3, ff=2, kvs=4, rs=3),
         op(r=(99, 46, 30, 42), l=(99, 56, 0, 0), ol=90, fc=1, kvs=2, rs=3, det=9),
         op(r=(99, 70, 50, 54), l=(99, 20, 0, 0), ol=62, fc=6, ff=5, kvs=5, rs=4),
         op(r=(99, 40, 28, 38), l=(99, 64, 0, 0), ol=78, fc=2, kvs=1, rs=2, det=5),
         op(r=(99, 64, 44, 50), l=(99, 30, 0, 0), ol=46, fc=5, kvs=3)],
         fb=3),
     morph(mlvl=2, dtun=3)),   # clean .. honky-tonk
    ("STEELPAN", "a steel pan: a bright ping on a warm hum",
     voice("PM STEEL", 5, [
         op(r=(99, 44, 30, 40), l=(99, 70, 0, 0), ol=99, kvs=2, rs=2),
         op(r=(99, 70, 48, 52), l=(99, 40, 0, 0), ol=68, fc=2, kvs=4, rs=3),
         op(r=(99, 48, 32, 42), l=(99, 60, 0, 0), ol=88, fc=1, kvs=2, rs=2, det=10),
         op(r=(99, 76, 56, 58), l=(99, 24, 0, 0), ol=60, fc=5, ff=3, kvs=5, rs=4),
         op(r=(99, 42, 30, 40), l=(99, 66, 0, 0), ol=82, fc=2, kvs=1, rs=2, det=4),
         op(r=(99, 66, 46, 50), l=(99, 34, 0, 0), ol=50, fc=3, kvs=3)],
         fb=2),
     morph(mlvl=2, mrat=1, len=1)),   # soft rubber .. ringing metal

    # ---- CAT: bars and bells ----
    ("XYLO", "its xylophone: a wooden tock on a short warm note",
     voice("PM XYLO", 5, [
         op(r=(99, 48, 35, 40), l=(99, 64, 0, 0), ol=99, kvs=2, rs=3),
         op(r=(99, 72, 52, 56), l=(99, 26, 0, 0), ol=70, fc=4, kvs=4, rs=3),
         op(r=(99, 52, 37, 42), l=(99, 50, 0, 0), ol=90, kvs=2, rs=3, det=8),
         op(r=(99, 78, 58, 60), l=(99, 0, 0, 0), ol=60, fc=10, kvs=5, rs=4),
         op(r=(99, 42, 32, 38), l=(99, 60, 0, 0), ol=80, fc=0, kvs=1, rs=2),
         op(r=(99, 86, 70, 70), l=(99, 0, 0, 0), ol=44, fc=3, kvs=3)],
         fb=1),
     morph(mlvl=3, len=1)),
    ("GLOCK", "a glockenspiel: small bright bells",
     voice("PM GLOCK", 5, [
         op(r=(99, 40, 26, 38), l=(99, 74, 0, 0), ol=97, kvs=2, rs=2),
         op(r=(99, 54, 36, 44), l=(99, 52, 0, 0), ol=64, fc=4, kvs=3, rs=3),
         op(r=(99, 44, 28, 40), l=(99, 64, 0, 0), ol=84, fc=2, kvs=2, rs=2, det=8),
         op(r=(99, 64, 44, 50), l=(99, 36, 0, 0), ol=56, fc=7, ff=2, kvs=4, rs=3),
         op(r=(99, 48, 32, 40), l=(99, 50, 0, 0), ol=74, fc=3, kvs=2, rs=3, det=5),
         op(r=(99, 70, 50, 54), l=(99, 10, 0, 0), ol=42, fc=11, kvs=3)],
         fb=0),
     morph(mlvl=2, mrat=1)),   # glassy .. metallic
    ("TOYORGAN", "a little reed organ that holds while the key does",
     voice("PM TOYORG", 32, [
         op(r=(96, 60, 40, 60), l=(99, 96, 94, 0), ol=92, kvs=1, rs=1),
         op(r=(94, 60, 40, 60), l=(99, 94, 92, 0), ol=78, fc=2, kvs=1, rs=1, det=9),
         op(r=(92, 60, 40, 60), l=(99, 92, 90, 0), ol=70, fc=3, kvs=1, rs=1),
         op(r=(90, 60, 40, 60), l=(99, 90, 88, 0), ol=58, fc=4, kvs=1, rs=2, det=5),
         op(r=(88, 60, 40, 60), l=(99, 88, 86, 0), ol=48, fc=6, kvs=1, rs=2),
         op(r=(86, 60, 40, 60), l=(99, 86, 84, 0), ol=40, fc=8, kvs=1, rs=2)],
         fb=0, lfs=28, lfd=0, lamd=10, lfw=0, lpms=1),
     morph(dtun=6, vmod=0)),   # straight .. wobbly and wide (six carriers: DTUN spreads them)
    ("HARP", "a small harp: a soft pluck with a long shimmer",
     voice("PM HARP", 5, [
         op(r=(99, 36, 24, 36), l=(99, 70, 0, 0), ol=99, kvs=2, rs=2),
         op(r=(99, 58, 40, 46), l=(99, 40, 0, 0), ol=60, fc=2, kvs=3, rs=3),
         op(r=(99, 40, 26, 38), l=(99, 62, 0, 0), ol=86, fc=1, kvs=2, rs=2, det=9),
         op(r=(99, 70, 50, 54), l=(99, 16, 0, 0), ol=54, fc=5, kvs=4, rs=4),
         op(r=(99, 38, 26, 38), l=(99, 66, 0, 0), ol=80, fc=3, kvs=1, rs=2, det=5),
         op(r=(99, 62, 44, 50), l=(99, 24, 0, 0), ol=44, fc=4, kvs=3)],
         fb=1),
     morph(mlvl=2, dtun=2, len=1)),   # a lute .. a shimmering harp

    # ---- DOG: boxes and bells ----
    ("BELLBOX", "a music box: a soft bell that rings on a little",
     voice("PM BELLBOX", 5, [
         op(r=(99, 40, 28, 36), l=(99, 72, 0, 0), ol=98, kvs=2, rs=2),
         op(r=(99, 50, 34, 40), l=(99, 50, 0, 0), ol=62, fc=3, ff=17, kvs=3, rs=2),
         op(r=(99, 44, 30, 38), l=(99, 60, 0, 0), ol=86, fc=2, kvs=2, rs=2, det=8),
         op(r=(99, 60, 40, 46), l=(99, 30, 0, 0), ol=54, fc=7, kvs=4, rs=3),
         op(r=(99, 50, 34, 40), l=(99, 40, 0, 0), ol=78, fc=4, det=6, kvs=2, rs=3),
         op(r=(99, 70, 50, 60), l=(99, 0, 0, 0), ol=46, fc=9, kvs=3)],
         fb=0),
     morph(mlvl=2, len=1)),
    ("CHIMES", "wind chimes: long bright bells, far apart",
     voice("PM CHIMES", 5, [
         op(r=(99, 30, 20, 32), l=(99, 80, 0, 0), ol=96, kvs=2, rs=1),
         op(r=(99, 44, 30, 40), l=(99, 56, 0, 0), ol=58, fc=3, ff=14, kvs=3, rs=2),
         op(r=(99, 32, 22, 34), l=(99, 76, 0, 0), ol=84, fc=2, kvs=2, rs=1, det=10),
         op(r=(99, 50, 36, 44), l=(99, 40, 0, 0), ol=52, fc=5, ff=9, kvs=4, rs=3),
         op(r=(99, 36, 24, 36), l=(99, 70, 0, 0), ol=76, fc=4, det=4, kvs=2, rs=2),
         op(r=(99, 60, 44, 50), l=(99, 20, 0, 0), ol=44, fc=11, kvs=3)],
         fb=0),
     morph(mlvl=2, mrat=1, len=2)),   # soft tubes .. glass, and how long they ring
    ("VIBES", "vibraphone: a round bell with a slow wobble",
     voice("PM VIBES", 5, [
         op(r=(99, 38, 26, 36), l=(99, 70, 0, 0), ol=98, kvs=2, rs=2),
         op(r=(99, 56, 38, 44), l=(99, 40, 0, 0), ol=56, fc=4, kvs=3, rs=3),
         op(r=(99, 40, 28, 38), l=(99, 64, 0, 0), ol=84, fc=1, kvs=2, rs=2, det=9),
         op(r=(99, 66, 46, 50), l=(99, 28, 0, 0), ol=50, fc=6, kvs=4, rs=3),
         op(r=(99, 42, 30, 38), l=(99, 60, 0, 0), ol=76, fc=2, kvs=2, rs=2, det=5),
         op(r=(99, 70, 50, 54), l=(99, 10, 0, 0), ol=40, fc=8, kvs=3)],
         fb=0, lfs=40, lfd=20, lamd=24, lfw=0, lpms=1),
     morph(mlvl=2, meg=2, len=1)),   # dull mallets .. bright, with a slower bloom
    ("PUDDING", "a round rubbery bass that wobbles when pressed",
     voice("PM PUDDNG", 5, [
         op(r=(99, 50, 36, 44), l=(99, 70, 0, 0), ol=99, kvs=1, rs=1),
         op(r=(99, 66, 46, 52), l=(99, 36, 0, 0), ol=70, fc=1, kvs=3, rs=2),
         op(r=(99, 52, 36, 44), l=(99, 64, 0, 0), ol=90, fc=0, ff=50, kvs=1, rs=1, det=8),
         op(r=(99, 74, 54, 56), l=(99, 20, 0, 0), ol=62, fc=2, kvs=4, rs=3),
         op(r=(99, 46, 32, 42), l=(99, 68, 0, 0), ol=84, fc=0, kvs=1, rs=1),
         op(r=(99, 64, 44, 50), l=(99, 30, 0, 0), ol=52, fc=1, kvs=3)],
         fb=3, lfs=50, lfd=0, lpmd=4, lfw=0, lpms=1),
     morph(fb=4, mlvl=2)),   # soft .. growly (the feedback)

    # ---- LLAMA: breath and voices ----
    ("PIPES", "a breathy pipe that holds for as long as the key does",
     voice("PM PIPES", 5, [
         op(r=(72, 30, 24, 46), l=(99, 92, 88, 0), ol=98, kvs=1),
         op(r=(80, 50, 30, 50), l=(99, 60, 44, 0), ol=52, fc=1, kvs=2),
         op(r=(66, 30, 24, 44), l=(99, 90, 84, 0), ol=86, fc=2, kvs=1, det=8),
         op(r=(90, 60, 30, 50), l=(99, 40, 30, 0), ol=44, fc=2, kvs=2),
         op(r=(60, 28, 22, 42), l=(99, 94, 90, 0), ol=88, fc=1, det=6, kvs=1),
         op(r=(99, 70, 40, 50), l=(99, 30, 20, 0), ol=50, fc=3, kvs=3)],
         fb=2, lfs=30, lfd=40, lpmd=6, lfw=4, lpms=2),
     morph(mlvl=2, dtun=3)),   # hollow .. reedy and wide
    ("SOFTPAD", "a slow warm pad that swells in and lingers",
     voice("PM PAD", 32, [
         op(r=(44, 30, 20, 40), l=(99, 90, 86, 0), ol=90, kvs=1, rs=1),
         op(r=(40, 30, 20, 40), l=(99, 88, 84, 0), ol=84, fc=1, kvs=1, rs=1, det=11),
         op(r=(46, 30, 20, 40), l=(99, 90, 86, 0), ol=80, fc=2, kvs=1, rs=1, det=3),
         op(r=(42, 30, 20, 40), l=(99, 86, 82, 0), ol=72, fc=2, kvs=1, rs=2, det=12),
         op(r=(38, 30, 20, 40), l=(99, 84, 80, 0), ol=60, fc=3, kvs=1, rs=2, det=5),
         op(r=(48, 30, 20, 40), l=(99, 80, 76, 0), ol=50, fc=4, kvs=1, rs=2, det=9)],
         fb=0, lfs=22, lfd=30, lamd=14, lfw=0, lpms=1),
     morph(dtun=8, len=1)),   # still .. a wide shimmering wash
    ("FLUTE", "a wooden flute with a little breath at the start",
     voice("PM FLUTE", 5, [
         op(r=(78, 40, 28, 52), l=(99, 94, 90, 0), ol=98, kvs=1, rs=1),
         op(r=(99, 70, 40, 60), l=(99, 30, 24, 0), ol=48, fc=1, kvs=3),
         op(r=(74, 40, 28, 50), l=(99, 92, 88, 0), ol=82, fc=2, kvs=1, det=7),
         op(r=(99, 80, 50, 60), l=(99, 20, 10, 0), ol=56, fc=3, kvs=3),
         op(r=(70, 36, 26, 48), l=(99, 94, 90, 0), ol=70, fc=1, det=5, kvs=1),
         op(r=(99, 90, 60, 70), l=(99, 10, 0, 0), ol=60, fc=11, kvs=4)],
         fb=4, lfs=34, lfd=50, lpmd=5, lfw=0, lpms=2),
     morph(mlvl=2, meg=-2)),   # pure .. breathy with more chiff
    ("AHH", "a small choir of ahhs, soft and slow",
     voice("PM AHH", 32, [
         op(r=(50, 34, 24, 44), l=(99, 92, 88, 0), ol=92, kvs=1, rs=1),
         op(r=(48, 34, 24, 44), l=(99, 90, 86, 0), ol=86, fc=1, kvs=1, rs=1, det=10),
         op(r=(52, 34, 24, 44), l=(99, 90, 86, 0), ol=78, fc=2, kvs=1, rs=1, det=4),
         op(r=(46, 34, 24, 44), l=(99, 88, 84, 0), ol=66, fc=3, kvs=1, rs=2, det=12),
         op(r=(50, 34, 24, 44), l=(99, 86, 82, 0), ol=56, fc=4, kvs=1, rs=2, det=6),
         op(r=(54, 34, 24, 44), l=(99, 80, 76, 0), ol=44, fc=5, kvs=1, rs=2, det=8)],
         fb=0, lfs=26, lfd=20, lamd=8, lpmd=3, lfw=0, lpms=1),
     morph(dtun=6, len=1)),   # one voice .. a crowd
]


def main(path):
    assert len(BANK) == 16, "the bank is four groups of four (pm_engine.h PM_NPRESET)"
    homes = ["KALIMBA", "XYLO", "BELLBOX", "PIPES"]           # each pet's own sound: first in its group
    for i, h in enumerate(homes):
        assert BANK[i * 4][0] == h, f"{PETS[i]}'s own sound must be preset {i * 4}"
    for name, _, _, m in BANK:
        assert len(name) <= 8 and name.isupper(), name     # (the gauge shows it)
    L = ["/* generated by tools/gen_pm_patches.py: PurpleMonkey's FM6 voices (packed), PM_NPRESET of them, four a pet",
         " * (the pet's own first: pm_preset_home), each with its TONE recipe */",
         "#pragma once",
         f"static const uint8_t PM_FM6[{len(BANK)}][128] = {{"]
    for name, _, v, _ in BANK:
        L.append("    {" + ", ".join(str(x) for x in pack(v)) + f"}},   /* {name} */")
    L.append("};")
    L.append("static const char *const PM_PRESET_NAME[" + str(len(BANK)) + "] = {" +
             ", ".join(f'"{n}"' for n, *_ in BANK) + "};")
    L.append("/* TONE per detent: " + ", ".join(MORPH_KEYS) + " (pm_sound.c pm_sound_apply) */")
    L.append(f"static const int8_t PM_MORPH[{len(BANK)}][{len(MORPH_KEYS)}] = {{")
    for name, _, _, m in BANK:
        L.append("    {" + ", ".join(str(x) for x in m) + f"}},   /* {name} */")
    L.append("};")
    Path(path).write_text("\n".join(L) + "\n")
    print(f"pm patches: {len(BANK)} -> {path}")


if __name__ == "__main__":
    main(sys.argv[1])
