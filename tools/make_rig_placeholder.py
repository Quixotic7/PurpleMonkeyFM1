#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""A stand-in rig, drawn from ellipses: assets/purplemonkey/rig/placeholder/ (parts as PNGs and rig.json).

  tools/make_rig_placeholder.py

NOT artwork. It exists so the rig renderer, the animation player and their cost can be built and measured before
the pets are drawn as parts (docs/RIG-ART-SPEC.md says what that art must look like). It is a purple monkey-shaped
doll of eight parts with the same joints a real pet would have, and three animations. The firmware shows it only
when asked to (the emulator's PM_RIG_DEMO=1); the pets themselves stay the cut-outs until real parts exist.
"""
import json
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "assets/purplemonkey/rig/placeholder"
K = 4                                  # drawn 4x, scaled down: soft edges before the alpha threshold
PURPLE, DARK, CREAM, INK, RED = (150, 92, 210), (104, 58, 160), (255, 236, 200), (30, 20, 44), (232, 70, 60)


def part(w, h, draw):
    im = Image.new("RGBA", (w * K, h * K), (0, 0, 0, 0))
    draw(ImageDraw.Draw(im), K)
    return im.resize((w, h), Image.LANCZOS)


def limb(w, h, hand):
    def d(g, k):
        g.rounded_rectangle((k, k, (w - 1) * k, (h - 1) * k), radius=w * k // 2, fill=PURPLE, outline=INK, width=k)
        g.ellipse((k, (h - w) * k, (w - 1) * k, (h - 1) * k), fill=hand, outline=INK, width=k)
    return part(w, h, d)


def body(g, k):
    g.ellipse((2 * k, 1 * k, 30 * k, 37 * k), fill=PURPLE, outline=INK, width=k)
    g.ellipse((9 * k, 12 * k, 23 * k, 32 * k), fill=CREAM)


def head(g, k):
    g.ellipse((0, 10 * k, 12 * k, 22 * k), fill=DARK, outline=INK, width=k)             # ears
    g.ellipse((28 * k, 10 * k, 40 * k, 22 * k), fill=DARK, outline=INK, width=k)
    g.ellipse((5 * k, 2 * k, 35 * k, 32 * k), fill=PURPLE, outline=INK, width=k)
    g.ellipse((10 * k, 12 * k, 30 * k, 31 * k), fill=CREAM)
    g.ellipse((14 * k, 13 * k, 18 * k, 19 * k), fill=INK)                               # eyes
    g.ellipse((22 * k, 13 * k, 26 * k, 19 * k), fill=INK)
    g.chord((14 * k, 18 * k, 26 * k, 28 * k), 10, 170, fill=RED, outline=INK, width=k)  # a smile


def tail(g, k):
    g.arc((2 * k, 2 * k, 26 * k, 26 * k), 150, 400, fill=INK, width=6 * k)
    g.arc((2 * k, 2 * k, 26 * k, 26 * k), 152, 398, fill=PURPLE, width=4 * k)


PARTS = {   # name: (image, pivot in the part, parent, where the pivot sits in the parent)
    "body": (part(32, 38, body), (16, 34), None, None),
    "tail": (part(28, 28, tail), (5, 22), "body", (26, 30)),
    "leg_l": (limb(10, 22, CREAM), (5, 3), "body", (11, 33)),
    "leg_r": (limb(10, 22, CREAM), (5, 3), "body", (21, 33)),
    "arm_l": (limb(9, 24, CREAM), (4, 3), "body", (5, 11)),
    "head": (part(40, 34, head), (20, 31), "body", (16, 5)),
    "arm_r": (limb(9, 24, CREAM), (4, 3), "body", (27, 11)),
}
ORDER = ["tail", "leg_l", "leg_r", "arm_l", "body", "head", "arm_r"]     # back to front


def key(ms, root=(0, 0), **angles):
    return {"ms": ms, "root": list(root), "a": angles}


RIG = {
    "note": "placeholder rig: tools/make_rig_placeholder.py. Angles in degrees, clockwise on the screen, relative to the parent.",
    "feet": 20,            # px from the root part's pivot down to the ground
    "parts": [{"name": n, "file": f"{n}.png", "pivot": list(PARTS[n][1]), "parent": PARTS[n][2],
               "at": list(PARTS[n][3]) if PARTS[n][3] else None} for n in PARTS],
    "order": ORDER,
    "anims": {
        "idle": [key(1400, head=-4, arm_l=6, arm_r=-6, tail=-8), key(1400, (0, 1), head=4, arm_l=10, arm_r=-10, tail=10),
                 key(250, head=6, arm_r=-150, tail=0), key(200, head=6, arm_r=-120), key(200, head=6, arm_r=-155),
                 key(200, head=4, arm_r=-120), key(400, arm_r=-150)],
        "play": [key(160, (0, 2), body=-5, head=6, arm_l=70, arm_r=-25, leg_l=8, leg_r=-4, tail=20),
                 key(160, (0, -2), body=0, head=-3, arm_l=20, arm_r=-80, leg_l=0, leg_r=0, tail=-10),
                 key(160, (0, 2), body=5, head=-6, arm_l=25, arm_r=-70, leg_l=4, leg_r=-8, tail=-20),
                 key(160, (0, -2), body=0, head=3, arm_l=80, arm_r=-20, leg_l=0, leg_r=0, tail=10)],
        "dance": [key(140, (-3, 0), body=-10, head=-8, arm_l=160, arm_r=-150, leg_l=25, leg_r=-5, tail=30),
                  key(140, (0, -6), body=0, head=0, arm_l=130, arm_r=-130, leg_l=-10, leg_r=10, tail=0),
                  key(140, (3, 0), body=10, head=8, arm_l=150, arm_r=-160, leg_l=5, leg_r=-25, tail=-30),
                  key(140, (0, -6), body=0, head=0, arm_l=130, arm_r=-130, leg_l=-10, leg_r=10, tail=0)],
    },
}

if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for n, (im, *_rest) in PARTS.items():
        im.save(OUT / f"{n}.png")
    (OUT / "rig.json").write_text(json.dumps(RIG, indent=1) + "\n")
    print(f"placeholder rig: {len(PARTS)} parts -> {OUT}")
