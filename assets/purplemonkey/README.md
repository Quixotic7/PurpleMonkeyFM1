# PurpleMonkey artwork

`concept/` holds the approved concept material, copied unchanged from `ChoralRootFM1Designer/docs/purplemonkey-v1/`
and `examples/purplemonkey-v1/` on 2026-10-08 so that the build does not depend on another checkout:

| File | What |
| --- | --- |
| `band-friends-midnight-v3.png` | the approved character sheet: four animals, HELLO / PLAY / TOGETHER. **The sprites are cut from this** by `tools/gen_pm_sprites.py` |
| `musical-worlds-v4.png` | the approved environment and announcement storyboard: the direction for the scene (colours, name title, beat dots), not used as pixels |
| `purplemonkey-worlds-v4.json` | the Designer's six panel states (labels, LEDs, notes) |
| `PET-SWITCH-UX.md`, `README.md` | the announcement and interaction notes |
| `MIDNIGHT-PROMPT.md`, `MUSICAL-WORLDS-PROMPT.md` | provenance: the prompts the two sheets were generated with |

The two sheets are generated concept art (see the prompt files), supplied by the project's owner. They are concept
sheets, not sprite atlases: the firmware's sprites are a mechanical cut-out and downscale of the character sheet
(crop, key out the ground, keep the animal, scale to 120 px, 63 colours an animal), good enough to judge the
direction on a 240 x 240 screen and not the final pixel art. Known limits of the cut-outs: fine hatching turns to
noise at this size; the ground shadow under each animal is part of the sprite; loose notes and sparks are dropped.
`docs/img/pm_sprites.png` shows exactly what the firmware gets.
