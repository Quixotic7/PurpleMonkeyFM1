# PurpleMonkeyFM1 — first design study

Concepts and static storyboards only. No firmware or Designer source changed.

## Review files

- `band-friends.png`: monkey, black-and-white cat, white fluffy dog and llama; hello, play and together poses. Built-in image generation concept art, not an animation-ready sprite atlas.
- `screen-concepts.png`: illustrated visual direction for six screen moments. Concept art, not exact pixel assets or a working interface.
- `two-mode-overview.png`: actual Designer export comparing Drum and Synth panels.
- `purplemonkey-panels.png`: eight Designer panel states with annotations.
- `purplemonkey-screens.png`: exact Designer screen wireframes; words/roundels stand in for future animal art.
- `../../examples/purplemonkey-v1/purplemonkey-ui.json`: editable eight-state design, load using the Designer's Load button.
- `PROMPTS.md`: image generation prompts.

## Proposed controls

FX → DRUMS. SEL → SYNTH. PLAY → start/stop backing drum pattern. HOME → restore familiar sound/groove while retaining transport state. Other function buttons are deliberately unassigned for this first study. All sound playing happens on the 27 membrane keys.

SELECT chooses kit/instrument for the current keyboard mode. PRESETS controls brightness; ALGORITHM controls length. KNOB1–4 always control beat Speed, Busy, Bounce and Squish. MASTER remains volume. Proposed overlay labels retain stock IDs in the panel exports for orientation; they are not existing device printing.

## Interaction storyboard

1. Boot into Synth, cat ready, beat stopped.
2. PLAY starts backing drums immediately, without changing keyboard mode.
3. DRUMS selects a repeated small drum map; hits respond immediately.
4. Hold multiple keys for complementary drum repeats, not simultaneous machine-gun rolls.
5. BUSY adds curated pattern layers while preserving a recognizable pulse.
6. SYNTH changes the keyboard while backing drums continue.
7. Hold several synth notes: bounded open voicing and clocked phrase; four animal friends appear.
8. PLAY stops accompaniment; keyboard remains playable.

Mode changes release old-mode voices gracefully and require held keys to be lifted before triggering the new mode. This is a proposed rule to review. Drum repeats use the internal clock even if the backing pattern is off; synth holds become a sustained texture when it is off. Changing Busy with backing stopped gives immediate visual feedback and prepares the next pattern start.

The five synth note names illustrate scale membership, not a finalized octave map. Final mapping should repeat within a comfortable bounded register. Key labels are review annotations, not a requirement for reading or physical stickers.

Visual rewards follow sound events. White LEDs highlight actual held keys above decorative trails. Pattern stop does not imply all audio is silenced. Adding friends does not require adding another audio voice or increasing loudness. Timings, register, patch loudness, animation rate and combined FM6/drum performance remain future hardware-validation topics.

## Illustrated Designer revision (v4)

Open `/purplemonkey-preview.html?design=examples/purplemonkey-v1/purplemonkey-worlds-v4.json` on the Designer server. This is a separate mockup-only copy of the Designer with an optional `screen.art` source and crop field; original `index.html` is unchanged. Six states include the actual character art, updated PET encoder, announcement notes, mode LEDs and held-key LEDs. `worlds-v4-panels.png` and `worlds-v4-screens.png` are its exports. `designer-v4-preview.png` records the loaded Designer UI.

Artwork source rectangles are fitted into the 240×240 display for layout review; production artwork would be redrawn at the target aspect and simplified. Name speech and environment animation are annotated concepts, not functioning audio/animation. The original Designer can load this JSON with wireframe fallback; use the preview copy to see character art. SELECT now chooses PET, superseding the earlier SOUND label.
