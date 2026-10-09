# Cute face-only cat revision (v3)

This corrects the rejected realistic v2. Only head shape, ears, eyes, nose and mouth are revised. The original cartoon body, tail, upper/lower limbs, hands and xylophone from rig-64-modular/cat are preserved pixel-for-pixel, including their palette colors and attachment geometry. The altered body/tail from v2 are not used.

Style: cute Dr. Seuss-inspired storybook ink, flat colors, broad rounded cheeks, asymmetric ears and eyes, crooked smile, uneven white blaze, amber eyes and a small pink nose with a dark spot. The face is stylized rather than photorealistic. Independent ear drawings are retained. The original closed-eye ink supplies a clear blink; remaining features come from the new face-only source. Sleepy mouth currently reuses the small closed smile.

Files: cat/spritesheet-64.png (35 sprites in 64px cells), individual PNGs, rig.json, face-variants.json, head-review.png, faces-preview.png, expressions-preview.gif and assembled.png. All sprites <=64px, binary alpha, shared palette <=31 colors. Builder validates bounds, atlas round trips, and exact pixel preservation of every non-face part.

Use this complete cat folder instead of v2 or rig-64-modular/cat when selecting the new face. Other pets remain in rig-64-modular. The 21-part rig and per-part facial texture swapping requirements are unchanged. No firmware files or hardware were changed.

Integration: `tools/gen_pm_rig.py` takes the cat from here (`OVERRIDES`, looked in before the sets) and the other three pets from rig-64-modular. This folder has no moves.json on purpose: the part names are rig-64-modular's, so the cat's choreography (play, dance, idle, props, planted, order, faces, reactions) is read from `rig-64-modular/moves.json`, and that is where the rig editor edits it. A moves.json saved here would take its place, and would need the cat's props/planted/order of its own.

Rebuild: python3 tools/art/build_cat_cute_face.py. Requires original rig-64-modular/cat, the generated source and crop-grid.json. Some frames were registered from hand-inspected crop rectangles because source layout is not an exact grid.

Source created with built-in image_gen; saved as source/cat-face-source.png. Prompt specification: redraw only the cat's modular face in a cute Dr. Seuss children's storybook style; no realistic fur, texture, hatching or gradients; black/white/pink/amber flat colors; asymmetric markings, unequal eyes, independent ears, crooked smile; blank head plus separate ears, eyes, nose and mouths on transparent background; no bodies, limbs, instruments or tails. Original generated filename: exec-a5cb0fd9-78f2-4806-bd66-5933a3fceda1.png.
