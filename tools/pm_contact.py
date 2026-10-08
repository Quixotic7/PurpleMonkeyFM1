#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""LCD screenshots (the emulator's .ppm, 240x240) -> one labelled contact sheet PNG, each at 2x.

  tools/pm_contact.py OUT.png SHOT.ppm [SHOT.ppm ...]
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw


def main(out, shots, cols=4, k=2):
    rows = (len(shots) + cols - 1) // cols
    cw, ch = 240 * k + 8, 240 * k + 22
    sheet = Image.new("RGB", (cols * cw + 8, rows * ch + 8), (12, 10, 20))
    d = ImageDraw.Draw(sheet)
    for i, s in enumerate(shots):
        im = Image.open(s).convert("RGB").resize((240 * k, 240 * k), Image.NEAREST)
        x, y = 8 + (i % cols) * cw, 8 + (i // cols) * ch
        sheet.paste(im, (x, y))
        d.text((x + 2, y + 240 * k + 4), Path(s).stem, fill=(230, 220, 250))
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
