# Art for rigged pets: what the parts must be

The firmware animates a pet by turning separately drawn body parts about joints (`firmware/src/pm_rig.c`). This
is what it needs from the art. One folder per animal under `assets/purplemonkey/rig/` (`monkey/`, `cat/`, `dog/`,
`llama/`); `placeholder/` there is a working example made of ellipses.

## The short version

- One image per body part, the part **alone and complete** on a transparent (or flat key-colour) background.
- All parts of an animal drawn at the **same scale**, in a neutral standing pose, facing the viewer or three-quarter.
- Each part drawn **whole, including what another part will cover** (the top of an arm that goes behind the
  shoulder, the neck under the head).
- Round, generous overlaps at every joint, so nothing gaps when a part turns.
- 6 to 12 parts per animal.

## Parts

| Animal | Suggested parts (back to front) |
| --- | --- |
| Monkey | tail, far leg, near leg, far arm (with drumstick), body, drum, head, near arm (with drumstick) |
| Cat | tail, far leg, near leg, far arm (with mallet), body, head, near arm (with mallet); xylophone as one more part in front |
| Dog | tail, far leg, near leg, far ear, far arm (with maraca), body, head, near ear, near arm (with maraca) |
| Llama | tail, far legs (2), near legs (2), body, neck, head (with ears), bell collar |

Optional and worth having: a second **head with eyes closed** per animal (a blink is then a swap), and a second
mouth-open head. Anything held (stick, mallet, maraca) belongs to the hand's part.

Not parts: the ground shadow, music notes, sparkles, motion lines. The firmware draws those itself.

## How each part must be drawn

1. **Complete shapes.** Draw the hidden end of every limb as a rounded stump that reaches well inside the part it
   joins (about a quarter of the limb's width past the joint). Draw the body without arms, legs, head or tail, with
   the fur continuing where they attach. This is the one thing that cannot be fixed afterwards: cutting an existing
   flat drawing apart leaves holes.
2. **Joints are circles.** Where two parts meet, both should be roughly round around the joint (shoulder, hip, neck,
   tail root), so the outline stays closed at any angle. Outlines should not run across a joint.
3. **Neutral pose.** Arms and legs hanging straight down, tail and ears relaxed, head upright. Angles in the
   animation are measured from this pose.
4. **One light direction, no cast shadows between parts** (a shadow of the head on the body would turn with the
   body and look wrong).
5. **Style for a small screen.** The pet will be about **80 px tall** on a 240 x 240 screen. Bold outline, flat
   colours with at most one shade each, big eyes, no fine hatching (it becomes noise). The black cat needs a light
   rim or light markings on every part so it reads against the night sky.
6. **Colours.** At most 31 colours per animal across all its parts (the tool reduces them; fewer and flatter is
   better). Keep the approved likenesses: the cat's amber eyes, pink nose, white blaze and bib; the dog white and
   fluffy; the llama cream with bells; the monkey purple with a cream face.

## Files

- PNG with transparency, one per part, named for the part (`head.png`, `arm_r.png`, ..). Or one sheet with the
  parts well apart on a single flat colour that appears nowhere in the animal (pure magenta or green): I will cut
  it. In a sheet, leave at least a part's width of empty space between parts.
- **Any resolution**, as long as every part of one animal is at the same scale. 4x to 8x the final size is ideal
  (an animal about 320 to 640 px tall); I scale them down together.
- Each part at most 255 x 255 px after scaling, which at 80 px tall they all are.
- A reference image of the assembled animal in the neutral pose, same scale, so I can place the joints.

You do not need to mark pivots: I place them from the assembled reference and write `rig.json`. If you want to,
a small dot of a second key colour at each joint on a copy of the sheet is enough.

## What I do with them

`rig.json` beside the parts lists each part's pivot, its parent and where it is pinned, the drawing order, and the
animations as key poses (`tools/gen_pm_rig.py` has the format; `placeholder/rig.json` is an example). Three
animations per pet to start with: `idle` (breathing, an occasional wave), `play` (playing its instrument, in time)
and `dance` (several keys held). Adding an animation or a key pose costs a few bytes; adding a part costs its
pixels (the whole placeholder is 4 KB).

## If the art comes from an image generator

Ask for a "character parts sheet" or "paper-doll / cut-out puppet sheet": the animal's body parts laid out
separately, not assembled, plus one assembled view. The usual failure is parts that are drawn already overlapping
or cropped at the joint; reject those. A prompt to start from:

> Cut-out puppet parts sheet for a 2D rigged character: [purple monkey with cream face, whimsical storybook ink
> style, bold outline, flat colours]. Each body part drawn separately and complete, spaced apart on a flat pure
> magenta background, no shadows: head; body with no limbs; left arm and right arm hanging straight, each ending in
> a rounded shoulder stump; left leg and right leg; tail. All parts at the same scale, front three-quarter view,
> neutral standing pose. Also one small assembled view of the whole character in the corner.
