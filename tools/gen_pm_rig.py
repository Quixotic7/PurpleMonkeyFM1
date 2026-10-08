#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""Rigged pets (firmware/src/pm_rig.c) -> pm_rig.h

  tools/gen_pm_rig.py OUT.h                   the four pets, each from the newest art set that has it (SETS)
  tools/gen_pm_rig.py OUT.h RIGDIR ...        these folders instead (an older set, the placeholder doll)

A rig is a folder: one PNG per body part (RGBA, the part alone on a clear ground, drawn whole: also where another
part will cover it) and rig.json (docs/RIG-ART-SPEC.md):
  feet    px from the root part's pivot down to the ground
  parts   [{name, file, pivot [x, y] in the part's own pixels (the joint it turns about), parent (a part's name,
          null for the one root), at [x, y] in the PARENT's pixels (where this part's pivot is pinned)}]
  order   the parts' names, back to front
  anims   {idle, play, dance}: lists of keys {ms, root [dx, dy], a {part: degrees}}: each key is a pose (clockwise
          degrees relative to the parent; parts not named: 0) and how long the move to the NEXT key takes; the list
          loops. The firmware blends between keys.
Up to MAXP parts a rig (the articulated set has 20 to 22: upper and lower limbs, hands, feet, a head of layers).

Pictures a part can change between (the firmware swaps a part's pixels, never its joint, place or order):
  face-variants.json (assets/purplemonkey/rig-64-modular/README.md): {layers: {part: {canvas, pivot, frames {variant:
          file}, default}}, expressions {name: {part: variant}}}. The parts eyes, mouth, nose, ear_l, ear_r each
          have their own variants (LAYERS: their names and order in the firmware) and are chosen independently;
          "expressions" says which of each makes neutral, blink, happy, sing, surprised, sleepy (EXPRS).
  expressions.json (the earlier sets): six whole pictures of one part, the head: exported as one more layer, HEAD,
          whose variant is the expression itself.
Every variant of a part must have that part's canvas and pivot. A part's pictures are cropped together to the box
their opaque pixels share (the ears come on 64 x 64 canvases that are mostly clear), and its pivot and its
children's pins move with the crop: nothing shifts on the screen. All of a rig's pictures, parts and every variant,
are reduced to ONE palette together (index 0 clear, at most NCOL colours); palette.json in a folder is the art
pass's own record and is not read.

moves.json beside the rig folders (one per art set), all optional, per rig folder name:
  an animation's name  {beats, keys}: PurpleMonkey's choreography in place of rig.json's (which stays untouched);
          beats: how many beats a loop takes when the beat plays
  props   [part, ..]: things on the ground, not limbs (the cat's xylophone): they stand where the rest pose puts
          them and neither move nor turn with the pet
  planted [[upper leg, lower leg, foot] or [leg], ..]: legs whose feet stay on their spot; the firmware bends the
          knee (or aims the one-piece leg) to keep them there whatever the body does
  order   the parts back to front, in place of rig.json's

Which pet has which rig is by folder name (PETS), never by the order folders happen to list in.
Output: each picture as palette indices (one byte a pixel, 0 clear: the renderer reads them at any angle), one
palette a rig, the joints, the keys with angles in 1/256 turns. Parents are written before their children.
"""
import json
import math
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
RIGS = ROOT / "assets/purplemonkey/rig"
SETS = [ROOT / "assets/purplemonkey/rig-64-modular", ROOT / "assets/purplemonkey/rig-64", RIGS]   # newest first
NCOL = 31
MAXP = 24
ANIMS = ["idle", "play", "dance"]
EXPRS = ["neutral", "blink", "happy", "sing", "surprised", "sleepy"]      # pm_rig.c PM_RF_*
PETS = ["monkey", "cat", "dog", "llama"]                                  # pm_engine.h PM_MONKEY .. PM_LLAMA
# the parts that change picture (pm_rig.c PM_RL_*) and their variants, in the firmware's order
LAYERS = [("eyes", ["open", "blink", "happy", "surprised"]), ("mouth", ["smile", "sing", "happy", "sleepy"]),
          ("nose", ["neutral", "scrunch", "tilt"]), ("ear_l", ["relaxed", "perk", "droop", "flick"]),
          ("ear_r", ["relaxed", "perk", "droop", "flick"]), ("head", EXPRS)]
NVAR = max(len(v) for _, v in LAYERS)
NLEG = 4
NIGHT = (22, 18, 46)


def rgb565(c):
    return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | c[2] >> 3


def load(d):
    """-> rig (json), parts parents-first (each with "ims": its pictures, [0] the default, "var": {name: index}),
    palette, and per part its pictures as index lists"""
    rig = json.loads((d / "rig.json").read_text())
    parts = [dict(p) for p in rig["parts"]]
    names = [p["name"] for p in parts]
    assert len(parts) <= MAXP, f"{d.name}: {len(parts)} parts, more than {MAXP}"
    assert sum(p["parent"] is None for p in parts) == 1, f"{d.name}: exactly one root part"
    done, seq = set(), []                       # parents first
    while len(seq) < len(parts):
        n0 = len(seq)
        for p in parts:
            if p["name"] not in done and (p["parent"] is None or p["parent"] in done):
                seq.append(p)
                done.add(p["name"])
        assert len(seq) > n0, f"{d.name}: a part's parent is missing or the parents form a loop"
    assert seq[0]["parent"] is None, f"{d.name}: the root part must come first"
    assert sorted(rig["order"]) == sorted(names), f"{d.name}: order must name every part once"
    by = {p["name"]: p for p in seq}
    for p in seq:
        p["ims"] = [Image.open(d / p["file"]).convert("RGBA")]
        p["var"] = {}
        p["pivot"], p["at"] = list(p["pivot"]), list(p["at"]) if p["at"] else None
    expr = {}
    if (d / "face-variants.json").exists():      # layers: each part's own variants
        fv = json.loads((d / "face-variants.json").read_text())
        for li, (lname, vnames) in enumerate(LAYERS):
            if lname not in fv["layers"]:
                continue
            la, p = fv["layers"][lname], by[lname]
            assert list(la["pivot"]) == p["pivot"] and list(la["canvas"]) == list(p["ims"][0].size), \
                f"{d.name}: {lname}: the variants' canvas or pivot is not the part's"
            assert not set(la["frames"]) - set(vnames), f"{d.name}: {lname}: variants the firmware has no use for"
            p["ims"] = [Image.open(d / la["frames"][la["default"]]).convert("RGBA")]
            p["layer"] = li
            for v in vnames:                     # (a missing variant: the default)
                if v in la["frames"] and v != la["default"]:
                    im = Image.open(d / la["frames"][v]).convert("RGBA")
                    assert im.size == p["ims"][0].size, f"{d.name}: {lname} {v} is {im.size}"
                    p["var"][v] = len(p["ims"])
                    p["ims"].append(im)
                else:
                    p["var"][v] = 0
        for e in EXPRS:
            expr[e] = {ln: fv["expressions"].get(e, fv["expressions"]["neutral"]).get(ln, fv["layers"][ln]["default"])
                       for ln, _ in LAYERS if ln in fv["layers"]}
    elif (d / "expressions.json").exists():      # one head, six whole pictures: the HEAD layer
        ex = json.loads((d / "expressions.json").read_text())
        p = by[ex["part"]]
        assert list(ex["pivot"]) == p["pivot"] and list(ex["canvas"]) == list(p["ims"][0].size), \
            f"{d.name}: the faces' canvas or pivot is not the head's"
        assert not set(ex["frames"]) - set(EXPRS), f"{d.name}: faces the firmware has no use for"
        p["layer"] = len(LAYERS) - 1
        if "neutral" in ex["frames"]:
            p["ims"] = [Image.open(d / ex["frames"]["neutral"]).convert("RGBA")]
        for v in EXPRS:
            if v in ex["frames"] and v != "neutral":
                im = Image.open(d / ex["frames"][v]).convert("RGBA")
                assert im.size == p["ims"][0].size, f"{d.name}: face {v} is {im.size}"
                p["var"][v] = len(p["ims"])
                p["ims"].append(im)
            else:
                p["var"][v] = 0
    # crop each part's pictures to the box their opaque pixels share; the pivot and the children's pins follow
    for p in seq:
        box = None
        for im in p["ims"]:
            b = im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
            if b:
                box = b if not box else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
        assert box, f"{d.name}: {p['name']} has no opaque pixel"
        p["ims"] = [im.crop(box) for im in p["ims"]]
        p["pivot"] = [p["pivot"][0] - box[0], p["pivot"][1] - box[1]]
        for c in seq:
            if c["parent"] == p["name"]:
                c["at"] = [c["at"][0] - box[0], c["at"][1] - box[1]]
    for p in seq:
        w, h = p["ims"][0].size
        assert w <= 255 and h <= 255 and all(-128 <= v <= 383 for v in p["pivot"] + (p["at"] or [])), p["name"]
    allims = [im for p in seq for im in p["ims"]]
    strip = Image.new("RGB", (sum(i.width for i in allims), max(i.height for i in allims)), NIGHT)
    x = 0
    for im in allims:
        over = Image.new("RGB", im.size, NIGHT)
        over.paste(im, (0, 0), im)
        strip.paste(over, (x, 0))
        x += im.width
    q = strip.quantize(NCOL, method=Image.MEDIANCUT, dither=Image.NONE)
    pal = q.getpalette()[:NCOL * 3]
    # Pillow may return fewer entries when the artwork uses fewer than NCOL colours.
    pal += [0] * (NCOL * 3 - len(pal))
    pal = [tuple(pal[i * 3:i * 3 + 3]) for i in range(NCOL)]
    qp, x = q.load(), 0
    for p in seq:
        p["pix"] = []
        for im in p["ims"]:
            a = im.getchannel("A").load()
            p["pix"].append([qp[x + i, j] + 1 if a[i, j] >= 128 else 0 for j in range(im.height) for i in range(im.width)])
            x += im.width
            assert max(p["pix"][-1]) <= NCOL, f"{d.name}: palette index out of bounds"
    return rig, seq, pal, expr


def main(out, dirs):
    # default: the four pets from the newest art set that has them, in PETS order. Never "whatever folders there
    # are": the sets also hold older variants and a placeholder doll
    if not dirs:
        dirs = [next(s / n for s in SETS if (s / n / "rig.json").exists()) for n in PETS
                if any((s / n / "rig.json").exists() for s in SETS)]
        assert len(dirs) == len(PETS), "a pet has no rig: the firmware has nothing else to draw it with"
    dirs = [Path(d).resolve() for d in dirs]
    L = ["/* generated by tools/gen_pm_rig.py from " + ", ".join(sorted({d.parent.name for d in dirs})) + " */",
         "#pragma once",
         f"#define PM_RIG_MAXP {MAXP}u", f"#define PM_RIG_NCOL {NCOL + 1}u", f"#define PM_RIG_NANIM {len(ANIMS)}u",
         f"#define PM_RIG_NEXPR {len(EXPRS)}u", f"#define PM_RIG_NLAYER {len(LAYERS)}u", f"#define PM_RIG_NVAR {NVAR}u",
         f"#define PM_RIG_NLEG {NLEG}u",
         "typedef struct { uint8_t w, h, r; int8_t parent;       /* parent: a part, -1 the root, -2 a prop on the ground */",
         "                 int16_t px, py, ax, ay; const uint8_t *d; } pm_rpart_t;",
         "typedef struct { uint16_t ms; int8_t dx, dy; int8_t a[PM_RIG_MAXP]; } pm_rkey_t;",
         "typedef struct { uint8_t npart, feet; const pm_rpart_t *part; const uint8_t *order; const uint16_t *pal;",
         "                 struct { uint8_t n, beats; const pm_rkey_t *key; } anim[PM_RIG_NANIM];   /* beats: a loop's, on the beat */",
         "                 int8_t arm[2], fore[2], hand[2], neck, head;   /* parts by role (viewer's left, right): -1 = none */",
         "                 int8_t leg[PM_RIG_NLEG][3];        /* planted legs: upper, lower, foot (a one-piece leg: -1, -1) */",
         "                 int8_t bend[PM_RIG_NLEG];          /* which way each one's knee goes: +1 the viewer's left, -1 right */",
         "                 int8_t layer[PM_RIG_NLAYER];       /* PM_RL_*: the part that wears this layer's pictures, -1 = none */",
         "                 const uint8_t *tex[PM_RIG_NLAYER][PM_RIG_NVAR];   /* its pictures (each the part's w x h), 0 = the part's own */",
         "                 uint8_t expr[PM_RIG_NEXPR][PM_RIG_NLAYER];   /* PM_RF_*: which picture of each layer makes it */",
         "               } pm_rig_t;"]
    table, total, vtotal = [], 0, 0
    for ri, d in enumerate(dirs):
        rig, seq, pal, expr = load(d)
        idx = {p["name"]: i for i, p in enumerate(seq)}
        mf = d.parent / "moves.json"             # the choreography of this art set, if it has one
        moves = json.loads(mf.read_text()) if mf.exists() else {}
        props = set(moves.get("props", {}).get(d.name, []))
        chains = [c if isinstance(c, list) else [c] for c in moves.get("planted", {}).get(d.name, [])]
        assert props <= set(idx), f"{d.name}: props name no such part"
        assert len(chains) <= NLEG and all(len(c) in (1, 3) and set(c) <= set(idx) for c in chains), f"{d.name}: planted"
        for p in seq:
            assert not (p["name"] in props and p["parent"] != seq[0]["name"]), f"{p['name']}: a prop hangs from the root"
        for c in chains:
            assert seq[idx[c[0]]]["parent"] == seq[0]["name"], f"{c[0]}: a planted leg hangs from the root"
            assert len(c) == 1 or (seq[idx[c[1]]]["parent"] == c[0] and seq[idx[c[2]]]["parent"] == c[1]), f"{c}: not a chain"
        T = f"PM_RIG{ri}"
        rows = []
        for i, p in enumerate(seq):
            w, h = p["ims"][0].size
            for vi, px in enumerate(p["pix"]):
                total, vtotal = total + (len(px) if not vi else 0), vtotal + (len(px) if vi else 0)
                L.append(f"static const uint8_t {T}_D{i}{'_' + str(vi) if vi else ''}[{len(px)}] = {{" + ",".join(map(str, px)) + "};")
            pv, at = p["pivot"], p["at"] or [0, 0]
            r = math.ceil(max(math.hypot(cx - pv[0], cy - pv[1]) for cx in (0, w) for cy in (0, h)))
            assert r <= 255, p["name"]
            rows.append(f"    {{{w}, {h}, {r}, {-2 if p['name'] in props else idx[p['parent']] if p['parent'] else -1}, "
                        f"{pv[0]}, {pv[1]}, {at[0]}, {at[1]}, {T}_D{i}}},   /* {i} {p['name']} */")
        L.append(f"static const pm_rpart_t {T}_PART[{len(seq)}] = {{")
        L += rows
        L.append("};")
        order = moves.get("order", {}).get(d.name, rig["order"])       # moves.json may restack a rig
        assert sorted(order) == sorted(idx), f"{d.name}: order must name every part once"
        L.append(f"static const uint8_t {T}_ORDER[{len(seq)}] = {{" + ", ".join(str(idx[n]) for n in order) + "};")
        L.append(f"static const uint16_t {T}_PAL[PM_RIG_NCOL] = {{0, " + ", ".join(f"0x{rgb565(c):04X}" for c in pal) + "};")
        an = []
        for ai, name in enumerate(ANIMS):
            mv = moves.get(d.name, {}).get(name)                       # PurpleMonkey's choreography, if any
            keys = mv["keys"] if mv else rig["anims"][name]
            beats = int(mv["beats"]) if mv else (1 if name == "dance" else 2)
            L.append(f"static const pm_rkey_t {T}_A{ai}[{len(keys)}] = {{   /* {name} */")
            for k in keys:
                ang = [0] * MAXP
                for n, deg in k["a"].items():
                    assert n in idx, f"{d.name}: {name}: no part {n}"
                    v = round(deg * 256 / 360) % 256
                    ang[idx[n]] = v - 256 if v > 127 else v
                L.append(f"    {{{int(k['ms'])}, {int(k['root'][0])}, {int(k['root'][1])}, {{" + ", ".join(map(str, ang)) + "}},")
            L.append("};")
            an.append(f"{{{len(keys)}, {beats}, {T}_A{ai}}}")

        def role(*names):
            return next((idx[n] for n in names if n in idx), -1)
        legs = [[idx[n] for n in c] + [-1] * (3 - len(c)) for c in chains] + [[-1, -1, -1]] * (NLEG - len(chains))
        bend = [(-1 if c[0].endswith("_r") else 1) for c in chains] + [0] * (NLEG - len(chains))
        lpart, ltex = [-1] * len(LAYERS), [["0"] * NVAR for _ in LAYERS]
        ex = [[0] * len(LAYERS) for _ in EXPRS]
        for i, p in enumerate(seq):
            if "layer" not in p:
                continue
            li = p["layer"]
            lpart[li] = i
            for vi, v in enumerate(LAYERS[li][1]):
                k = p["var"].get(v, 0)
                ltex[li][vi] = f"{T}_D{i}" + (f"_{k}" if k else "")
            for ei, e in enumerate(EXPRS):
                ex[ei][li] = ei if LAYERS[li][0] == "head" else LAYERS[li][1].index(expr[e][LAYERS[li][0]])
        table.append(
            f"    {{{len(seq)}, {int(rig['feet'])}, {T}_PART, {T}_ORDER, {T}_PAL, {{" + ", ".join(an) + "},\n"
            f"     {{{role('upper_arm_l', 'arm_l')}, {role('upper_arm_r', 'arm_r')}}}, {{{role('forearm_l')}, {role('forearm_r')}}}, "
            f"{{{role('hand_l')}, {role('hand_r')}}}, {role('neck')}, {role('head_shape', 'head')},\n"
            f"     {{" + ", ".join("{" + ", ".join(map(str, c)) + "}" for c in legs) + "}, {" + ", ".join(map(str, bend)) + "},\n"
            f"     {{" + ", ".join(map(str, lpart)) + "},\n     {" + ", ".join("{" + ", ".join(t) + "}" for t in ltex) + "},\n"
            f"     {{" + ", ".join("{" + ", ".join(map(str, e)) + "}" for e in ex) + f"}}}},   /* {d.name} */")
    names = [d.name for d in dirs]
    L.append(f"#define PM_NRIG {len(table)}u")
    L.append("/* a pet's rig by its folder's name (PM_MONKEY .. PM_LLAMA), -1 = it has none (and is not drawn) */")
    L.append("static const int8_t PM_RIG_OF_PET[4] = {" + ", ".join(str(names.index(n)) if n in names else "-1" for n in PETS) + "};")
    L.append(f"#define PM_RIG_PLACEHOLDER {names.index('placeholder') if 'placeholder' in names else -1}")
    L.append(f"static const pm_rig_t PM_RIGS[{max(1, len(table))}] = {{")
    L += table or ["    {0}"]
    L.append("};")
    Path(out).write_text("\n".join(L) + "\n")
    print(f"pm rigs: {len(table)} ({', '.join(d.parent.name + '/' + d.name for d in dirs)}), "
          f"{total} B of part pixels + {vtotal} B of variants -> {out}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
