#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# PurpleMonkey's host tests (no device toolchain): the engine alone (tests/pm_engine_test.c, warnings as errors),
# then, when SDL2 is there, the emulator's acceptance scripts (tools/emu/test_pm.sh).
#   sh tests/run_pm_tests.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
CC=${CC:-cc}
$CC -std=c99 -O1 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined -o build/host/pm_engine_test tests/pm_engine_test.c
build/host/pm_engine_test
# the voice (tests/pm_speech_test.c): every word bounded and ending in silence, no step at a cut; build/host/pm_speech.wav
$CC -std=c99 -O1 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Werror \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined -o build/host/pm_speech_test tests/pm_speech_test.c
build/host/pm_speech_test build/host/pm_speech.wav | tail -1
# the installers' safety check: PurpleMonkey says what it is, and the installer's classifier accepts that text and
# still refuses the firmwares upstream refuses (docs/INSTALL-COMPAT.md)
$CC -std=c99 -O1 -Wall -Wextra -Werror -o build/host/pm_info_test tests/pm_info_test.c
build/host/pm_info_test
python3 - <<'PY'
import sys
sys.path.insert(0, "tools")
from fm1_install import classify_firmware as c
want = [(("FM-1_927", "PurpleMonkey 0.1-dev"), "allow"), (("FM-1_927", "PurpleMonkey 1.0"), "allow"),
        (("FM-1_927", ""), "refuse"), (("FM-1_927", None), "refuse"), (("FM-1_920", "ChoralRoot 0.14"), "allow"),
        (("FM-1_015", ""), "allow"), (("FM-1_000", ""), "refuse"), (("FM-1_906", "SLOOP 2.4"), "refuse"),
        (("FM-1_909", "FELUCCA 0.9-BETA"), "refuse"), (("ota-FM-1_927", ""), "loader")]
bad = [(a, w, c(*a)[0]) for a, w in want if c(*a)[0] != w]
print("FAIL" if bad else "PASS", "the installer's classifier:", bad or f"{len(want)} firmwares as expected")
sys.exit(bool(bad))
PY
# the flash driver's erase / program wait (firmware/hal/fm1_flash_wait.h, felucca-bsp 2d37ec8): never back to XIP
# while the part is busy, -4 when late (the driver makes it 0: below), a reset at 2 s / 50 ms
# (tests/flash_wait_test.c, a host model of the part)
$CC -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -o build/host/flash_wait_test tests/flash_wait_test.c
build/host/flash_wait_test
# a late finish (past 500 ms / 10 ms, before 2 s / 50 ms) is a success, counted (fm1_flash_late.h fl_wait_done)
$CC -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -o build/host/flash_late_test tests/flash_late_test.c
build/host/flash_late_test
# the boot's flash check: every persist_boot asks fm1_flash_ok.h (0x856014 and the newer FM-1s' 0xB36014), none
# compares the id itself (a unit with the newer chip would run RAM-only and take no update: INSTALL-COMPAT.md)
if grep -n '== *0x856014' firmware/src/*.c firmware/src/*.h firmware/loader/*.c 2>/dev/null; then
    echo "FAIL the flash check: a JEDEC id compared outside fm1_flash_ok.h (above)"; exit 1
fi
echo "PASS the flash check: fl_jedec_ok (fm1_flash_ok.h) at every boot"
if [ -x /opt/homebrew/bin/sdl2-config ] || command -v sdl2-config >/dev/null 2>&1; then
    sh tools/emu/test_pm.sh
else
    echo "run_pm_tests: no SDL2: the emulator tests (tools/emu/test_pm.sh) skipped"
fi
