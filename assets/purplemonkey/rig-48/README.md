# 48 × 48 pet sprite variant

Rebuilt from the original high-resolution sheets, not enlarged from the 31px assets. Each pet has a transparent 192 × 192 spritesheet with sixteen **48 × 48 cells**. Body parts occupy the first cells, then the six head expressions; unused cells are transparent. `spritesheet-48.json` identifies each cell, the cropped image rectangle and pivot in cell coordinates.

Individual PNGs retain economical cropped dimensions of at most 48 × 48. Use those with `rig.json`; atlas pivots include the centering offset and must not be substituted for cropped-image pivots. All six expressions share the same head canvas and neck pivot. Hands and feet remain included in limbs per the supplied rig spec.

Each pet includes an assembled reference, pivot preview, labeled spritesheet preview, expression GIF and motion GIF. The common part scale and local attachment points were recomputed for the new size. Original 31px assets remain in `../rig/`.

Rebuild from the repository root with `python3 tools/art/build_pet_rigs_48.py` (Pillow and NumPy). Source art is read from `assets/purplemonkey/rig/`. The 48px files live separately so the existing default rig-directory scan does not automatically switch sizes.

Validation: 34 parts, 24 expression frames, binary alpha, at most 31 opaque colors per pet, all parts <=48px, matched head canvases, in-bounds pivots/attachments and exact atlas extraction checked. The current generator exported all four rigs and the resulting header passed Clang C syntax checking. Export reported 32,683 bytes of part pixels plus 31,175 bytes of additional faces, excluding metadata. No firmware integration or hardware test was performed for this variant.

For Claude: use these folders instead of the earlier 31px folders when integrating the larger pets. Pass the four directories explicitly to the exporter in monkey/cat/dog/llama order and verify current runtime mapping. Retain palette sharing between body and expressions. This requested 48px variant supersedes the earlier under-32px constraint for this option.
