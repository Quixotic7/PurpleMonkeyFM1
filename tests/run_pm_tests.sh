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
if [ -x /opt/homebrew/bin/sdl2-config ] || command -v sdl2-config >/dev/null 2>&1; then
    sh tools/emu/test_pm.sh
else
    echo "run_pm_tests: no SDL2: the emulator tests (tools/emu/test_pm.sh) skipped"
fi
