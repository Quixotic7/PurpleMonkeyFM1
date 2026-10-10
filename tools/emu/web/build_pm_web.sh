#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# PurpleMonkey FM-1 in the browser: build_web.sh with PurpleMonkey's firmware side (emu_web.c with -DEMU_PM:
# ../pm_emu_fw.c) and its page (pm_index.html, pm_keymap.js over keymap.js; emu.js and worklet.js are the same).
#   [PM_VERSION=0.10] sh tools/emu/web/build_pm_web.sh
#   -> build/emu-web-pm/{index.html, emu.js, keymap.js, pm_keymap.js, worklet.js, purplemonkey.wasm}
# Needs emcc (the Emscripten SDK: ~/emsdk/emsdk_env.sh is sourced when emcc is not on the PATH); the generated
# headers are made by tools/gen.sh. PM_VERSION: the version the firmware reports ("0.10" -> "PurpleMonkey 0.10",
# as tools/build.py --release); default firmware/src/purplemonkey.c's FELUCCA_VERSION.
set -e
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$ROOT"
if ! command -v emcc >/dev/null 2>&1; then
    for e in "${EMSDK:-}/emsdk_env.sh" "$HOME/emsdk/emsdk_env.sh"; do
        [ -f "$e" ] && { . "$e" >/dev/null 2>&1; break; }
    done
fi
command -v emcc >/dev/null 2>&1 || { echo "emcc not found: install the Emscripten SDK (~/emsdk)"; exit 1; }
sh tools/gen.sh
if [ -n "${PM_VERSION:-}" ]; then
    case "$PM_VERSION" in
        PurpleMonkey*) VER=$PM_VERSION ;;
        *) VER="PurpleMonkey $(printf %s "$PM_VERSION" | tr A-Z a-z)" ;;
    esac
else
    VER=$(sed -n 's/^#define FELUCCA_VERSION "\(.*\)"/\1/p' firmware/src/purplemonkey.c | head -1)
fi
OUT=build/emu-web-pm
W=tools/emu/web
mkdir -p "$OUT"
# the firmware unit as build_pm.sh builds it (warnings off: the firmware's own code), single-threaded, no JS runtime
emcc -O2 -w -std=gnu11 -I$W/compat -Ibuild/gen -Ifirmware/src -Itools/emu -DEMU_WEB -DEMU_PM "-DFELUCCA_VERSION=\"$VER\"" \
    --no-entry -sSTANDALONE_WASM -sFILESYSTEM=0 -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 \
    -o "$OUT/purplemonkey.wasm" $W/emu_web.c
cp $W/pm_index.html "$OUT/index.html"
cp $W/emu.js $W/keymap.js $W/pm_keymap.js $W/worklet.js "$OUT/"
echo "emu-web-pm: $OUT ($VER)"
for f in purplemonkey.wasm index.html emu.js keymap.js pm_keymap.js worklet.js; do
    printf '  %-18s %8d B\n' "$f" "$(wc -c < "$OUT/$f")"
done
