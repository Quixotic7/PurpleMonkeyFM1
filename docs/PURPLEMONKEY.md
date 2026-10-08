# PurpleMonkey FM-1: design, architecture, status

A firmware for the M-VAVE FM-1 for a small child (about one year and up) who likes pressing the membrane keys,
turning knobs and playing drums. FM6 synthesis, synthesised drums, four animal friends, a screen that answers the
music. The brief is [`CLAUDE_HANDOFF.md`](../CLAUDE_HANDOFF.md); this file says what was built from it, where it
differs, and what has and has not been checked.

**Nothing here has run on an FM-1.** Everything below was verified on the Mac emulator (the same firmware sources
with a host HAL) and by a device build that was compiled, linked and packaged but not installed.

## What it does now (stages 2 and 3 of the brief, with parts of 4 and 5)

| Control (printed name) | PurpleMonkey | |
| --- | --- | --- |
| the 27 keys | play: notes in SYNTH, drum sounds in DRUMS | the only things that play |
| FX | DRUMS | lit in DRUMS, a glow otherwise |
| SEL | SYNTH | lit in SYNTH, a glow otherwise |
| PLAY | BEAT: the backing pattern on / off | orange LED lit while it plays, the green one on each beat |
| HOME | every knob back to its familiar setting | pet, mode and beat stay |
| SELECT | PET: Monkey, Cat, Dog, Llama, round and round | its name shows for a second |
| PRESETS | BRIGHT | the synth's brightness, 17 steps |
| ALGORITHM | LENGTH | the synth's decay and release, 17 steps |
| KNOB 1 | SPEED | 72 .. 136 BPM, 104 at HOME |
| KNOB 2 | BUSY | 7 levels of the pattern |
| KNOB 3 | BOUNCE | straight .. a 2 : 1 shuffle |
| KNOB 4 | SQUISH | the drums: woody and short .. round and rubbery |
| MASTER | volume | |
| ENV LFO EDIT GLO SAVE ARP SEQ REC OCT- OCT+ | nothing | dark |
| OCT- + OCT+ + HOME held 5 s | the platform's update mode | three buttons, not upstream's two; a countdown shows from 2 s, letting go cancels |
| a MIDI keyboard on USB or the TRS jack | plays the pet's synth voice chromatically; channel 10 plays its kit (GM map) | changes nothing else; CC 120 / 123 let go |
| USB audio to a computer | records the master (channels 1-2) and the synth alone (3-4) | upstream's recording device, still named "ChoralRoot In" |

It powers on as the Cat in SYNTH with the beat stopped. There is no menu, nothing to save, nothing to erase.

**SYNTH.** The white keys climb C major pentatonic from C3 to C6 (16 notes, three octaves); a black key plays the
note of the white key on its left. A press is a note at once. At most five notes sound from held keys (a sixth
releases the oldest; its key stays lit); each further note is a little softer. Two or more keys held for half a second
bloom: the lowest held note sounds an octave down under the rest (an open voicing; it stays until the keys go and
does not chase the fingers), and a phrase walks up and down the held notes an octave above them: short eighths while
the beat plays; without it, long soft quarter notes that overlap, a texture more than a tune. Letting go ends it.
Five keys, the bass and two phrase notes are the eight FM6 voices.

**DRUMS.** Nine sounds three times over (kick, hat, snare, low tom, rim, high tom, conga, shaker, cowbell; the second
and third groups vary them and tune the toms and congas higher). A press is a hit at once, never moved to the grid.
A key held past 280 ms repeats on the clock with its sound's own rhythm (kick on the beats, snare on 2 and 4, hats
between, clave on the rim ..), so several held keys make a groove. A sound hits at most once a step and at most five
hits share a step. The repeats run whether or not the beat does.

**BEAT.** A two-bar pattern per pet, from its first step the moment PLAY is pressed. BUSY's lowest level is the
anchor (kick and backbeat); each level adds hits and removes none; a change waits for the next beat. The clock is
never restarted by a mode or pet change.

**Pets.** Each has an FM6 voice and a drum kit: Monkey a thumb-piano pluck and the LATIN kit, Cat its xylophone and
the VINTAGE (rhythm box) kit, Dog a music-box bell and the 808 kit, Llama pipes that hold and the JAZZ kit. Each has
its own night colours and its own pattern.

**Screen.** The pet stands on a hill under a moon and is never still. Its three drawn poses are key frames: HELLO
at rest; PLAY while anything sounds, with TOGETHER as a flourish every fourth accent; TOGETHER and PLAY in turn,
turning round every four accents, with three keys held or a bloom running. Between the key frames it is moved, not
redrawn: every hit squashes it into its knees and a spring throws it back up past its height; it leans to the other
side on each beat; a ripple runs up its body while it plays; at rest it breathes and waves every six seconds. It also
hops on every hit (highest for a snare); a bubble rises for each synth
note; a ripple under its feet for a kick; a star twinkles for a hat; a coloured pop for the other drums; four dots
count the beat; a knob shows its name and position for 1.3 s.

**LEDs.** White, three levels as the hardware has them (off, glow, lit): every key glows and is lit while it is held
and playing; the mode's button is lit; PLAY as above; unassigned buttons are dark.

## Architecture

One compilation unit as upstream: [`firmware/src/purplemonkey.c`](../firmware/src/purplemonkey.c).

```
keys, buttons, knobs ─ pm_ui.c (main loop) ── pm_post ──▶ ring ──▶ pm_out.c (audio ISR, every 32 samples)
                          ▲    │                                    │  pm_engine.c: keys, clock, pattern,
                 pm_snap  │    └─ pm_sound.c: pet's patch,          │              repeats, phrase
              (IRQ off)   │       BRIGHT, LENGTH ─▶ FM6 part 0      ├─ notes ─▶ voice.c ─▶ FM6 (eng_fm6.c)
                          └────────────────────────────────────────┤
   LCD ◀─ pm_ui.c scene, 16 px tiles, ≤ 44 a frame                   └─ hits ──▶ pm_drum_synth.c, 6 voices
   LEDs ◀─ pm_ui.c                                                     both ─▶ fx.c mix, reverb, limiter ─▶ I2S
```

| File | What | From |
| --- | --- | --- |
| `pm_engine.c/.h` | the musical rules; plain C, no hardware | new |
| `pm_out.c` | the engine in the audio ISR; the drum voices and their mix; `events_block` | new, after ChoralRoot's `cr_out.c` |
| `pm_drum_synth.c` | the drum models | SLOOP's `drum_synth.c`, three marked changes |
| `pm_sound.c` | the pets' FM6 patches, BRIGHT, LENGTH | new |
| `pm_ui.c` | input, LEDs, the scene and its tile renderer | new |
| `pm_shim.c` | the names `main.c` expects | new, after `cr_shim.c` |
| everything else in `firmware/` | boot, boot guard, LCD, input scan, audio ISR, voices, FM6, FX, USB, flash driver, update loader | ChoralRoot at `44453d0`, unchanged except `main.c` (one `#if` for the splash) |

ChoralRoot's instrument files (`cr_*.c`), its other engines' stores and its settings are still in the tree (the
ChoralRoot emulator and `FM1_UNIT=choralroot ./build.sh` still build, as the baseline) but are not in PurpleMonkey's
unit. See [`UPSTREAM.md`](UPSTREAM.md).

The drums are not a Felucca part: `fx.c` calls `events_block()` after clearing its mix buffers and before rendering
the parts, and PurpleMonkey's `events_block` adds the drum voices to the dry mix and the reverb send there. No
upstream DSP file was edited for it.

## Where this differs from the brief, and why

| Brief | Built | Why |
| --- | --- | --- |
| Speech cues "Monkey!" etc., with a 250 ms settle and coalescing | Not built. The name appears at once on every pet change and fades after 1.0 s | The recordings do not exist, and there is no sample path in the image (the SAMPLE engine is not built). The visual half needs no settle time: the last pet turned to is simply the one shown |
| "Start with a few carefully voiced sounds" | Four FM6 patches written for this (`tools/gen_pm_patches.py`), tuned by measuring attack, decay, release and level in the emulator, **not by ear** | Nobody has listened to them yet. Expect to revoice |
| "Layered animal animation" | Three drawn poses per animal as key frames, with procedural motion between them (squash and stretch, lean, ripple, turn, breathing). **No new frames were drawn**: there is no blink, no separate arm or tail, no in-between drawing | The approved sheet has three poses per animal and I cannot draw more. More poses in the same style would drop straight into `tools/gen_pm_sprites.py` (a box per pose) and the key-frame table |
| Holding several keys "brings friends / the full band into the scene" | The pet dances and turns; no other animals appear | One animated pet already repaints most of what a frame can send (see Budgets); friends need smaller sprites or a faster LCD path |
| Environments "change slowly through musical phrases"; snare "bounces scenery"; held notes "grow ribbons / flowers" | Fixed colours per pet; the snare is a bigger hop | Stage 5 |
| "Exact note / octave assignment remains to design" | White keys rising pentatonic C3..C6, black key = the white key to its left | Any flat hand lands on consonant notes; neighbouring keys never clash |
| "open voicing or a gentle phrase" | Both: a bass an octave under the lowest held note, and the phrase | |
| "With drums off, prefer a sustained evolving texture" | Long, soft, overlapping phrase notes at a quarter-note pace | It evolves by walking the held notes, not by changing timbre |
| Mode change: old voices released, held keys must be pressed again | As proposed | |
| "Decide and document sound-transition behavior" on a pet change | The new pet's patch replaces the old one **under** the sounding notes (an FM6 edit, not a program change): held notes carry on in the new voice, no cut. Drum voices already sounding finish in the old kit | Measured: loading as a program change cut the notes with a step three times the signal's own; the edit has none |
| Pattern changes "at musically appropriate boundaries" | BUSY at the next beat; a pet's pattern at the next step; BOUNCE and SPEED at once (the clock position is kept) | |
| "Retain reliable platform ... USB/update ... behavior" | Boot, boot guard, update entry and loader are upstream's. USB audio recording and TRS MIDI in are kept. PurpleMonkey answers the installers' "which firmware is this?" (`pm_info.c`) and both installers know its name. Removed from the unit: settings and all stores, SAFE MODE's screen (the guard still counts and still falls back to the ROM loader after four failed boots), backup / restore (there is nothing to back up), the USB serial console, the panel calibration. The update gesture is OCT- + OCT+ + HOME | Nothing is stored, so there is nothing for SAFE MODE to protect or a backup to carry |
| "Use existing regression infrastructure" | ChoralRoot's emulator, script runner and CPU estimate are reused as they are; PurpleMonkey has its own firmware side for the emulator (`tools/emu/pm_*`) and its own tests | |
| MIDI | In: notes play the sounds (above); clock, program changes and other controllers are ignored. Out: nothing | A bigger keyboard can join in without being able to change the toy's state |

## Verified, and how

`sh tests/run_pm_tests.sh` (about two minutes):

- **Engine** (`tests/pm_engine_test.c`, built with `-Werror` and the address / undefined sanitizers; 388,176 checks):
  every key gives a note or hit on the press; notes are pentatonic and in range; six-note bound; softer with more
  notes; phrase timing with and without the beat; repeats only on their lane's steps; at most five hits and one per
  sound in a step; BEAT starts on its first kick; the step sequence is unbroken through 40 mode changes and 30 pet
  changes; each BUSY level contains the one below; BUSY waits for the beat; every knob clamps; tempo and step counts at
  both ends; 50 stop / starts; 400,000 random actions followed by hands off leave no note on, no repeat running and
  no key held; a key held past its age counter's limit (6.8 hours).
- **Emulator** (`tools/emu/test_pm.sh`, headless, simulated time, bit-identical between runs):
  the first slice end to end with LED expectations and ten screenshots; all 27 keys in both modes with the beat off
  (each must light its LED and produce sound) and on; 60 s of scripted random playing on every key, button and knob,
  then silence; the heaviest load. After each: no voice sounding, no input event lost, no sample at full scale,
  last second silent. Upstream's `tools/emu/wavclicks.py` finds no jump above half scale and no dropped block in
  any of the four recordings (its third check, "clicks", fires on every drum attack and is not used).
- **Upstream's own emulator suite** (`sh tools/emu/test.sh`, ChoralRoot) still passes in this tree.

Not verified: anything on hardware (timing, LED brightness, LCD colours and tearing, key feel, speaker tone and
level, battery, the update). How it sounds. Whether a child likes it.

## Budgets

Device build, JieLi toolchain, `./build.sh`, 2026-10-08 (not installed):

| | PurpleMonkey | ChoralRoot `44453d0` (same tree, `FM1_UNIT=choralroot`) | Limit |
| --- | --- | --- | --- |
| Flash (XIP: `.text` + `.ram_text` + `.data`) | 299,396 B (51.5 %) | 354,692 B (61.0 %) | 581,564 B |
| RAM (`.data` + `.bss`) | 39,812 B (40.5 %) | 79,420 B (80.8 %) | 98,304 B |
| POOL (big buffers) | 264,300 B (76.8 %) | 317,536 B (92.3 %) | 344,064 B |

Of the flash, the sprites are 121 KB (twelve poses at up to 120 x 120 and four names, one byte a pixel, spans).
Felucca's other engines (ANALOG, PHASE, LOFI, VOICE, TRIO, WHEEL, PHYS, NOISE) are still compiled in and unused;
removing them would free flash and POOL for speech samples.

Audio: the emulator's estimate of the device's cost per 128-frame block (host instructions scaled by upstream's
calibration, `tools/emu/README.md`; budget 2,902 us). **An estimate, calibrated by upstream for ChoralRoot's code,
not a measurement of this firmware on the device.**

| Script | Average | Worst block |
| --- | --- | --- |
| first slice | 10 % | 31 % |
| every key, both modes | 12 % | 29 % |
| 60 s random playing | 23 % | 36 % |
| heaviest: 8 FM6 voices (the cap) held with the bloom, busiest beat at the fastest tempo, then 13 drum keys repeating | 31 % | 48 % |

The worst-block figure moves by a few points between runs (the host's instruction counter); ChoralRoot's own
worst-case scripts, run in the same emulator from this tree, peak at 20 to 49 % (eight scripts, recorded in
[`baseline/`](baseline/)): PurpleMonkey's heaviest case costs about what ChoralRoot's does.

FM6 is capped at 8 voices (`FM6_POLY`, ChoralRoot's cap) and the drums at 6 (`PM_NDRUM`). The screen sends at most
84 of its 225 tiles a frame (21,504 pixels, about 29 ms of SPI at the panel's 12 MHz): enough for the whole pet,
which is repainted on each of its 20 animation frames a second while it moves. So while music plays the main loop
spends more than half its time feeding the LCD; the keys are scanned between tile runs and the audio is an
interrupt, but **this is the number most in need of a hardware measurement**: the 29 ms is computed from the SPI
clock, not measured, and if the real panel is slower the animation rate or the pet's size has to come down.

## Install, rollback (for later; nothing has been installed)

`./build.sh` writes `build/purplemonkey.fwsc`, identity `FM-1_927`. **Do not install it yet without reading this.**

- **The installers' safety check.** Every Felucca-based firmware reports an `FM-1_9xx` identity, which does not tell
  them apart, so upstream's installers ask the running firmware for its version text and refuse to install over one
  they cannot name (an FM-1 was bricked by an install over SLOOP). PurpleMonkey answers that query
  (`firmware/src/pm_info.c`: "PurpleMonkey <version>"), and `tools/fm1_install.py` and `web/fm1ota.js` in this tree
  accept it; they refuse the same firmwares upstream refuses. Tested on the host: the reply byte for byte, and the
  classifier on ten identities. **Not tested: the exchange over real USB.** An installer from another tree
  (ChoralRoot's released web page, Felucca's) does not know the name and will refuse to install over PurpleMonkey
  unless forced; returning to the official V15 is never refused by any of them.
- Kept from upstream and unchanged: the boot guard (two crashes within 30 s of boot are counted; four enter the ROM
  loader), the build's own checks (image fits the slot, RAM and POOL limits, no ROM calls, the entry stub), the
  update loader, the installers' resume after an interrupted write.
- Upstream's warnings apply unchanged: [`INSTALL-COMPAT.md`](INSTALL-COMPAT.md) (units have been bricked by installs
  over some firmwares; charge fully; no hub; do not unplug), [`BOOT-SAFETY.md`](BOOT-SAFETY.md),
  [`UNBRICK-HANDOFF.md`](UNBRICK-HANDOFF.md).
- OCT- + OCT+ + HOME held for 5 s enters the ROM update mode (a countdown shows from 2 s; letting go cancels; a power
  cycle leaves it). OCT- + OCT+ held at power-on no longer runs a panel calibration.
- The USB device still enumerates under upstream's names ("ChoralRoot FM-1", recording device "ChoralRoot In"):
  the installers find the FM-1 by its port name, so the strings were left alone until that can be tried on a unit.

## Rigged pets

All four pets are rigs: body parts turned about joints by the firmware (`firmware/src/pm_rig.c`), built from the
articulated set `assets/purplemonkey/rig-64-modular/` (its README has the art's own notes). Monkey and cat have 21
parts, the dog 20, the llama 22: upper and lower limbs, hands, feet, and a head made of a blank shape with separate
ears, eyes, nose and mouth. The earlier sets (`rig-64/`, `rig/`, the placeholder doll) stay in the repository and
still export when named on the generator's command line; they are not in the image.

- **Capacity.** 24 parts a rig (`MAXP` in `tools/gen_pm_rig.py`, `PM_RIG_MAXP`); a key pose is 24 angles and an
  offset (28 bytes). A pose on the stack is about 130 bytes; the renderer holds one and builds one.
- **Mapping** is by folder name, each pet from the newest set that has it, in a fixed order; the generator stops
  if a pet has no rig (there is no other picture of it in the image any more).
- **Independent face layers.** Eyes (open, blink, happy, surprised), mouth (smile, sing, happy, sleepy), nose
  (neutral, scrunch, tilt) and each ear (relaxed, perk, droop, flick) are parts with their own pictures; the pose
  picks one per layer, separately. Only the pixels change: joint, parent and place in the order stay, and a swap
  repaints just that part. `face-variants.json`'s six combinations give the expressions (surprised on arrival,
  happy with three keys or a bloom, sing on a held note, sleepy after 20 s, a blink every 3.9 s, neutral; a face
  holds 350 ms before a lesser one replaces it), and on top of them single layers answer on their own: the ear on
  the side that just struck flicks, the nose scrunches at a snare.
- **One palette a pet**, parts and every variant reduced together (index 0 clear, at most 31 colours);
  `palette.json` is not read. A part's pictures are cropped together to the box their opaque pixels share (the
  ears arrive on 64 x 64 canvases) and its pivot and its children's pins move with the crop.
- **Planted feet with knees** (`moves.json` "planted": the cat and the monkey). Each foot stays flat on its spot;
  the firmware bends the knee to keep it there (two-bone inverse kinematics from the hip to the ankle's place at
  rest). So the body can sway, lean and sink without the feet moving: a hit now dips a planted pet into its knees
  instead of hopping it. The dog and the llama keep free legs (they breakdance).
- **The cat plays its xylophone**: elbows out, both mallets laid in across the bars, the xylophone a prop that
  stands still on the ground in front of it. A note lifts one forearm a little at the elbow and whips its mallet
  up 35 degrees at the wrist, then both fall back on the bar; the low half of the keyboard is the left hand's,
  the high half the right's, and the upper arm reaches along the bars for the note.
- **Other motion** as before: play and dance loops locked to the beat (`moves.json`, `beats`), heads tipping
  with each beat and turning over the bar, the dog's and llama's bar-long breakdance.
- **Cost.** 54,156 B of part pixels and 23,478 B of variants for the four pets.
- **LCD.** Only the parts that moved, turned or changed picture are repainted, at most 112 tiles (about 38 ms of
  SPI by calculation) a frame. Still the first number to measure on a unit.

The choreography is meant to be made by hand: `python3 tools/rig_editor/serve.py` serves an editor for the key
poses of every pet and animation (BUILDING.md, "Animating the pets by hand"), which saves `moves.json`.

Not done: the monkey, dog and llama still have the earlier, simpler choreography carried over by renaming parts
(their elbows and knees barely used; the monkey's sticks are not aimed at its drum); squash and mirroring.

## Open questions for you

1. **The other three pets** still use the earlier motion (whole-arm strikes, hops, the body turning). Say when the cat is right and I will carry it over.
2. **Speech.** Needs recordings (four words, one voice) and a decision on where they live: about 0.5 s each at
   22 kHz 8-bit is roughly 45 KB for all four, which fits the flash as it is.
3. **The sounds** need ears. `build/host/pm_emu --front` plays them.
4. **USB names.** Rename the device to "PurpleMonkey FM-1" once an install has been tried, or keep upstream's?
5. **The artwork's licence** before anything is distributed (see LICENSING.md).

## Next

Stage 3 is in (bounded macros, pattern levels, held-note and held-drum behaviour, voice bounds, transitions).
Stage 4: the announcement state machine and speech once assets exist. Stage 5: more poses, ribbons and flowers, slow
colour drift, LED trails under held keys, friends if the LCD budget allows. Stage 6: an install / rollback note
written against a real unit, after a first supervised install.
