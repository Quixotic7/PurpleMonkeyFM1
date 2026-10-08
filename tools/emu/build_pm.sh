#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the PurpleMonkey FM-1 emulator: build/host/pm_emu (clang, SDL2 from Homebrew, CoreMIDI, zlib).
#   sh tools/emu/build_pm.sh            (from anywhere)
# tools/emu/build.sh with PurpleMonkey's firmware side (pm_emu_fw.c through pm_firmware.h) in place of
# ChoralRoot's; the emulator itself (emu.c, keymap.c, emu_img.c, emu_midi.c) is the same. The generated headers
# (build/gen) are made by tools/gen.sh when one is missing or older than its generator.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
sh tools/gen.sh
SDL_CFLAGS=$(/opt/homebrew/bin/sdl2-config --cflags 2>/dev/null || echo "-I/opt/homebrew/include/SDL2 -D_THREAD_SAFE")
SDL_LIBS=$(/opt/homebrew/bin/sdl2-config --libs 2>/dev/null || echo "-L/opt/homebrew/lib -lSDL2")
CC=${CC:-clang}
OUT=build/host
OBJ=build/host/pm_emu_obj
mkdir -p "$OUT" "$OBJ" build/emu
# the firmware: one unit (warnings off: Felucca's own code; PurpleMonkey's files are checked by tests/run_pm_tests.sh)
$CC -O2 -w -Ibuild/gen -Ifirmware/src -Itools/emu -c tools/emu/pm_emu_fw.c -o "$OBJ/pm_emu_fw.o"
for f in emu keymap emu_img; do
    $CC -O2 -Wall -Wextra -Wno-unused-parameter $SDL_CFLAGS -c "tools/emu/$f.c" -o "$OBJ/$f.o"
done
$CC -O2 -Wall -Wno-deprecated-declarations -c tools/emu/emu_midi.c -o "$OBJ/emu_midi.o" $SDL_CFLAGS
$CC -o "$OUT/pm_emu" "$OBJ/emu.o" "$OBJ/keymap.o" "$OBJ/emu_img.o" "$OBJ/emu_midi.o" "$OBJ/pm_emu_fw.o" \
    $SDL_LIBS -lz -lm -framework CoreMIDI -framework CoreFoundation
echo "built $OUT/pm_emu"
