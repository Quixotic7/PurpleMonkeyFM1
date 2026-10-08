# Articulated 64px pets with modular faces

This is the latest requested artwork option. It replaces single-piece limbs with independent elbow, wrist, knee and ankle chains, and replaces flattened expression heads with separately swappable facial features. Previous versions remain available. Files are artwork/rig handoff assets; firmware integration is not included.

## Delivered parts

- Monkey and cat: body, tail, blank head shape, two ears, eyes, nose, mouth; left/right upper arms, forearms, hands holding mallets; left/right upper legs, lower legs, feet; separate drum or xylophone. **21 active parts each.**
- Dog: the same articulated biped arrangement, with hands holding maracas and no separate body instrument. **20 active parts.**
- Llama: body, tail, neck, bell collar, blank head shape, two ears, eyes, nose, mouth; four upper legs, four lower legs and four hooves. **22 active parts.** Llama remains a quadruped with no hands or held instrument.

Opposite limbs and ears use mirrored artwork. Names describe viewer sides; llama's left pair is hind legs and right pair is front legs. Eyes are a paired sprite, independently swappable from nose and mouth.

## Facial variations

Each pet includes:

- `head_shape.png`: no eyes, nose, mouth or ears baked in.
- Each ear: relaxed, perk, droop, flick.
- Eyes: open, blink, happy, surprised.
- Mouth: smile, sing, happy, sleepy.
- Nose: neutral, scrunch, tilt.

`face-variants.json` records the canvas, pivot, default texture, all variant filenames, source registration offsets and six example expression combinations. Variants within each layer have identical dimensions and pivots. Facial features use a shared head-local canvas; ears have 64px canvases with their root registered to a common point. Features can be mixed independently. Poses are stylized generated drawings rather than phoneme-accurate lip sync.

## Sheets and previews

Each transparent `spritesheet-64.png` is 384 × 384: six columns by six rows of **64 × 64 cells**, with unused cells transparent. There are 35 occupied cells for monkey, 35 for cat, 34 for dog and 36 for llama. Individual PNG canvases never exceed 64 × 64.

`spritesheet-64.json` records each cell, the individual PNG rectangle inside it and the pivot in cell coordinates. For cropped PNGs use `rig.json` pivots; for atlas cells use the atlas pivots. These coordinate spaces are intentionally distinct.

Also included: labeled sheet preview, neutral assembly, six-expression preview, animated face preview, animated limb preview and a four-pet overview. Preview backgrounds/labels are not part of the transparent sprites.

## Joints and rendering

Upper arms attach to the body, forearms to upper arms, hands to forearms. Upper legs attach to the body, lower legs to upper legs, feet to lower legs. The new generated limb art has whole rounded ends with pivot positions inside the overlap; this is not merely slicing the previous limbs. Example poses exercise elbows around 45 degrees and knees around 28 degrees. Black joint outlines remain visible as a cut-out puppet style. Joint overlap was visually reviewed; extreme poses and on-device rendering still need review.

## Integration for Claude

The current export tool has `MAXP = 12`; this set needs at least **22**, with **24** a reasonable capacity. Update the real export/build/runtime path together and audit stack/memory usage, arrays, indexing and pet mapping. Do not silently drop limbs or merge the requested parts to fit 12.

The current single-head `expressions.json` path does not implement this modular face format. Extend export/runtime to consume `face-variants.json`, choose a texture per facial part, and retain its dimensions/pivot/hierarchy on swaps. Quantize all default parts and ALL variants into the same palette per pet. Never assume PNG or `palette.json` indices equal a separately re-quantized export palette. Blend joint rotations, but swap textures discretely using sensible expression priorities/timing.

The atlas PNGs are useful for editors; firmware may prefer the smaller per-part PNG canvases. Transparent padding still consumes bytes in a naive export. Measure full variant memory costs before firmware deployment.

## Validation and reproduction

Asset validation passed for max64 dimensions, binary alpha, <=31 opaque colors per pet across default parts and all variants, pivot and attachment bounds, and exact atlas-to-PNG round trips. The neutral/default texture rigs exported using an isolated validation process with exporter `MAXP` temporarily set to24; that generated header passed Clang C syntax checking. The actual exporter and firmware were not modified. This does not validate modular texture swapping or hardware performance.

Rebuild from the repository root with:

1. `python3 tools/art/build_pet_rigs_articulated.py`
2. `python3 tools/art/build_pet_faces_modular.py`

Requires Pillow and NumPy. The first script reads the preserved `rig-64/` assets plus `rig-64-articulated/source/limb-kit.png`. The second reads its result plus `rig-64-modular/source/face-kit.png`. Source-sheet gutters are manually registered because the generator did not produce a mathematically uniform grid. The final exported sprite cells are exact64px.

Source images were created using the built-in image-generation tool; see `source/GENERATION-NOTES.md`. `validation.json` contains the asset counts. No firmware or hardware changes were made in this pass.
