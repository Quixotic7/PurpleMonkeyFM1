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

`tools/gen.sh` regenerates PurpleMonkey's three headers when their generators change:

| Header | Generator | From |
| --- | --- | --- |
| `pm_fm6.h` | `tools/gen_pm_patches.py` | the four FM6 voices, written there as operator settings |
| `pm_drumkits.h` | `tools/gen_drumkits.py --kits VINTAGE,LATIN,808,JAZZ` | SLOOP's kits, four of its 32 |
| `pm_sprites.h` | `tools/gen_pm_sprites.py` | `assets/purplemonkey/concept/band-friends-midnight-v3.png`; also writes the review sheet `docs/img/pm_sprites.png` |

`tools/emu/test_pm.sh` writes its logs, WAVs and screenshots to `build/emu/pm/` (overwritten each run).
`tools/pm_contact.py OUT.png SHOT.ppm ...` makes a contact sheet of screenshots.

## Animating the pets by hand

```
python3 tools/rig_editor/serve.py        # then open http://127.0.0.1:8877/
```

A page for keyframing the rigs: pick a pet and an animation (idle, play, dance), drag parts to turn them, add and
time key poses, play the loop with or without the beat. In its Pose (IK) mode a dragged hand or foot pulls its
arm or leg after it, and a dragged body leaves the hands and feet where they are; what is saved is still plain
angles per part. It poses the parts as the firmware does (same joints,
easing and planted-leg solver) and previews what the firmware adds on top (a struck hand, the head's bob, the dip
on a hit). Save writes that art set's `moves.json` (the first save of a session keeps the old one as
`moves.json.bak`); `sh tools/emu/build_pm.sh` then picks it up.

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
