# 64px pets with separate hands

Four transparent sprite atlases, each 256 × 320 pixels: four columns by five rows of exact 64 × 64 cells. Unused cells are transparent. Individual PNGs are cropped to at most 64 × 64 and retain their own local pivots. `spritesheet-64.json` records atlas cell rectangles, content rectangles and pivots in cell coordinates.

Monkey and cat have 10 rig parts each; dog has 11. Each has independent `hand_l.png` and `hand_r.png`, parented to its corresponding arm at a wrist pivot. Hands retain their held drumsticks, mallets or maracas. Llama remains a nine-part quadruped with hooves included in its legs; it has no hands. Total: 40 parts and 24 expression frames.

The original source sheets were extracted at the larger scale. Hand cutouts share a small overlap of existing wrist pixels with their arms. No hidden joint art was synthesized. Example motions use restrained ±8-degree wrist rotations; wider rotation ranges may need additional painted overlap. Small disconnected source flecks were removed from tails and split arm/hand pieces. All expressions keep shared head dimensions and pivots.

Review `spritesheet-preview.png`, `pivot-preview.png`, `assembled.png`, `motion-preview.gif` and `expressions-preview.gif` in each pet folder. `rig-overview-64.png` shows the four assembled pets and pivots. Preview labels/backgrounds are not in the transparent atlases.

Rebuild with `python3 tools/art/build_pet_rigs_64.py` from the repository root (Pillow and NumPy). Source sheets remain in `assets/purplemonkey/rig/`. Earlier 31px and 48px variants are preserved. This folder is separate from the default rig scan; supply its four pet directories explicitly to the exporter when selecting this version.

Validation passed for sprite size, binary alpha, <=31 opaque colors per pet, head alignment, pivot/attachment bounds and atlas extraction. All four rigs exported with the current `tools/gen_pm_rig.py`; generated C passed Clang syntax checking. Export uses 55,807 bytes of body pixels plus 55,230 bytes of additional face pixels, before metadata. Firmware integration and hardware memory/performance validation remain implementation work.

For Claude: use these rigs for the requested 64px/separate-hands option. Keep hand transforms relative to their arm and preserve the atlas-versus-cropped pivot distinction. The 12-part limit is respected. Check current runtime and export support before integrating; do not replace older variants or unrelated work.
