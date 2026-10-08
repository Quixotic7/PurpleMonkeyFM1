# Claude Opus handoff: integrate the extracted pet rigs

> Cat likeness update: a photo-based asymmetric revision is available at `assets/purplemonkey/rig-64-cat-v2/cat/`. See its parent README and head-review.png; use that complete cat folder for the new likeness, keeping the other three pets from rig-64-modular. Its facial canvases/registration differ from the original cat.

> CURRENT REQUEST: use `assets/purplemonkey/rig-64-modular/README.md`. This latest version adds separate upper arms/forearms/hands, upper legs/lower legs/feet, and modular head shape/ears/eyes/nose/mouth. It needs 20–22 parts per pet and per-part texture swaps; the current 12-part limit and old single-head expression pathway are insufficient. Earlier variant notes below are historical.

> Latest variant: `assets/purplemonkey/rig-64/README.md` describes the requested 64px cells with separate hands for monkey, cat and dog. Use its rigs and wrist hierarchy for this option. Llama remains four-legged. Earlier size variants are preserved.

> Update: the user requested a larger 48px alternative. Use `assets/purplemonkey/rig-48/README.md` and the four pet folders there for that variant. Each atlas cell is exactly 48 × 48; individual part PNGs are at most 48 × 48. The original 31px assets remain available. The earlier size constraint below applies only to the original delivery. Inspect the current exporter/runtime: they may already have expression integration from subsequent work.

Work in `/Volumes/Q7Media-2025/Projects/Github/PurpleMonkeyFM1/PurpleMonkeyFM1`, the actual firmware Git repository inside the outer workspace. Integrate the approved pet artwork and facial animations into PurpleMonkeyFM1. Inspect current instructions and implementation first; preserve existing work and avoid broad rewrites. Files may be untracked, so do not assume untracked files are disposable.

Read these first:

- `docs/RIG-ART-SPEC.md`
- `assets/purplemonkey/rig/ART-DELIVERY.md`
- `docs/PURPLEMONKEY.md` and relevant existing handoff/integration docs
- `tools/gen_pm_rig.py`
- `firmware/src/pm_rig.c` and its callers

## Ready artwork

The folders `assets/purplemonkey/rig/{monkey,cat,dog,llama}/` contain 34 extracted body parts in total and six head expressions per pet: neutral, blink, happy, sing, surprised, sleepy. All runtime sprite PNGs are at most 31 × 31 pixels, use binary transparency, and share at most 31 opaque colors per animal. Hands and held instruments belong to arms; feet belong to legs, following the rig spec.

Each folder includes:

- `rig.json`: parent hierarchy, local pivots, parent attachment coordinates, draw order, ground offset and initial idle/play/dance motions.
- `head.png`: neutral head, identical to `head_neutral.png`.
- `head_*.png` and `expressions.json`: aligned face frames on a shared canvas and pivot.
- `palette.json`, `extraction.json`: palette and extraction provenance.
- Assembled, pivot and expression previews plus motion and expression GIFs.

Use individual sprites, not the large source sheets or preview images, in firmware. Source sheets and `extracted-source/` are retained for future art revisions. Do not regenerate artwork unless a concrete integration defect requires it. Left/right names mean viewer-left/right. Dog ears are separate parts and must remain attached when its face changes.

`review/rig-overview.png` and `review/validation.json` show the reviewed assemblies and asset checks. `tools/art/extract_pet_rigs.py` rebuilds the extracted assets using Pillow and NumPy. The compact archive `pet-rigs-extracted-v1.zip` is a convenience copy; the editable files in the repository are authoritative.

## Implement

1. Inspect the current rig mapping and build pipeline, then wire monkey, cat, dog and llama to their corresponding rigs. Avoid inadvertently selecting the placeholder through alphabetical directory order. Preserve current pet selection behavior.
2. Extend the export/runtime path to consume expression frames. Treat them as alternative textures for the existing head part, not extra attached parts. Preserve each head's dimensions, pivot, hierarchy and attachment across swaps.
3. Quantize all parts AND facial frames together into one exported palette per pet. The current generator independently re-quantizes body PNGs; do not assume its palette indices match `palette.json`. Keep transparency at index zero and respect memory/rendering limits.
4. Add restrained expression behavior appropriate to the existing event architecture: brief periodic blink, singing during held notes, happy for multiple held keys, surprised on pet introduction, sleepy after settled inactivity, and neutral fallback. Use sensible priorities and timing so events do not cause rapid flickering or interrupt a held expression unpredictably. These are expressions, not phoneme lip-sync.
5. Use the supplied idle/play/dance motions as starting poses; connect performance timing to the existing music/beat state. Maintain shoulder, neck and hip overlap through interpolation. Do not create new menu navigation for this integration.
6. Preserve the approved product behavior: membrane keyboard for playing; dedicated synth/drum mode controls; pattern start/stop; musical key mashing; music-reactive environment colors; pet switching with a brief name display and spoken animal cue if the relevant audio assets and support exist. Do not invent missing recorded samples—report that dependency explicitly.

## Validation and boundaries

The four rigs previously exported successfully with `tools/gen_pm_rig.py`: 14,008 bytes of body-part pixels before metadata and added expression storage. The generated header passed Clang C syntax checking. A small palette-padding fix was added to that generator because Pillow can return fewer than 31 palette entries. Preserve that fix.

The artwork pass did not integrate firmware or test hardware. Generated facial drawings retain minor shape variation; alignment stabilizes the shared canvas and neck attachment. The strict under-32-pixel part constraint makes assembled pets smaller than the approximate 80-pixel art-spec target; do not silently upscale stored sprites beyond the limit.

Run relevant asset/export checks and the project's firmware build. Verify all four mappings, expression dimensions/pivots, transparency, palette bounds, safe fallback behavior, draw order and memory cost. Inspect representative rendered poses and face transitions. Report what was tested, what requires hardware, and any remaining integration dependencies. Do not flash hardware or commit/push unrelated work.
