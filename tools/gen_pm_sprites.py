#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""PurpleMonkey's pets for the 240x240 screen (firmware/src/pm_scene.c) -> pm_sprites.h

  tools/gen_pm_sprites.py OUT.h [--sheet OUT.png]

Source: assets/purplemonkey/concept/band-friends-midnight-v3.png, the approved character sheet (four animals x
HELLO / PLAY / TOGETHER on a dark ground; generated concept art, see the prompt files next to it). The sheet is not
a sprite atlas, so each pose is cut out of it here:
  1. crop the pose (BOXES, sheet pixels);
  2. the ground: every pixel within BG_TOL of the sheet's dark blue that a flood fill reaches from the crop's edge
     (the cat's black fur is not that blue, and is enclosed by its outline, so it stays);
  3. keep the animal: the largest connected piece (a few pixels of slack join whiskers and tufts to it); the loose
     notes, sparks and motion lines of the sheet are dropped (the firmware draws its own, from the music);
  4. scale to fit BOX px, edges blended onto the scene's night colour; quantise each animal's three poses to one
     palette of NCOL colours without dithering (flat areas, short runs);
  5. pack: per row, spans of (skip, count, count palette indices); a row table for clipped drawing.
The pet names are drawn here too (Inter Tight, heavy, cream with a dark outline): the announcement's title.
--sheet writes a contact sheet of what the firmware gets, for review (docs/img/pm_sprites.png).
"""
import sys
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets/purplemonkey/concept/band-friends-midnight-v3.png"
FONT = ROOT / "assets/fonts/InterTight[wght].ttf"
BG = (20, 16, 38)                 # the sheet's ground
BG_TOL = 20
NIGHT = (22, 18, 46)              # what edge pixels are blended onto (the scene's sky, about)
BOX = 120                         # a pose fits BOX x BOX
NCOL = 63                         # + index 0: clear (an index is a byte: more colours cost only the palette)
PETS = ["MONKEY", "CAT", "DOG", "LLAMA"]
POSES = ["HELLO", "PLAY", "TOGETHER"]
K = 1.254                         # the boxes were read off a 1000 px view of the 1254 px sheet
BOXES = {
    "MONKEY": [(150, 136, 392, 345), (430, 136, 678, 345), (735, 128, 992, 350)],
    "CAT": [(140, 340, 420, 578), (440, 340, 700, 578), (720, 340, 992, 572)],
    "DOG": [(150, 572, 362, 778), (430, 572, 652, 778), (735, 572, 992, 762)],
    "LLAMA": [(180, 745, 362, 996), (430, 760, 682, 996), (760, 748, 992, 996)],
}
SLACK = {"MONKEY": 5, "CAT": 7, "DOG": 1, "LLAMA": 1}   # MaxFilter size joining loose strokes to the animal


def cut(sheet, box, slack=7):
    """the pose of a box as RGBA at sheet scale: the ground and the loose decorations clear"""
    im = sheet.crop(tuple(int(round(v * K)) for v in box))
    w, h = im.size
    px = im.load()
    near = [[sum((px[x, y][i] - BG[i]) ** 2 for i in range(3)) <= BG_TOL ** 2 for x in range(w)] for y in range(h)]
    ground = [[False] * w for _ in range(h)]
    q = deque()
    for x in range(w):
        q.append((x, 0)), q.append((x, h - 1))
    for y in range(h):
        q.append((0, y)), q.append((w - 1, y))
    while q:
        x, y = q.popleft()
        if not (0 <= x < w and 0 <= y < h) or ground[y][x] or not near[y][x]:
            continue
        ground[y][x] = True
        q.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    solid = Image.new("L", (w, h))
    solid.putdata([0 if ground[y][x] else 255 for y in range(h) for x in range(w)])
    fat = solid.filter(ImageFilter.MaxFilter(slack)).load()   # a few px of slack: whiskers, tufts, fingers
    label = [[0] * w for _ in range(h)]
    sizes = [0]
    for y0 in range(h):
        for x0 in range(w):
            if not fat[x0, y0] or label[y0][x0]:
                continue
            n = len(sizes)
            sizes.append(0)
            q.append((x0, y0))
            while q:
                x, y = q.popleft()
                if not (0 <= x < w and 0 <= y < h) or label[y][x] or not fat[x, y]:
                    continue
                label[y][x] = n
                sizes[n] += 1
                q.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    big = max(range(1, len(sizes)), key=lambda i: sizes[i])
    out = im.convert("RGBA")
    out.putalpha(Image.new("L", (w, h)))
    op = out.load()
    for y in range(h):
        for x in range(w):
            if label[y][x] == big and not ground[y][x]:
                op[x, y] = px[x, y] + (255,)
    return out.crop(out.getbbox())


def fit(im, box=BOX):
    s = min(box / im.width, box / im.height)
    im = im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)
    return im.crop(im.getbbox())


def title(text):
    """a pet's name: heavy cream letters with a dark outline, <= 200 px wide"""
    font = ImageFont.truetype(str(FONT), 300)
    try:
        font.set_variation_by_axes([900])
    except Exception:
        pass
    im = Image.new("RGBA", (1600, 420), (0, 0, 0, 0))
    ImageDraw.Draw(im).text((60, 30), text, font=font, fill=(255, 244, 214, 255), stroke_width=22,
                            stroke_fill=(34, 22, 60, 255))
    im = im.crop(im.getbbox())
    s = min(200 / im.width, 64 / im.height)
    return im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS)


def flatten(ims):
    """RGBA images -> (palette [(r, g, b)], [index rows]) with one palette for all; 0 = clear"""
    strip = Image.new("RGB", (sum(i.width for i in ims), max(i.height for i in ims)), NIGHT)
    x = 0
    solid = []
    for im in ims:
        over = Image.new("RGB", im.size, NIGHT)
        over.paste(im, (0, 0), im)
        strip.paste(over, (x, 0))
        solid.append(im.getchannel("A").point(lambda a: 255 if a >= 128 else 0))
        x += im.width
    q = strip.quantize(NCOL, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = q.getpalette()[:NCOL * 3]
    pal = [tuple(pal[i * 3:i * 3 + 3]) for i in range(NCOL)]
    out, x = [], 0
    for im, a in zip(ims, solid):
        qp, ap = q.load(), a.load()
        out.append([[qp[x + i, j] + 1 if ap[i, j] else 0 for i in range(im.width)] for j in range(im.height)])
        x += im.width
    return pal, out


def pack(rows):
    """rows of indices -> (bytes, row offsets): spans of skip, count, count indices; skip 255 ends a row"""
    data, offs = bytearray(), []
    for r in rows:
        offs.append(len(data))
        x, w = 0, len(r)
        while x < w:
            s = x
            while x < w and r[x] == 0:
                x += 1
            if x == w:
                break
            skip = x - s
            e = x
            while e < w and r[e] != 0 and e - x < 255:
                e += 1
            while skip > 254:
                data += bytes((254, 0))
                skip -= 254
            data += bytes((skip, e - x)) + bytes(r[x:e])
            x = e
        data.append(255)
    return bytes(data), offs


def rgb565(c):
    return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | c[2] >> 3


def main(path, sheet_out=None, names_only=False):
    sheet = Image.open(SRC).convert("RGB")
    L = ["/* generated by tools/gen_pm_sprites.py from assets/purplemonkey/concept/band-friends-midnight-v3.png */",
         "#pragma once",
         "typedef struct { uint8_t w, h, pal; uint8_t ax; const uint16_t *row; const uint8_t *d; } pm_sprite_t;",
         f"#define PM_SPR_NCOL {NCOL + 1}u", f"#define PM_SPR_POSES {0 if names_only else 3}u"]
    pals, table, review, total = [], [], [], 0
    # --names-only (the firmware's build since the pets are rigs, tools/gen_pm_rig.py): the name titles alone; the
    # whole-pose cut-outs stay reproducible from the sheet (run without it) but are not in the image
    groups = [] if names_only else [(p, [fit(cut(sheet, b, SLACK[p])) for b in BOXES[p]]) for p in PETS]
    groups.append(("NAMES", [title(p) for p in PETS]))
    for gi, (name, ims) in enumerate(groups):
        pal, idx = flatten(ims)
        pals.append(pal)
        for k, (im, rows) in enumerate(zip(ims, idx)):
            data, offs = pack(rows)
            total += len(data) + 2 * len(offs)
            tag = f"{name}_{k}"
            L.append(f"static const uint8_t PM_SPR_{tag}_D[{len(data)}] = {{" + ",".join(map(str, data)) + "};")
            L.append(f"static const uint16_t PM_SPR_{tag}_R[{len(offs)}] = {{" + ",".join(map(str, offs)) + "};")
            table.append(f"    {{{im.width}, {im.height}, {gi}, {im.width // 2}, PM_SPR_{tag}_R, PM_SPR_{tag}_D}},"
                         f"   /* {name} {POSES[k] if name != 'NAMES' else PETS[k]} */")
            back = Image.new("RGB", im.size, NIGHT)
            bp = back.load()
            for y, r in enumerate(rows):
                for x, v in enumerate(r):
                    if v:
                        bp[x, y] = pal[v - 1]
            review.append(back)
    L.append(f"static const uint16_t PM_SPR_PAL[{len(pals)}][PM_SPR_NCOL] = {{")
    for pal in pals:
        L.append("    {0, " + ", ".join(f"0x{rgb565(c):04X}" for c in pal) + "},")
    L.append("};")
    L.append("/* [pet * 3 + pose], then the four names */")
    L.append(f"static const pm_sprite_t PM_SPR[{len(table)}] = {{")
    L += table
    L.append("};")
    L.append(f"#define PM_SPR_NAME {0 if names_only else len(PETS) * 3}u")
    Path(path).write_text("\n".join(L) + "\n")
    print(f"pm sprites: {len(table)}, {total} B of pixels and rows -> {path}")
    if sheet_out and not names_only:
        W = 3 * (BOX + 8) + 8
        out = Image.new("RGB", (W + 216, 4 * (BOX + 8) + 8), NIGHT)
        for i, im in enumerate(review[:12]):
            out.paste(im, (8 + (i % 3) * (BOX + 8), 8 + (i // 3) * (BOX + 8)))
        for i, im in enumerate(review[12:]):         # the names, down the right
            out.paste(im, (W, 8 + i * (BOX + 8) + (BOX - im.height) // 2))
        Path(sheet_out).parent.mkdir(parents=True, exist_ok=True)
        out.save(sheet_out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == "--sheet" else None, "--names-only" in sys.argv)
