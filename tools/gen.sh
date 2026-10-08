#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The generated headers of a host build (build/gen), made when missing or older than what makes them:
#   Felucca's / ChoralRoot's (fonts, palettes, tables, FM6 patches ..): tools/build.py's generate step
#   PurpleMonkey's: pm_drumkits.h (SLOOP's kits, a curated few), pm_fm6.h (the pets' FM6 voices), pm_sprites.h
#   (the pets cut out of the approved concept sheet; also docs/img/pm_sprites.png, the review sheet)
# No device toolchain, no Docker and no SDK needed.
set -e
cd "$(dirname "$0")/.."
export DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib${DYLD_FALLBACK_LIBRARY_PATH:+:$DYLD_FALLBACK_LIBRARY_PATH}
PY="${PYTHON:-python3}"
mkdir -p build/gen
for h in ui_fonts.h ui_palettes.h felucca_tables.h felucca_fm6.h felucca_samples.h; do
    if [ ! -f "build/gen/$h" ]; then
        "$PY" -c 'import sys; sys.path.insert(0, "tools"); import build; build.generate()'
        break
    fi
done
[ build/gen/pm_drumkits.h -nt tools/gen_drumkits.py ] && [ build/gen/pm_drumkits.h -nt tools/drumkit_levels.json ] ||
    "$PY" tools/gen_drumkits.py build/gen/pm_drumkits.h --kits VINTAGE,LATIN,808,JAZZ
[ build/gen/pm_fm6.h -nt tools/gen_pm_patches.py ] || "$PY" tools/gen_pm_patches.py build/gen/pm_fm6.h
[ build/gen/pm_rig.h -nt tools/gen_pm_rig.py ] && [ -z "$(find assets/purplemonkey/rig assets/purplemonkey/rig-64 assets/purplemonkey/rig-64-modular -newer build/gen/pm_rig.h \( -name '*.png' -o -name '*.json' \) 2>/dev/null | head -1)" ] ||
    "$PY" tools/gen_pm_rig.py build/gen/pm_rig.h
[ build/gen/pm_sprites.h -nt tools/gen_pm_sprites.py ] ||
    "$PY" tools/gen_pm_sprites.py build/gen/pm_sprites.h --names-only
