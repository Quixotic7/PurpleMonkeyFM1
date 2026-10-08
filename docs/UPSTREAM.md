# Upstream revisions and what was taken

PurpleMonkey FM-1 is built on three sibling projects. This records exactly what came from where, so the tree can
be compared against, or rebased onto, its sources. All are GPL-3.0-only as a whole; see
[`../LICENSING.md`](../LICENSING.md).

| Project | Revision | Taken |
| --- | --- | --- |
| **ChoralRoot FM-1** <https://github.com/Quixotic7/ChoralRootFM1> | `44453d0524e4aae93ed4c13adfc78b0a75bec55a` (2026-10-07, "design: the QUAD engine sheet rendered"), clean working tree | The whole tree by `git archive`, except `design/` (19 MB of mock-ups), `.github/` (its release and Pages workflows: they publish) and `docs/releases/`. This is the base: Felucca's platform as ChoralRoot carries it, the FM6 engine (Melodee's), the emulator, the build and install tools |
| **SLOOP** <https://github.com/isod89/sloop-fm1> | `a1c5d68767ae10fafb6821dc63b9b1fc490342d2` (SLOOP 2.4.1) | Three files: `firmware/src/drum_synth.c` -> `firmware/src/pm_drum_synth.c`; `tools/gen_drumkits.py`; `tools/drumkit_levels.json`. Nothing else: not its sequencer, drum track, UI, HAL, loader or build |
| **Felucca** <https://github.com/hugelton/Felucca> | `20c275e39f75fa820978032efaceddfc5283c8cb` (1.0.1) | Nothing directly; it is the ancestor of both of the above and was read for reference |

`ChoralRootFM1Designer/` supplied the approved concept art and notes, copied to
[`../assets/purplemonkey/concept/`](../assets/purplemonkey/concept/) (see the README there). `MWaveFM1Reference/`
was read only.

## Why ChoralRoot as the base, and not SLOOP

Both are Felucca forks with FM6. ChoralRoot was chosen because:

- it has the Mac emulator with a script runner, LED and screen capture and a CPU estimate, which is where all of
  PurpleMonkey's verification runs;
- its instrument is already cut away from Felucca's sequencer behind a small seam (`FELUCCA_SEQ 0`:
  `events_block`, a handful of names), and that seam is where PurpleMonkey plugs in;
- its install notes ([`INSTALL-COMPAT.md`](INSTALL-COMPAT.md)) record that an FM-1 was bricked by an install over
  SLOOP, with the cause still open. SLOOP's HAL differs from ChoralRoot's in ten files, and its linker
  script differs too; none of that was taken.

SLOOP's drum synth is self-contained (it needs only Felucca's fixed-point helpers and tables, which ChoralRoot has)
and came over with three changes.

## Changes to imported files

| File | Change |
| --- | --- |
| `firmware/src/main.c` | Two `#if`s, both inert in ChoralRoot's unit: `splash()` takes `PM_SPLASH` first; the update gesture adds HOME to OCT- + OCT+ under `PM_UBOOT_HOME` |
| `tools/fm1_install.py`, `web/fm1ota.js` | One line each in the firmware classifier: a version text starting "PurpleMonkey" is installed over, like ChoralRoot's |
| `tools/emu/scripts/` | PurpleMonkey's scripts added beside ChoralRoot's |
| `firmware/src/pm_drum_synth.c` (SLOOP's `drum_synth.c`) | Marked `PM:` in the file: the kits' header name; SLOOP's `PITCH_INC[]` table replaced by ChoralRoot's `pitch_inc()`; `ds_on_lane()` added (a lane hit directly, with a transposition and SQUISH), `ds_on()` kept on top of it; two fields added to `dsv_t` for SQUISH |
| `tools/gen_drumkits.py` (SLOOP's) | `--kits NAME,...` added: emit only the named kits |
| `tools/build.py` | The unit, identity and version name come from `FM1_UNIT` (default `purplemonkey`; `choralroot` builds the upstream unit); three PurpleMonkey generators added to `generate()` |
| `tools/size_fns.py` | PurpleMonkey's main-loop files added to the build-for-size list |
| `README.md`, `PLAN.md`, `BUILDING.md` | Moved to `docs/upstream/CHORALROOT-*.md`; new ones written |

Everything else imported is byte-identical to its source revision. ChoralRoot's own unit, emulator and tests still
build and pass here (`FM1_UNIT=choralroot ./build.sh`, `sh tools/emu/test.sh`), which is the baseline PurpleMonkey's
numbers are compared with.

## New files

`firmware/src/purplemonkey.c`, `pm_engine.c`, `pm_engine.h`, `pm_out.c`, `pm_sound.c`, `pm_ui.c`, `pm_shim.c`, `pm_info.c`;
`tools/gen_pm_patches.py`, `gen_pm_sprites.py`, `pm_contact.py`, `gen.sh`; `tools/emu/pm_firmware.h`,
`pm_emu_fw.c`, `build_pm.sh`, `test_pm.sh`, `pm_scripts.py`, `pm_wav.py`, `scripts/pm_slice.txt`;
`tests/pm_engine_test.c`, `pm_info_test.c`, `run_pm_tests.sh`; `docs/PURPLEMONKEY.md`, `UPSTREAM.md`, `baseline/`, `img/`;
`assets/purplemonkey/`.

## Not carried over, on purpose

ChoralRoot's `.github/workflows/` build a release on a tag and publish a site. They are not in this tree, so that
nothing here can publish by accident. `web/` (the installer and landing pages) is in the tree but is ChoralRoot's
and has not been adapted: do not deploy it for PurpleMonkey as it is.
