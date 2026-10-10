#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""Rigged pets (firmware/src/pm_rig.c) -> pm_rig.h

  tools/gen_pm_rig.py OUT.h                   the four pets, each from the newest art set that has it (SETS; a pet
                                              with a revision of its own, OVERRIDES, from there)
  tools/gen_pm_rig.py OUT.h RIGDIR ...        these folders instead (an older set, the placeholder doll)

A rig is a folder: one PNG per body part (RGBA, the part alone on a clear ground, drawn whole: also where another
part will cover it) and rig.json (docs/RIG-ART-SPEC.md):
  feet    px from the root part's pivot down to the ground
  parts   [{name, file, pivot [x, y] in the part's own pixels (the joint it turns about), parent (a part's name,
          null for the one root), at [x, y] in the PARENT's pixels (where this part's pivot is pinned)}]
  order   the parts' names, back to front
  anims   {idle, play, dance}: lists of keys {ms, root [dx, dy], a {part: degrees}}: each key is a pose (clockwise
          degrees relative to the parent; parts not named: 0) and how long the move to the NEXT key takes; the list
          loops. The firmware blends between keys. A key may also have t {part: [dx, dy]}: that part slid off its
          joint by so many px, in its parent's frame (the head pushed forward or pulled down into the shoulders);
          what hangs from it goes along.
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

moves.json beside the rig folders (one per art set), all optional, per rig folder name. A set without one (the
cat's rig-64-cat-v4: a face revision of rig-64-modular's cat, the same parts) shares the moves.json of the newest
set in SETS that has the pet (moves_of()):
  an animation's name  {beats, keys}: PurpleMonkey's choreography in place of rig.json's (which stays untouched);
          beats: how many beats a loop takes when the beat plays
  props   [part, ..]: things on the ground, not limbs (the cat's xylophone): they stand where the rest pose puts
          them and neither move nor turn with the pet
  planted [[upper leg, lower leg, foot] or [leg], ..]: legs whose feet stay on their spot; the firmware bends the
          knee (or aims the one-piece leg) to keep them there whatever the body does. A key's "feet" {foot: [dx,
          dy]} moves such a foot from its rest spot for that pose (a step, a kick: blended like the angles), and
          the foot part's own angle in the key turns it
  order   the parts back to front, in place of rig.json's
  faces   {expression: {keys [{ms, layers {eyes, mouth, nose, ear_l, ear_r: variant}, a {ear_l, ear_r: degrees},
          t {ear_l, ear_r: [dx, dy] px}}]}}:
          the face's own animation for neutral, blink, happy, sing, surprised, sleepy, played apart from the body's:
          each key's pictures show for its ms (a swap, not a blend), the ears' angles and places (t: screen px each
          ear is moved, x right, y down: an ear's pictures were drawn one by one and do not all sit alike) blend to
          the next key's. Not given: one key, the expression as face-variants.json has it
  reactions  {note_l, note_r, snare: {a {part: degrees}, t {part: [dx, dy] px}, dip px, rise, hold, fade ms},
          bob {idle, play, dance: {on, head [beat, bar] degrees, neck [beat, bar] degrees, x [beat, bar] px,
          y [beat, bar] px}}}: what the firmware adds on top of the animation when a note is played by the left or
          right hand or a snare hits (each part turned that far and pushed that far on screen, the body that far
          down: coming on over rise, staying for hold, dying away over fade; 0, 0, 250 when not given), and the
          head's bob with the beat per animation: a sine each beat and one over the bar, scaled into the head's turn,
          the neck's turn and the head pushed sideways and up and down (only with the beat on, and only if "on").
          An older bob {beat, bar, idle} is read as the head turning by that in all three, the neck leaning against
          it by half, idle on only if idle. Not given: default_reactions() below

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
# a pet's own revision, looked in before SETS (newest first): the cat's cute face-only revision of rig-64-modular's
# cat (new head shape, ears, eyes, nose, mouth; the body, tail, limbs, hands and xylophone the same, so its
# choreography stays in rig-64-modular/moves.json: moves_of())
OVERRIDES = {"cat": [ROOT / "assets/purplemonkey/rig-64-cat-v4"]}
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
REACTS = ["note_l", "note_r", "snare"]                                    # pm_rig.c PM_RR_*
NIGHT = (22, 18, 46)


def default_reactions(names, planted):
    """what a pet does at a note or a hit when moves.json does not say (tools/rig_editor/index.html has the same):
    the hand of that side strikes, the head nods to it; a pet on planted feet dips into its knees, another hops"""
    r = {n: {"a": {}, "dip": 0} for n in REACTS}
    head = "head_shape" if "head_shape" in names else "head" if "head" in names else None
    arms = "upper_arm_l" in names or "arm_l" in names
    for s, sign in (("l", -1), ("r", 1)):
        a = r["note_" + s]["a"]
        if "forearm_" + s in names and "hand_" + s in names:
            a["forearm_" + s], a["hand_" + s] = sign * 13, sign * 35
        elif "arm_" + s in names and "hand_" + s in names:
            a["arm_" + s], a["hand_" + s] = -sign * 7, sign * 37
        elif "arm_" + s in names:
            a["arm_" + s] = -sign * 65
        if head:
            a[head] = -sign * (8 if arms else 28)
            if "neck" in names:
                a["neck"] = sign * (4 if arms else 14)
        r["note_" + s]["dip"] = 4 if planted else -5
    r["snare"]["dip"] = 6 if planted else -8
    r["bob"] = default_bob(13, 8, False, "neck" in names)
    return r


def default_bob(beat, bar, idle, neck):
    """the head's bob per animation from the older three numbers (and the editor's defaults)"""
    one = lambda on: {"on": on, "head": [beat, bar], "neck": [-beat / 2, -bar / 2] if neck else [0, 0], "x": [0, 0], "y": [0, 0]}
    return {"idle": one(bool(idle)), "play": one(True), "dance": one(True)}


def rgb565(c):
    return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | c[2] >> 3


def sets_of(pet):
    """the art sets a pet's rig is looked for in, newest first: its own revisions (OVERRIDES), then SETS"""
    return OVERRIDES.get(pet, []) + SETS


def rig_of(pet):
    """the folder of a pet's rig: the newest set that has one, None if none has"""
    return next((s / pet for s in sets_of(pet) if (s / pet / "rig.json").exists()), None)


def moves_of(d):
    """the moves.json a rig folder goes by: its own set's; a revision of a pet's (OVERRIDES) without one shares the
    newest SETS entry's that has a rig of that name (the same parts); None if there is none"""
    shared = [s for s in SETS if (s / d.name / "rig.json").exists()] if d.parent in OVERRIDES.get(d.name, []) else []
    for s in [d.parent] + shared:
        if (s / "moves.json").exists():
            return s / "moves.json"
    return None


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
        dirs = [rig_of(n) for n in PETS if rig_of(n)]
        assert len(dirs) == len(PETS), "a pet has no rig: the firmware has nothing else to draw it with"
    dirs = [Path(d).resolve() for d in dirs]
    L = ["/* generated by tools/gen_pm_rig.py from " + ", ".join(sorted({d.parent.name for d in dirs})) + " */",
         "#pragma once",
         f"#define PM_RIG_MAXP {MAXP}u", f"#define PM_RIG_NCOL {NCOL + 1}u", f"#define PM_RIG_NANIM {len(ANIMS)}u",
         f"#define PM_RIG_NEXPR {len(EXPRS)}u", f"#define PM_RIG_NLAYER {len(LAYERS)}u", f"#define PM_RIG_NVAR {NVAR}u",
         f"#define PM_RIG_NLEG {NLEG}u", f"#define PM_RIG_NREACT {len(REACTS)}u",
         "typedef struct { uint8_t w, h, r; int8_t parent;       /* parent: a part, -1 the root, -2 a prop on the ground */",
         "                 int16_t px, py, ax, ay; const uint8_t *d; } pm_rpart_t;",
         "typedef struct { uint16_t ms; int8_t dx, dy; int8_t a[PM_RIG_MAXP];",
         "                 int8_t fx[PM_RIG_NLEG], fy[PM_RIG_NLEG];   /* each planted foot, from where it stands at rest */",
         "                 int8_t tx[PM_RIG_MAXP], ty[PM_RIG_MAXP];   /* each part slid off its joint, px in its parent's frame */",
         "               } pm_rkey_t;",
         "typedef struct { uint16_t ms; uint8_t var[PM_RIG_NLAYER];   /* a face key: each layer's picture for this long, */",
         "                 int8_t ear[2], earx[2], eary[2]; } pm_rfkey_t;   /* and the ears (left, right) turned and moved (px), blended to the next */",
         "typedef struct { uint8_t npart, feet; const pm_rpart_t *part; const uint8_t *order; const uint16_t *pal;",
         "                 struct { uint8_t n, beats; const pm_rkey_t *key; } anim[PM_RIG_NANIM];   /* beats: a loop's, on the beat */",
         "                 int8_t arm[2], fore[2], hand[2], neck, head;   /* parts by role (viewer's left, right): -1 = none */",
         "                 int8_t leg[PM_RIG_NLEG][3];        /* planted legs: upper, lower, foot (a one-piece leg: -1, -1) */",
         "                 int8_t bend[PM_RIG_NLEG];          /* which way each one's knee goes: +1 the viewer's left, -1 right */",
         "                 int8_t layer[PM_RIG_NLAYER];       /* PM_RL_*: the part that wears this layer's pictures, -1 = none */",
         "                 const uint8_t *tex[PM_RIG_NLAYER][PM_RIG_NVAR];   /* its pictures (each the part's w x h), 0 = the part's own */",
         "                 struct { uint8_t n; const pm_rfkey_t *key; } face[PM_RIG_NEXPR];   /* PM_RF_*: each expression's own",
         "                                                    * animation of the face, apart from the body's */",
         "                 struct { int8_t a[PM_RIG_MAXP]; int8_t tx[PM_RIG_MAXP], ty[PM_RIG_MAXP]; int8_t dip; uint16_t rise, hold, fade; }",
         "                   react[PM_RIG_NREACT];            /* PM_RR_*: ms to come on, to stay, to die away. At its height: each part",
         "                                                    * turned by a, slid off its joint by tx, ty, the body dip px down (- = a hop) */",
         "                 pm_rbob_t bob[PM_RIG_NANIM];       /* the head's bob with the beat, per animation */",
         "               } pm_rig_t;"]
    L[L.index("typedef struct { uint8_t npart, feet; const pm_rpart_t *part; const uint8_t *order; const uint16_t *pal;")] = (
        "typedef struct { uint8_t on; int8_t head[2], neck[2], x[2], y[2]; } pm_rbob_t;   /* [a sine each beat, one over the bar] scaled\n"
        " * into the head's turn, the neck's turn (units of 1/256 turn) and the head pushed sideways and down (px); on: only if 1 */\n"
        "typedef struct { uint8_t npart, feet; const pm_rpart_t *part; const uint8_t *order; const uint16_t *pal;")
    table, total, vtotal = [], 0, 0
    for ri, d in enumerate(dirs):
        rig, seq, pal, expr = load(d)
        idx = {p["name"]: i for i, p in enumerate(seq)}
        mf = moves_of(d)                         # the choreography of this art set (or the one it shares), if any
        moves = json.loads(mf.read_text()) if mf else {}
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
                fx, fy = [0] * NLEG, [0] * NLEG           # "feet": {foot (or one-piece leg): [dx, dy]}: planted feet moved
                for n, o in k.get("feet", {}).items():
                    ci = [c[-1] for c in chains].index(n) if n in [c[-1] for c in chains] else -1
                    if ci >= 0:                           # (a foot that is not planted here: its legs are plain angles)
                        fx[ci], fy[ci] = max(-127, min(127, round(o[0]))), max(-127, min(127, round(o[1])))
                tx, ty = [0] * MAXP, [0] * MAXP           # "t": {part: [dx, dy]}: a part slid off its joint (the head), a prop moved
                for n, o in k.get("t", {}).items():
                    assert n in idx, f"{d.name}: {name}: no part {n}"
                    tx[idx[n]], ty[idx[n]] = max(-127, min(127, round(o[0]))), max(-127, min(127, round(o[1])))
                L.append(f"    {{{int(k['ms'])}, {int(k['root'][0])}, {int(k['root'][1])}, {{" + ", ".join(map(str, ang)) + "}, {" +
                         ", ".join(map(str, fx)) + "}, {" + ", ".join(map(str, fy)) + "}, {" + ", ".join(map(str, tx)) + "}, {" +
                         ", ".join(map(str, ty)) + "}},")
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
        # each expression's face animation: moves.json "faces" {pet: {expression: {keys [{ms, layers {layer: variant},
        # a {ear_l, ear_r: degrees}, t {ear_l, ear_r: [dx, dy]}}]}}}, else one key: the expression's pictures as the art gives them
        frows = []
        faces = moves.get("faces", {}).get(d.name, {})
        for ei, e in enumerate(EXPRS):
            keys = faces.get(e, {}).get("keys") or [{"ms": 1000}]
            L.append(f"static const pm_rfkey_t {T}_F{ei}[{len(keys)}] = {{   /* {e} */")
            for k in keys:
                var = list(ex[ei])
                for ln, v in k.get("layers", {}).items():
                    li = [n for n, _ in LAYERS].index(ln)
                    assert lpart[li] >= 0 and v in LAYERS[li][1], f"{d.name}: face {e}: {ln} has no picture {v}"
                    var[li] = LAYERS[li][1].index(v)
                ear = [max(-127, min(127, round(k.get("a", {}).get(n, 0) * 256 / 360))) for n in ("ear_l", "ear_r")]
                sl = [[max(-127, min(127, round(k.get("t", {}).get(n, [0, 0])[i]))) for n in ("ear_l", "ear_r")] for i in (0, 1)]
                L.append(f"    {{{max(20, int(k['ms']))}, {{" + ", ".join(map(str, var)) + "}, {" + ", ".join(map(str, ear)) + "}, {"
                         + ", ".join(map(str, sl[0])) + "}, {" + ", ".join(map(str, sl[1])) + "}},")
            L.append("};")
            frows.append(f"{{{len(keys)}, {T}_F{ei}}}")
        rx = default_reactions(set(idx), bool(chains))
        for n, v in moves.get("reactions", {}).get(d.name, {}).items():   # moves.json's own, reaction by reaction
            rx[n] = v
        units = lambda deg: max(-127, min(127, round(deg * 256 / 360)))
        px = lambda v: max(-127, min(127, round(v)))
        rrows = []
        for n in REACTS:
            ang, tx, ty = [0] * MAXP, [0] * MAXP, [0] * MAXP
            for pn, deg in rx[n].get("a", {}).items():
                assert pn in idx, f"{d.name}: reaction {n}: no part {pn}"
                ang[idx[pn]] = units(deg)
            for pn, o in rx[n].get("t", {}).items():   # a part slid off its joint at the hit
                assert pn in idx, f"{d.name}: reaction {n}: no part {pn}"
                tx[idx[pn]], ty[idx[pn]] = px(o[0]), px(o[1])
            tm = [max(0, min(5000, int(rx[n].get(x, dflt)))) for x, dflt in (("rise", 0), ("hold", 0), ("fade", 250))]
            rrows.append("{{" + ", ".join(map(str, ang)) + "}, {" + ", ".join(map(str, tx)) + "}, {" + ", ".join(map(str, ty)) +
                         f"}}, {px(rx[n].get('dip', 0))}, {tm[0]}, {tm[1]}, {max(1, tm[2])}}}")
        bob = rx.get("bob", {})
        if "beat" in bob or "bar" in bob:         # the older shape: three numbers for the head
            bob = default_bob(bob.get("beat", 13), bob.get("bar", 8), bob.get("idle", False), "neck" in idx)
        brows = []
        for anm in ANIMS:
            b = bob.get(anm, {})
            pair = lambda key, f: "{" + ", ".join(str(f(v)) for v in (list(b.get(key, [0, 0])) + [0, 0])[:2]) + "}"
            brows.append(f"{{{1 if b.get('on') else 0}, {pair('head', units)}, {pair('neck', units)}, {pair('x', px)}, {pair('y', px)}}}")
        table.append(
            f"    {{{len(seq)}, {int(rig['feet'])}, {T}_PART, {T}_ORDER, {T}_PAL, {{" + ", ".join(an) + "},\n"
            f"     {{{role('upper_arm_l', 'arm_l')}, {role('upper_arm_r', 'arm_r')}}}, {{{role('forearm_l')}, {role('forearm_r')}}}, "
            f"{{{role('hand_l')}, {role('hand_r')}}}, {role('neck')}, {role('head_shape', 'head')},\n"
            f"     {{" + ", ".join("{" + ", ".join(map(str, c)) + "}" for c in legs) + "}, {" + ", ".join(map(str, bend)) + "},\n"
            f"     {{" + ", ".join(map(str, lpart)) + "},\n     {" + ", ".join("{" + ", ".join(t) + "}" for t in ltex) + "},\n"
            f"     {{" + ", ".join(frows) + "},\n"
            f"     {{" + ", ".join(rrows) + "},\n     {" + ", ".join(brows) + f"}}}},   /* {d.name} */")
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
