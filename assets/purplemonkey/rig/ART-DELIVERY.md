# Pet rig artwork delivery

Source sheets for the workflow in `docs/RIG-ART-SPEC.md`. All artwork generated with the built-in image_gen tool. Prompts and construction notes live in each animal's `SOURCE-NOTES.md`.

## Selected body sheets

- Monkey: `monkey/parts-source-v1.png` (8 loose parts + assembled reference, RGBA).
- Cat: `cat/parts-source-v1.png` (8 loose parts + assembled reference, RGBA).
- Dog: `dog/parts-source-alpha-v2.png` (9 loose parts + assembled reference, RGBA). The magenta v1 is retained as a fallback.
- Llama: `llama/parts-source-alpha-v2.png` (9 loose parts + assembled reference, RGBA). The magenta v1 is retained as a fallback.

The alpha versions were checked for transparent background pixels. Some viewers show hidden RGB color around transparent pixels; alpha-aware rendering is required. The existing generator uses alpha >=128, rather than requiring 255; dog/llama source alpha tops out at 254.

Arms include hands and held instruments; legs include feet/hooves. This follows the later rig spec's suggested 6–12-part structure rather than the initial fully separated hand/foot request. Dog ears are separate; llama has four legs and a separate neck/collar. The reference cat is shown without its detached xylophone in the assembled view so the limb placement can be seen.

## Extracted delivery — 2026-10-08

Extraction, frame registration, palette reduction and initial pivot placement are complete. Each pet folder now contains:

- Individual transparent PNG parts, each at most **31 × 31 pixels** (34 parts total).
- `rig.json`: hierarchy, parent-local attachment points, local rotation pivots, draw order, ground offset, and conservative idle/play/dance keyframes.
- Six `head_*.png` expressions on a shared canvas and head pivot; `head.png` is identical to neutral.
- `expressions.json`: frame filenames, registration offsets and shared pivot. All expressions use one common scale per pet and bottom-centre neck/chin registration, with no per-frame resizing. Some drawn facial features vary between generated expressions; registration stabilizes the attachment, not every facial landmark.
- `palette.json`: shared RGB palette for the body and expressions; binary transparency in all final sprites.
- `assembled.png`, `pivot-preview.png`, `faces-preview.png`, `expressions-preview.gif`, and `motion-preview.gif`.
- `extraction.json` and `extracted-source/`: reproducible crop coordinates, source hash, common body-part scale and high-resolution cutouts.

Hands/instruments remain part of the arms and feet remain part of the legs, following the supplied 6–12-part rig spec. The strict under-32-pixel parts limit makes these assembled pets smaller than the approximate 80-pixel target. Previews use nearest-neighbour enlargement; native sprite dimensions are unchanged.

Run `python3 tools/art/extract_pet_rigs.py` from the project root to rebuild. This requires Pillow and NumPy. Source artwork is retained. The overview and dimension/alpha/palette checks are in `review/`.

### Integration and validation

The existing `tools/gen_pm_rig.py` successfully generated a header for all four rigs (14,008 bytes of part pixels), and the generated header passed Clang C syntax checking. The export tool needed a two-line palette-padding fix because Pillow can return fewer palette entries for low-color artwork. No firmware runtime files were changed.

`expressions.json` is handoff metadata: the current generator/runtime does **not** consume it. When implementing head swaps, quantize body parts and expressions together into the actual exported palette and use the same head dimensions/pivot for every frame. The existing generator independently re-quantizes body PNGs, so its palette indices must not be assumed to match `palette.json`.

Pivots were visually reviewed in neutral, play and dance poses. Motion GIFs interpolate conservative example poses for review; they are starting choreography, not a hardware-tested performance. Firmware integration, timing to music and on-device validation remain for the implementation stage.

`pet-rigs-extracted-v1.zip` is the compact handoff with sprites, rig/expression metadata, previews, source provenance, extraction script, and corrected export tool. Original large source sheets stay in the project and the earlier source-art archive.

## Facial expression sheets

Each `expressions-source-v1.png` has six head poses in a 3-column × 2-row grid:

| Row | Left | Middle | Right |
| --- | --- | --- | --- |
| 1 | neutral | blink | happy |
| 2 | sing | surprised | sleepy |

Extracted frames now share a fixed canvas and neck pivot. The high-resolution originals retain minor shape variation between expressions. Dog expression heads exclude the separate floppy ears; other animals include ears in their heads.

Suggested uses: neutral for idle; a brief blink swap periodically; sing on sustained notes; happy for a held-key reward; surprised for pet introduction or a new musical event; sleepy for a settled idle moment. These are expressions, not phoneme-accurate speech lip-sync.

Expression frames replace the same head part, not six simultaneously attached rig parts. Inspect the current rig format before wiring them in: the art does not itself add a texture-swap mechanism. Keep the same per-animal palette across body and face assets.
