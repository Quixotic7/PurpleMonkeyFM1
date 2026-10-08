# Monkey — rig artwork source

Generated with built-in image_gen, using the approved midnight character sheet as identity reference. This is source artwork for the extraction/rigging workflow in docs/RIG-ART-SPEC.md, not an installed firmware rig.

## Parts (8)

head with both ears; body with cream belly and NO limbs/head/tail; far arm including hand holding drumstick; near arm including hand holding drumstick; far leg including foot; near leg including foot; whole curling tail; separate round red/yellow drum.

The assembled view is on the right; do not treat it as another body part. Hands/feet are included with their limbs as the rig spec suggests. Dog ears and llama neck are independently articulated. No ground/shadow/notes layers are included.

## Extraction and sizing

Keep a common scale for all parts of this animal. The source art is larger than the initial under-32-pixel request, following the later rig spec's explicit allowance for source sheets at any resolution. To retain that initial export limit, choose one scale factor s <= 31 / max(all extracted part widths and heights), then pad with transparency only as needed. Do not normalize each piece independently to 31×31: that destroys proportions. The resulting assembled height may be less than the spec's approximate 80 px; review that tradeoff in the rigging pass.

The assembled view is an AI-drawn placement reference, not a pixel-exact composite; establish pivots from the actual extracted parts and check joint overlap through the animation range. Palette reduction to <=31 colors is still the rig tool's job. Source art has antialiasing and more colors. No rig.json is supplied and nothing has been wired into firmware. Six expression heads are provided in expressions-source-v1.png; see ART-DELIVERY.md for frame order and alignment requirements.

## Initial generation prompt

Technical CHARACTER PARTS SHEET / PAPER-DOLL PUPPET ART, one animal only: purple monkey with cream face and belly, big round ears, curling tail. Attached image is identity reference only. Original Dr. Seuss-inspired whimsical storybook character but simplified for an 80-pixel-tall screen puppet: clean bold ink outline, flat colors with at most one shade each, large readable eyes, sparse tufts, NO fine hatching, NO shadows, no gradients, no texture. Use 12 or fewer flat colors, all pieces at exactly the SAME DRAWING SCALE. Neutral front-three-quarter standing view, all arms and legs hang STRAIGHT DOWN, head upright, tail relaxed. 
Deliver ONE transparent-background asset sheet. Upper/left ~75% of canvas: EXACTLY 8 disjoint complete parts laid out neatly with empty gaps at least one small part-width; no touching, no overlapping pieces, no cropped pieces. Parts are: head with both ears; body with cream belly and NO limbs/head/tail; far arm including hand holding drumstick; near arm including hand holding drumstick; far leg including foot; near leg including foot; whole curling tail; separate round red/yellow drum. Draw hidden attachment ends as generous rounded stumps extending inside the parent when assembled. Do NOT draw hard black cut lines across the joint ends. Body is complete uninterrupted fur with no holes, no appendages attached. Each limb includes its hand/foot as specified; held instrument belongs to its arm, not floating separately. All pieces retain common scale, do not enlarge small pieces to fill a cell.
Rightmost ~25% of canvas: ONE complete assembled neutral standing reference character using those SAME sized parts, approximately 400 pixels tall, with all joints connected. Parts must be whole even where covered in this assembled view. No second assembled figure. Leave a wide transparent separation from the loose parts.
No labels or text, no border/grid lines, no ground shadow, no music notes, no sparkles or motion lines. Actual alpha transparency. Adult rigging production sheet, not a decorative contact sheet. Ensure reference instrument proportions match detached instrument. 

## Joint/pose correction prompt

Correct this monkey puppet rigging sheet. Preserve eight separate parts, one assembled reference, identity, overall layout and transparency. Remove ALL lavender cross-section ovals and internal black cap lines at neck, hips, shoulders, tail root. Fill each with continuous matching PURPLE fur ending in a rounded closed extension; no sockets/holes on torso, torso is uninterrupted purple outline with cream belly. Body has NO legs/stubs. All detached arms hang VERTICALLY DOWN, shoulder stump at TOP, wrist and hand at BOTTOM, holding drumstick pointing down; do not curl forearms upward. Assembled reference also stands in neutral rest with arms down and sticks down at sides, drum remains in front. Legs are near-straight and feet flat. Same scale between loose parts and assembled figure. Simplify purple shading to flat purple plus at most one darker shade; no gradients, no hatching, no shadows. Rounded generous joint overlaps. Maintain cream face, ears, hands, feet and cheerful face. No extra figures, no text, clean actual alpha background.

