# PurpleMonkeyFM1 — implementation handoff for Claude Opus

## Mission and authorization

Build a new M-VAVE FM-1 firmware for a young child (roughly one year and up) who enjoys randomly pressing the membrane keyboard, turning knobs, and playing drums. The user has approved the design direction and is handing it off to begin development. Deliver a responsive musical toy/instrument with FM6 synthesis, synthesized drums, four animated animal friends, and music-reactive environments. Start with a working vertical slice, then expand. Do not stop at another planning document.

This document summarizes a design conversation, not an existing implementation. No PurpleMonkey firmware has been developed. Do not mistake static concept artwork or the Designer preview for working firmware. Flashing a physical device, publishing releases, and pushing remote changes require separate user authorization.

## Workspace: important directory distinction

The outer workspace is `/Volumes/Q7Media-2025/Projects/Github/PurpleMonkeyFM1`. It is a collection of projects, not itself a Git repository.

The actual new destination repository is:
`/Volumes/Q7Media-2025/Projects/Github/PurpleMonkeyFM1/PurpleMonkeyFM1`

At handoff it contained only `.git` before this document was added. Inspect its Git status, branch, remotes and applicable AGENTS.md before changing it. Develop there. Treat sibling projects as references; preserve their source and user changes.

Sibling references, relative to the OUTER workspace:

- `ChoralRootFM1/`: working FM-1 platform, FM6 integration, emulator/build/installer references.
- `sloop-fm1/`: synthesized drum engine and groove/clock references.
- `Felucca/`: underlying platform reference.
- `ChoralRootFM1Designer/`: physical panel designer and all approved concept assets.
- `MWaveFM1Reference/`: hardware reference material.

Read relevant repository instructions and licenses. Reuse compatible working platform code rather than inventing hardware access. Inspect build/boot/flash layout carefully before choosing a base. ChoralRoot is the natural initial candidate; select the narrowest reliable integration after comparing the actual code. Preserve upstream attribution and licensing obligations (the reference firmware includes GPL code).

## Approved product principles

- Only the 27-key membrane keyboard is suitable for playing. Do not use function buttons as drum pads.
- Dedicated DRUMS and SYNTH buttons select what the keyboard plays. Dedicated BEAT starts/stops backing drum patterns in either mode.
- No child-facing menu diving, save dialogs, track selection, recording workflow, destructive actions, or multi-button gestures.
- Random playing should produce pleasant, immediate sound. Holding several keys should develop into a rewarding musical arrangement and visual scene.
- Knobs should have enjoyable bounded ranges. More notes should not mean a large increase in loudness.
- Every action gets an immediate visible response, even if a musical variation waits for the next step.
- Characters and environments react to actual musical events. Effects should remain readable, smooth and uncluttered on a small display.

## Hardware facts from local references

27 keys in F3–G5 physical layout, 14 function buttons including OCT−/OCT+, seven endless encoders with NO push switches, MASTER analog volume pot, 240×240 color TFT.

Keyboard LEDs are WHITE, not RGB. Function LEDs are white except REC red and PLAY orange, with a separate green transport indicator. Use hardware-supported brightness states; check actual driver capability before promising smooth PWM fades. The Designer's green numbered badges are annotations, not physical RGB lights.

## Proposed control mapping to implement first

| Physical control | PurpleMonkey role |
| --- | --- |
| FX | DRUMS: keyboard plays synthesized percussion |
| SEL | SYNTH: keyboard plays FM6 notes |
| PLAY | BEAT: backing pattern start/stop |
| HOME | Restore familiar sound/groove; retain transport state |
| SELECT | PET: Monkey → Cat → Dog → Llama |
| PRESETS | BRIGHT: bounded timbre brightness |
| ALGORITHM | LENGTH: bounded decay/sustain character |
| KNOB1 | SPEED: backing/repeat clock tempo |
| KNOB2 | BUSY: curated pattern density |
| KNOB3 | BOUNCE: bounded swing/syncopation |
| KNOB4 | SQUISH: drum timbre, woody/short to round/rubbery |
| MASTER | Volume |
| Remaining function buttons / OCT buttons | Unassigned, harmless in initial child mode |

PET supersedes the earlier SOUND mapping. Pet selection is independent of DRUMS/SYNTH and BEAT. All four pets should be selectable in either mode; they can each supply a curated sound/kit personality. Animal roles in artwork are not restrictions on keyboard mode. Four beat knobs keep their meanings in both modes.

## Musical behavior

### Synth

Use the six-operator FM6 engine under the hood. Start with a few carefully voiced marimba/pluck, mellow bell, rounded bass and soft sustained sounds. Avoid exposed operator editing, arbitrary DX7 patch morphing and unrestricted patch randomization.

Map every key to a pleasant scale (pentatonic initially) in a bounded register. Exact note/octave assignment remains to design: repeated C/D/E/G/A labels in the mockup show scale membership, not a finalized five-octave map.

Tap = immediate note. Hold = note blooms. Several held keys, after an initial trial threshold near 0.5 seconds, develop into an open voicing or a gentle phrase through selected held notes. Use a bounded voice count, duplicate-note handling and gain management. Every held key gets visual feedback even if not every note is simultaneously voiced. Do not constantly infer a different chord as fingers move.

With backing drums on, held-note phrases follow the same clock. With drums off, prefer a sustained evolving texture. Releasing all keys ends manual phrases gracefully; backing beat continues if enabled.

### Drums and patterns

Use Sloop's synthesized drum models, not its whole sequencer UX. Inspect:
- `sloop-fm1/firmware/src/drum_synth.c`
- `sloop-fm1/firmware/src/drums.c`
- `sloop-fm1/tools/gen_drumkits.py`
- `sloop-fm1/firmware/src/seq.c`

Reference describes 32 synthesized kits; curate a much smaller friendly selection. Existing drum code has six drum voices separate from synth-part budgets. Combined CPU/RAM load must be measured, not assumed.

All 27 keys play a repeated small kit. Exact grouping may be improved from the mockup. Manual hits sound immediately, not delayed to the next grid step. Holding a drum key adds a bounded clocked repeat; holding several keys yields complementary kick/hat/tom rhythms rather than identical rapid rolls on every lane. Repeats can use the internal clock even with backing pattern stopped.

BEAT starts a complete familiar groove immediately. Stop affects backing pattern only, not keyboard availability. Busy adds intentional hats, shakers and extra hits while retaining rhythmic anchors. Bounce changes groove without losing clock phase. Squish changes bounded synthesis parameters. Smooth tempo/timbre changes; apply pattern changes at musically appropriate boundaries. No build-up of queued fills or unbounded event stacks.

### Mode transitions

Keep backing beat and clock running when switching DRUMS/SYNTH. Initial proposed rule: gracefully release old-mode voices/repeats; keys already held must be lifted and pressed again to trigger in the new mode. Ensure no stuck notes. Pet switching must not silently change mode or transport. Decide and document sound-transition behavior for already sounding voices.

## Pet announcements

On PET selection, immediately show the selected animal and its large name. The title pops in once and fades after about 0.8–1.2 seconds; do not repeatedly flash the entire display.

Proposed short recorded cues: “Monkey!”, “Cat!”, “Dog!”, “Llama!” in one friendly voice. These audio files DO NOT EXIST yet. FM6 remains the main synth; a tiny one-shot sample path can provide speech. Inspect available audio/sample mechanisms and storage limits before implementing. Use licensed/original recordings and document provenance. Do not claim temporary sounds are finished speech assets.

Wait about 250 ms after selection settles before speaking. During rapid turning, cancel pending announcements and speak only the final pet. Fade an interrupted cue rather than overlap voices or queue animal names. No repeat announcement if selection did not change. Keep keyboard responsive and backing music running; gentle accompaniment ducking may help. Respect master level/output ceiling. Adult configuration may later allow speech off.

Timing values are starting hypotheses, not measured requirements.

## Visual identity and behavior

Four original Dr. Seuss-inspired whimsical storybook animals: expressive ink contours, bendy limbs, tufts and joyful poses. Avoid copying specific existing book characters/costumes.

- Purple monkey: cream face, round ears, curling tail; drum/kick personality.
- Black-and-white cat: golden amber eyes, pink nose, tapered white forehead blaze, white muzzle/chest, black ears/back. Match the approved v3/v4 likeness.
- White fluffy dog: floppy ears, visible dark eyes/nose; percussion personality.
- Cream llama: long curved neck, upright ears, bells; sustained-note personality.

Dark purple/teal/indigo/plum environments change slowly through musical phrases. Kick makes a soft localized pool/ripple, snare bounces scenery, hats add sparse dots, synth notes send bubbles upward, held notes grow ribbons/flowers. Holding several keys brings friends/the full band into the scene. Do not require another audio voice each time a visual friend appears. Release lets the environment settle.

Keep characters high contrast, especially the cat's black fur; use restrained light edge accents. Avoid whole-screen strobing. Give held-key LEDs priority over decorative trails. Rate-limit animations and cap particles; audio processing has priority.

## Approved artifacts

All paths below are under the OUTER workspace's `ChoralRootFM1Designer/`:

- `docs/purplemonkey-v1/band-friends-midnight-v3.png`: approved character direction and three poses per animal.
- `docs/purplemonkey-v1/musical-worlds-v4.png`: approved environment and announcement storyboard.
- `examples/purplemonkey-v1/purplemonkey-worlds-v4.json`: six panel states, current labels, LEDs and notes.
- `purplemonkey-preview.html`: separate Designer copy with artwork adapter; original `index.html` unchanged.
- `docs/purplemonkey-v1/worlds-v4-panels.png`: illustrated hardware panels.
- `docs/purplemonkey-v1/worlds-v4-screens.png`: fitted screen concepts.
- `docs/purplemonkey-v1/PET-SWITCH-UX.md`: announcement and environment notes.
- Prompt documents in the same folder describe generated art provenance.

Preview if the existing server is running:
`http://localhost:8876/purplemonkey-preview.html?design=examples/purplemonkey-v1/purplemonkey-worlds-v4.json`

These images are CONCEPT SHEETS, not animation-ready sprites or pixel-perfect display assets. The preview crops and stretches artwork into a square. Prepare clean isolated sprites/background layers and redraw/simplify for 240×240. Preserve original references. Do not use a whole storyboard texture as the final animated UI. Prioritize recognizable silhouettes/faces over fine hatching. Copy needed references into the destination repo for reproducibility; avoid depending on temporary attachment paths or a running localhost server.

## Platform references to inspect

- `ChoralRootFM1/README.md`, `BUILDING.md`, `docs/FM6.md`, `docs/INSTALL-COMPAT.md`, licensing documents.
- `ChoralRootFM1/firmware/src/choralroot.c`, `cr_ui.c`, `panel.c`, `lcd.c`.
- `ChoralRootFM1/firmware/hal/fm1_input.h`, `fm1_lcd_hw.h`.
- `ChoralRootFM1/firmware/src/fm6_core.c`, `eng_fm6.c`.
- `ChoralRootFM1/tools/emu/` and existing tests.

ChoralRoot currently caps FM6 at eight voices for performance. Treat eight as an initial budget, not proof that FM6 + drums + speech + animation will fit. Inspect source/version differences between references before merging; documentation may lag code. Retain reliable platform startup, USB/update, volume and fault-handling behavior while removing adult synth UX from the child path.

## Staged implementation and evidence

1. Establish a reproducible baseline in the destination repository. Document chosen upstream revisions/licenses, build emulator and firmware where toolchain is available. Capture baseline memory and audio timing. Do not flash.
2. Build a vertical slice: default Cat in Synth, membrane notes, DRUMS/SYNTH/BEAT buttons, one working synth voice, one small drum kit, one pattern, simple character feedback. Run it in the emulator with sound.
3. Implement bounded knob macros, curated pattern morphs, held-note/held-drum behaviors, voice management and robust transitions. Demonstrate random-key and simultaneous-knob playing.
4. Add all four pet selections and announcement state machine; integrate speech only once real assets and memory budget are available. Preserve playable behavior during asset work.
5. Add layered animal animation, reactive environments and hardware-correct LEDs. Validate readability at native 240×240 and under worst-case musical load.
6. Package a reproducible build with clear installation/rollback notes for later user-authorized hardware testing. Report what was actually verified on emulator versus hardware.

Meaningful acceptance checks:

- Every membrane key produces sound in both modes with pattern on/off.
- No stuck notes/repeats after releases, mode changes, rapid pet changes or stop/start.
- Pattern phase survives mode and pet changes; knob extremes remain usable.
- Holding many keys is musically bounded; no clipping surprises or runaway voices.
- Announcement requests coalesce; no overlapping speech backlog; playback remains responsive.
- LEDs match physical capabilities and reflect actual key/mode/transport state.
- Sustain/release and visual particles stay bounded over long sessions.
- Worst-case FM6 + drums + speech + UI stays within measured audio deadline, RAM and flash limits.
- User-facing controls never expose save/erase/editor menus.

Use existing regression infrastructure where it helps; add focused behavior tests for clocks, event ordering, transitions and bounds. Provide emulator screenshots/audio evidence when possible. Clearly report blockers (missing toolchain, speech assets, hardware validation) without fabricating success. Continue independent work rather than stopping at routine design choices. Keep changes reviewable and explain meaningful deviations from this design.
