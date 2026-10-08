# Dog — rig artwork source

Generated with built-in image_gen, using the approved midnight character sheet as identity reference. This is source artwork for the extraction/rigging workflow in docs/RIG-ART-SPEC.md, not an installed firmware rig.

## Parts (9)

head WITHOUT ears; body with NO limbs/head/tail; far arm including paw holding maraca; near arm including paw holding maraca; far leg including foot; near leg including foot; fluffy tail; far floppy ear; near floppy ear.

The assembled view is on the right; do not treat it as another body part. Hands/feet are included with their limbs as the rig spec suggests. Dog ears and llama neck are independently articulated. No ground/shadow/notes layers are included.

## Extraction and sizing

Keep a common scale for all parts of this animal. The source art is larger than the initial under-32-pixel request, following the later rig spec's explicit allowance for source sheets at any resolution. To retain that initial export limit, choose one scale factor s <= 31 / max(all extracted part widths and heights), then pad with transparency only as needed. Do not normalize each piece independently to 31×31: that destroys proportions. The resulting assembled height may be less than the spec's approximate 80 px; review that tradeoff in the rigging pass.

The assembled view is an AI-drawn placement reference, not a pixel-exact composite; establish pivots from the actual extracted parts and check joint overlap through the animation range. Palette reduction to <=31 colors is still the rig tool's job. Source art has antialiasing and more colors. No rig.json is supplied and nothing has been wired into firmware. Six expression heads are provided in expressions-source-v1.png; see ART-DELIVERY.md for frame order and alignment requirements.

## Initial generation prompt

Technical CHARACTER PARTS SHEET / PAPER-DOLL PUPPET ART, one animal only: WHITE fluffy dog with dark eyes and nose, pink tongue, two long floppy white ears separate from head, no cream-colored body. Attached image is identity reference only. Original Dr. Seuss-inspired whimsical storybook character but simplified for an 80-pixel-tall screen puppet: clean bold ink outline, flat colors with at most one shade each, large readable eyes, sparse tufts, NO fine hatching, NO shadows, no gradients, no texture. Use 12 or fewer flat colors, all pieces at exactly the SAME DRAWING SCALE. Neutral front-three-quarter standing view, all arms and legs hang STRAIGHT DOWN, head upright, tail relaxed. 
Deliver ONE transparent-background asset sheet. Upper/left ~75% of canvas: EXACTLY 9 disjoint complete parts laid out neatly with empty gaps at least one small part-width; no touching, no overlapping pieces, no cropped pieces. Parts are: head WITHOUT ears; body with NO limbs/head/tail; far arm including paw holding maraca; near arm including paw holding maraca; far leg including foot; near leg including foot; fluffy tail; far floppy ear; near floppy ear. Draw hidden attachment ends as generous rounded stumps extending inside the parent when assembled. Do NOT draw hard black cut lines across the joint ends. Body is complete uninterrupted fur with no holes, no appendages attached. Each limb includes its hand/foot as specified; held instrument belongs to its arm, not floating separately. All pieces retain common scale, do not enlarge small pieces to fill a cell.
Rightmost ~25% of canvas: ONE complete assembled neutral standing reference character using those SAME sized parts, approximately 400 pixels tall, with all joints connected. Parts must be whole even where covered in this assembled view. No second assembled figure. Leave a wide transparent separation from the loose parts.
No labels or text, no border/grid lines, no ground shadow, no music notes, no sparkles or motion lines. Actual alpha transparency. Adult rigging production sheet, not a decorative contact sheet. Ensure reference instrument proportions match detached instrument. 

## Joint/pose correction prompt

Correct this dog puppet rigging sheet. Keep the same character identity, count of parts, layout and assembled reference. Replace background AND all glow/soft shadows with completely uniform flat pure MAGENTA #FF00FF, edge to edge. NO gradient, no ground, no halo, no shadows anywhere. This is a chroma-key asset sheet. Keep magenta entirely OUT of the character itself. Remove all cream/beige cross-section ovals and interior black cap lines at joint ends. Each joint ends as a closed rounded SOLID extension matching local fur, continuous with its part. No holes/sockets in body. No colored joint caps. All pieces complete and same drawing scale as assembled reference. Simplify to flat colors and bold contours, no gradients. Dog fur is WHITE with only small light gray shade, not yellow/cream. Head has no ears, two ears remain separate. Both arms hang straight DOWN with shoulder at top, hand at bottom, maraca held downward. Assembled reference arms also hang down, not raised playing pose. Keep gaps wide, never overlap loose parts. No labels or text. This is a correction of joint construction and background, not a new character design.

