# Building PurpleMonkey FM-1

Two sides: a host side (tests and the Mac emulator) that needs no device toolchain, and the device build.
ChoralRoot's own guide, which this follows, is [docs/upstream/CHORALROOT-BUILDING.md](docs/upstream/CHORALROOT-BUILDING.md).

## Host side

```
pip3 install Pillow fonttools
brew install libraqm sdl2
sh tools/gen.sh                 # build/gen/*.h: fonts, tables, the pets' patches, kits and sprites
sh tools/emu/build_pm.sh        # -> build/host/pm_emu (runs tools/gen.sh itself)
sh tests/run_pm_tests.sh        # the engine's tests, then tools/emu/test_pm.sh
build/host/pm_emu --front       # the emulator in a window, with sound
```

`tools/gen.sh` regenerates PurpleMonkey's four headers when their generators (or, for the rigs, the art) change:

| Header | Generator | From |
| --- | --- | --- |
| `pm_fm6.h` | `tools/gen_pm_patches.py` | the four FM6 voices, written there as operator settings |
| `pm_drumkits.h` | `tools/gen_drumkits.py --kits VINTAGE,LATIN,808,JAZZ` | SLOOP's kits, four of its 32 |
| `pm_sprites.h` | `tools/gen_pm_sprites.py` | `assets/purplemonkey/concept/band-friends-midnight-v3.png`; also writes the review sheet `docs/img/pm_sprites.png` |
| `pm_rig.h` | `tools/gen_pm_rig.py` | the pets' rigs: monkey, dog and llama from `assets/purplemonkey/rig-64-modular/`, the cat from `assets/purplemonkey/rig-64-cat-v4/` (the cute face-only revision; its choreography is still `rig-64-modular/moves.json`). Remade when a PNG or JSON in those folders (or `rig/`, `rig-64/`) is newer than the header |

`tools/emu/test_pm.sh` writes its logs, WAVs and screenshots to `build/emu/pm/` (overwritten each run).
`tools/pm_contact.py OUT.png SHOT.ppm ...` makes a contact sheet of screenshots.

## Animating the pets by hand

```
python3 tools/rig_editor/serve.py        # then open http://127.0.0.1:8877/
```

A page for keyframing the rigs, laid out like a desktop animation tool: a tool palette on the left (Pose,
Turn, Move pet, Slide part), the stage in the middle, a Properties / Reactions / Rig panel on the right and a
timeline panel with transport controls along the bottom. Pick a set, a pet and an animation (idle, play, dance),
drag parts on the stage to pose them, add and time key poses on the timeline, play the loop with or without the
beat. With the Pose tool a dragged hand or foot pulls its arm or leg after it, a dragged forearm or shin aims
the elbow or knee while the hand or foot stays put (Flip bend swaps the side), and a dragged body leaves the
hands and feet where they are; what is saved is plain angles per part. Hovering a part names and outlines it;
the selected part gets its own section (angle, slide off the joint, a planted foot's spot) above the parts list,
which is grouped by limb. Numbers are scrubbable: drag a blue value sideways to change it (Shift for finer
steps) or click to type. The timeline shows the keys in proportion to their time with beat marks when the loop
is locked to the beat: click a key to select it, drag along the bar to scrub, drag a key's right edge to retime
it; each key has a thumbnail of its pose. Planted feet can be dragged to a new spot per key, any part can be
slid off its joint (the head), and the reactions the firmware adds at a note or a snare (each part turned and
slid off its joint, the body dipped) and the head's bob with the beat (per animation: the head's and neck's turn,
the head pushed sideways and up and down, each beat and over the bar) are edited in the Reactions tab (fire one
with L, R or S to see it; values are held on the stage while you adjust them). The face layers (eyes, nose,
mouth) are the Face editor's alone: the Body editor neither picks nor turns them; the ears take their picture
from the Face editor and can be turned in both (the face's turn adds to the body's). With the Pose tool, a
ring past each hand and foot and above the head turns that part in place, and dragging the head or a prop (the
drum, the xylophone) moves it.
The page is two editors: Body (the key poses) and Face (each expression's own loop of eye, mouth, nose and ear
pictures, picked from thumbnails and played apart from the body). Keys: Space plays, 1 to 4 pick the drag mode,
left and right step through the key poses, up and down turn the selected part, Alt+arrows nudge the pet,
Cmd/Ctrl+Z and Cmd/Ctrl+Shift+Z undo and redo, Cmd/Ctrl+S saves, ? opens the help. It poses the parts as the
firmware does (same joints, easing and planted-leg solver) and previews what the firmware adds on top (a struck
hand, the head's bob, the dip on a hit). Save writes that art set's `moves.json` (the first save of a session
keeps the old one as `moves.json.bak`); Revert reloads it. `sh tools/emu/build_pm.sh` then picks it up. The
page remembers the set, pet, animation, mode and playback settings between visits. The server takes its port
from the first argument or `$PORT`.

ChoralRoot's emulator and tests still build from this tree (`sh tools/emu/build.sh`, `sh tools/emu/test.sh`).

## Device build

Needs Docker and the JieLi toolchain as upstream describes (`tools/get_toolchain.sh`; on Linux x86-64 the toolchain
runs natively). The three JieLi SDK files the package carries are vendored in `tools/sdk/`.

```
./build.sh                      # build/purplemonkey.{bin,elf,dis,fwsc}, identity FM-1_927
FM1_UNIT=choralroot ./build.sh  # the ChoralRoot unit this tree was forked from, unchanged: the baseline
```

The build prints a size line and fails if RAM or POOL overflows. **It installs nothing.** Installing is a separate,
deliberate step with `tools/fm1_install.py` or the web installer, and for PurpleMonkey it has open points: read
[docs/PURPLEMONKEY.md](docs/PURPLEMONKEY.md#install-rollback-for-later-nothing-has-been-installed) first.

Build options are fixed in `firmware/src/purplemonkey.c` (no flash stores, no console; USB audio recording and TRS MIDI in are on;
`FM6_POLY 8`). `FELUCCA_SIZE=0` builds everything at `-Os` as upstream.
