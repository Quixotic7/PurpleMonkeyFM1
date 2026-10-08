# Source provenance and generation specifications

Method: built-in image_gen, transparent_background=true. No API-key/CLI fallback used.

Reference images: the approved monkey and cat `parts-source-v1.png`, plus dog and llama `parts-source-alpha-v2.png`, under `assets/purplemonkey/rig/`. They were inspected before generation.

## Limb-kit prompt specification

Create a technical cut-out puppet limb atlas matching the four reference pets, transparent background, exactly six columns and four rows, no labels or grid. Rows: purple monkey, black/white cat, fluffy white dog, cream llama. For the first three rows, columns: upper arm, forearm, hand holding the pet's instrument, upper leg, lower leg, foot. Llama columns: front upper leg, front lower leg, front hoof, hind upper leg, hind lower leg, hind hoof. Each segment alone, upright, whole and round-ended at both ends for elbow/knee overlap. Hands detached from forearms; feet detached from shins. Preserve bold whimsical ink outline and simple colors. Mirror the opposite side programmatically.

Generated original: `exec-37897f37-20ac-4bdd-bc33-78f04a47367c.png`; workspace copy at `../../rig-64-articulated/source/limb-kit.png`.

## Face-kit prompt specification

Create a technical modular-face atlas matching the same pets, transparent, no grid or labels, eight columns and eight rows. Each pet uses two rows in monkey/cat/dog/llama order. First row: blank earless head shape; ear relaxed; ear perk; ear droop; paired eyes open; paired eyes blink; paired eyes happy; paired eyes surprised. Second row: mouth smile; mouth sing; mouth happy; mouth sleepy; nose neutral; nose scrunch; nose tilt; ear flick. Feature sprites alone with no surrounding face/muzzle patches. No facial features baked into head shapes. Monkey purple ears and black nose; cat black/pink ears, amber eyes and pink nose; dog white floppy ears and black nose; llama cream/pink tall ears and pink nose. Rounded complete ear roots. Match reference bold storybook ink and flat colors.

Generated original: `exec-4a1556f4-ad06-4472-b1a3-d34212f206f2.png`; local copy `face-kit.png`. Output used uneven spacing despite the requested grid, so inspected gutter coordinates are recorded in `tools/art/build_pet_faces_modular.py`. Color quantization, extraction, mirroring, alignment and atlas packing are deterministic post-processing. Retained bodies and props originate from earlier approved source sheets.
