# Cat v4: viewer-left ear correction

Only the four ear_l textures were redrawn. The ear now has a simple pink inner triangle, cleaner black silhouette and rounded attachment root; ear_l.at.y is lowered by 2px to tuck into the head. Canvas64x64 and pivot32,36 are unchanged. All other individual sprite pixels are verified identical to v3, including the entire body, tail, limbs, props, right ear and facial features.

Use cat/ as the latest complete cat asset directory. See head-review.png and expressions-preview.gif. The existing 21-part modular rig integration requirements still apply. No firmware or hardware changes were made.

Rebuild: python3 tools/art/fix_cat_left_ear.py. Requires the v3 cat plus source/left-ear-source.png. Asset checks validated unchanged non-left-ear pixels, <=64px dimensions and binary alpha. Colors are quantized to the existing palette.

Source generated using built-in image_gen from the v3 head review. Prompt: four detached viewer-left cat ears in a 2x2 layout, relaxed/perked/gently drooped/outward-flicked, cute Dr. Seuss-inspired flat ink, smooth black outline, single salmon pink inner triangle, no white zigzags or texture, wide round attachment root, genuine transparency. Source original: exec-83fc80ef-3230-46a3-a3ac-0d056ba4224f.png.

## Integration

The build takes the cat from here (tools/gen_pm_rig.py OVERRIDES, which supersedes rig-64-cat-v3); the cat's choreography stays in assets/purplemonkey/rig-64-modular/moves.json, which the generator and the rig editor read for this folder. A moves.json saved here would take precedence and would need its own props, planted and order entries.
