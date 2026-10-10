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
| `pm_fm6.h` | `tools/gen_pm_patches.py` | the bank of 16 FM6 voices (four a pet), each with its MORPH recipe and its world, written there as operator settings |
| `pm_drumkits.h` | `tools/gen_drumkits.py --kits VINTAGE,LATIN,808,JAZZ` | SLOOP's kits, four of its 32 |
| `pm_sprites.h` | `tools/gen_pm_sprites.py` | `assets/purplemonkey/concept/band-friends-midnight-v3.png`; also writes the review sheet `docs/img/pm_sprites.png` |
| `pm_rig.h` | `tools/gen_pm_rig.py` | the pets' rigs: monkey, dog and llama from `assets/purplemonkey/rig-64-modular/`, the cat from `assets/purplemonkey/rig-64-cat-v4/` (the cute face-only revision; its choreography is still `rig-64-modular/moves.json`). Remade when a PNG or JSON in those folders (or `rig/`, `rig-64/`) is newer than the header |

The voice's words are not made by `tools/gen.sh`: `firmware/src/pm_speech_data.h` is in the tree, made by
`python3 tools/gen_pm_speech.py` (macOS `say`, or `--src DIR` for your own recordings; numpy). Run it again after
changing the words or the voice, then `sh tests/run_pm_tests.sh` and listen to `build/host/pm_speech.wav`.

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

## The browser emulator and the site

```
sh tools/emu/web/build_pm_web.sh          # -> build/emu-web-pm/ (needs the Emscripten SDK, ~/emsdk)
node tools/emu/web/test_pm_web_emu.mjs    # boots it headless: draws, plays, talks, changes pet
python3 web/make_site.py build/purplemonkey-X.Y.fwsc X.Y build/site
python3 -m http.server 8765 --directory build/site      # then http://localhost:8765/
```

`build_pm_web.sh` is ChoralRoot's `build_web.sh` with PurpleMonkey's firmware side (`emu_web.c -DEMU_PM`) and page
(`pm_index.html`, `pm_keymap.js`; `emu.js` and `worklet.js` are shared). `web/make_site.py` makes PurpleMonkey's
site when the package is named `purplemonkey*.fwsc`: the landing page `web/pm_site/`, the installer
`web/pm_index_pkg.html` (ChoralRoot's with PurpleMonkey's words: no Sounds, and no restore offered after an install,
because PurpleMonkey stores nothing), the emulator as `emu/`. The landing page's screens are the emulator's own
(`web/pm_site/img/`).

## Releasing

A release is a tag `vX.Y`; GitHub Actions builds it and publishes the site.

1. Write the notes: `docs/releases/X.Y.md` (they head the GitHub release's text).
2. Check locally: `sh tests/run_pm_tests.sh`, `./build.sh --release X.Y` (it makes `build/release-X.Y/`), the site
   as above.
3. A dry run on GitHub, the first time and after changing a workflow: Actions → release → Run workflow, with the
   version. It runs the host tests and the build and keeps the package as a workflow artifact; nothing is released.
4. Tag the commit on `main` and push the tag:

   ```
   git tag vX.Y && git push origin vX.Y
   ```

   `.github/workflows/release.yml` runs the host tests, builds the package on ubuntu (the JieLi toolchain runs
   natively there, fetched with `tools/get_toolchain.sh` and cached; the three SDK files are `tools/sdk/`), creates
   the GitHub release `vX.Y` (a pre-release for 0.x and for `X.Y-suffix`) and attaches `purplemonkey-X.Y.fwsc`,
   `purplemonkey-X.Y-app.bin`, `SHA256SUMS`, `LICENSE`, `LICENSING.md` and `LICENSES.zip`.
5. Then it starts `.github/workflows/pages.yml`, which downloads `purplemonkey-X.Y.fwsc` from the release, builds
   the browser emulator, runs `web/make_site.py` and deploys the site to
   <https://quixotic7.github.io/PurpleMonkeyFM1/>. A release published by hand starts it too. Versions with a
   suffix (`0.11-rc1`) leave the site as it is.

Once, before the first release: Settings → Pages → Build and deployment → Source: **GitHub Actions**.

When CI cannot fetch the toolchain (pkgman.jieliapp.com): build locally with `./build.sh --release X.Y`, create the
release `vX.Y` on GitHub by hand and upload every file of `build/release-X.Y/` (zip `LICENSES/` as `LICENSES.zip`),
publish it, and the site follows; or run pages.yml from the Actions tab (Run workflow, version `X.Y`).

Build options are fixed in `firmware/src/purplemonkey.c` (no flash stores, no console; USB audio recording and TRS MIDI in are on;
`FM6_POLY 8`). `FELUCCA_SIZE=0` builds everything at `-Os` as upstream.
