#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless acceptance of the PurpleMonkey emulator (no window, no audio device, simulated time: deterministic):
#   sh tools/emu/test_pm.sh         builds build/host/pm_emu when a source is newer, runs the scripts
# The scripts: tools/emu/scripts/pm_slice.txt (the first slice end to end) and the generated ones of
# tools/emu/pm_scripts.py (every key in both modes, random playing, the heaviest load). Outputs: build/emu/pm/
# (overwritten by each run; nothing is deleted).
# What this checks is the emulator: the same firmware sources on a Mac. Nothing here has run on an FM-1.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
EMU=build/host/pm_emu
OUT=build/emu/pm
fail=0
ok()  { echo "  ok    $*"; }
bad() { echo "  FAIL  $*"; fail=1; }
num() { sed -n "s/.*$1 \([0-9][0-9]*\).*/\1/p" "$2" | tail -1; }

if [ ! -x "$EMU" ] || [ -n "$(find tools firmware/src tests/hostsim.c -newer "$EMU" \( -name '*.[ch]' -o -name 'gen_*.py' \) 2>/dev/null | head -1)" ]; then
    sh tools/emu/build_pm.sh || { echo "build failed"; exit 1; }
fi
mkdir -p "$OUT/again"
python3 tools/emu/pm_scripts.py "$OUT" || exit 1

run() {   # run NAME SCRIPT: the log in $OUT/NAME.log, the WAV in $OUT/NAME.wav
    "$EMU" --headless --script "$2" --wav "$OUT/$1.wav" >"$OUT/$1.log" 2>&1
    st=$?
    if [ $st -eq 0 ]; then ok "$1: exit 0 ($(grep -c '^expect .*: ok' "$OUT/$1.log") expectations met)"
    else bad "$1: exit $st (see $OUT/$1.log)"; grep 'FAILED\|script' "$OUT/$1.log" | head -8 | sed 's/^/        /'; fi
}
# after hands off: the engine holds no note, no synth or drum voice sounds, no event was lost, and the output's last
# second is silent (|sample| <= 1: fx.c's DC blocker leaves a 1 LSB dither behind a kick, not a sound)
quiet() {
    last=$(grep 'pm: held' "$OUT/$1.log" | tail -1)
    echo "$last" | grep -q 'held 0000000 voiced 0000000 .*(engine holds 0) phrase 0  synth voices 0 drum voices 0' \
        && ok "$1: hands off: no key held, no note on, no voice sounding" || bad "$1: something still sounds:$last"
    [ "$(num 'events lost' "$OUT/$1.log")" = 0 ] && ok "$1: no input event lost" || bad "$1: input events lost"
    w=$(python3 tools/emu/pm_wav.py "$OUT/$1.wav")
    tp=$(echo "$w" | sed -n 's/.*tail_peak \([0-9]*\).*/\1/p')
    [ "${tp:-9}" -le 1 ] && ok "$1: the last second is silent" || bad "$1: the last second peaks at $tp"
    fsn=$(echo "$w" | sed -n 's/.*full_scale \([0-9]*\).*/\1/p')
    [ "${fsn:-9}" = 0 ] && ok "$1: no sample at full scale ($w)" || bad "$1: $fsn samples at full scale ($w)"
}
# the audio ISR's cost: the emulator's device estimate (host instructions per block, tools/emu/README.md), its
# worst block against the 2.9 ms a block has. The firmware does the same work on every run of a script (simulated
# time), but the host's instruction counter now and then charges a block for something else (seen: one block in
# 23000 at 170 %, gone on the next run); so a run over the limit is repeated, up to twice, and the lowest worst
# block counts: what every run has in common
peak_of() { sed -n 's/^cpu: host instructions per 128-frame block.*(\([0-9]*\) % \/ \([0-9]*\) %).*/\2/p' "$1"; }
cpu() {   # cpu NAME LIMIT SCRIPT
    pct=$(peak_of "$OUT/$1.log")
    for again in 2 3; do
        [ -n "$pct" ] && [ "$pct" -lt "$2" ] && break
        "$EMU" --headless --script "$3" >"$OUT/$1.cpu$again.log" 2>&1
        p2=$(peak_of "$OUT/$1.cpu$again.log")
        [ -n "$p2" ] && { [ -z "$pct" ] || [ "$p2" -lt "$pct" ]; } && pct=$p2
    done
    [ -n "$pct" ] && [ "$pct" -lt "$2" ] \
        && ok "$1: the audio block's device estimate peaks at $pct % of its budget (limit $2 %)" \
        || bad "$1: the audio block's device estimate peaks at ${pct:-?} % (limit $2 %) on every run"
    [ "$(num 'voices shed on overload' "$OUT/$1.log")" = 0 ] && ok "$1: no voice shed on overload" || bad "$1: voices shed"
    tm=$(num 'most in a frame' "$OUT/$1.log")
    [ "${tm:-999}" -le 112 ] && ok "$1: at most $tm tiles (of 225) sent to the LCD in a frame" || bad "$1: $tm tiles in a frame"
}

echo "the first slice (pm_slice.txt): Cat in SYNTH, a note, held keys, BEAT, DRUMS, BUSY, the pets"
run slice tools/emu/scripts/pm_slice.txt
for s in boot idle note hold beat drums busy dog llama monkey; do
    grep -q "^screenshot: $OUT/$s.ppm" "$OUT/slice.log" && [ -s "$OUT/$s.ppm" ] || bad "no screenshot $s.ppm"
done
sed -n '/^screenshot: .*hold.ppm/,/^screenshot: .*beat.ppm/p' "$OUT/slice.log" | grep 'pm: held' | tail -1 \
    | grep -q '(engine holds 0) phrase 0  synth voices 0 ' \
    && ok "slice: 6 s after the held keys' release: no note on, no synth voice" || bad "slice: a note outlived its keys"
[ "$(PM_EMU_MIDI=1 "$EMU" --headless --script tools/emu/scripts/pm_midi.txt --wav "$OUT/midi.wav" 2>&1 | tee "$OUT/midi.log" | grep -c '^expect .*: ok')" = 4 ] \
    && grep 'pm: held' "$OUT/midi.log" | tail -1 | grep -q 'notes-on 3 (engine holds 0) phrase 0  synth voices 0 drum voices 0' \
    && grep 'pm: drum' "$OUT/midi.log" | tail -1 | grep -q 'drum hits 2 ' \
    && ok "MIDI in: channel 1 plays the synth, channel 10 the kit, CC 123 lets go; nothing left sounding" \
    || bad "MIDI in (see $OUT/midi.log)"
mkdir -p "$OUT/rigs"
"$EMU" --headless --script tools/emu/scripts/pm_rigs.txt >"$OUT/rigs.log" 2>&1
# per pet, in the script's order: just arrived, at rest, one key held, three keys held, the beat alone, 22 s of nothing
got=$(grep '  pet: rig' "$OUT/rigs.log" | sed 's/.*rig \(-*[0-9]*\) face \([0-9]\) anim \([0-9]\).*/\1:\2:\3/' | tr '\n' ' ')
want=""
for r in 0 1 2 3; do want="$want$r:4:0 $r:N:0 $r:3:1 $r:2:2 $r:N:1 $r:5:0 "; done
[ "$(echo "$got" | sed 's/:[01]:\([01]\) /:N:\1 /g')" = "$want" ] \
    && ok "the four rigs: monkey, cat, dog, llama each on its own rig; surprised on arrival, neutral (or a blink) at rest, sing for a held note, happy and dancing for three, sleepy after 20 s" \
    || bad "rigs and faces: got $got"
quiet slice
cpu slice 70 tools/emu/scripts/pm_slice.txt

echo "every key, both modes, the beat off and on (pm_keys.txt)"
run keys "$OUT/pm_keys.txt"
n=$(grep -c '^expect sound .*: ok' "$OUT/keys.log")
[ "$n" = 108 ] && ok "108 presses sounded (27 keys x SYNTH, DRUMS x beat off, on)" || bad "$n of 108 presses sounded"
n=$(grep -c '^expect led .* on .*: ok' "$OUT/keys.log")
[ "$n" = 108 ] && ok "108 held keys lit their LED" || bad "$n of 108 held keys lit their LED"
quiet keys

echo "random playing for 60 s, then hands off (pm_mash.txt)"
run mash "$OUT/pm_mash.txt"
quiet mash
cpu mash 70 "$OUT/pm_mash.txt"
grep 'pm: mode' "$OUT/mash.log" | tail -1 | grep -q 'speed 0 busy 2 bounce 0 squish 0 sound \(0\|4\|8\|12\) tone 0 wobble 0 space 0 length 0 world [0-5]' \
    && ok "HOME: every knob back at its familiar setting" || bad "HOME left a knob turned"

echo "the heaviest load (pm_load.txt): FM6 voices at their cap with the phrase, the busiest beat, 13 drum keys repeating"
run load "$OUT/pm_load.txt"
grep -q 'synth voices [6-8] ' "$OUT/load.log" \
    && ok "the synth's voices were full: $(grep -o 'synth voices [0-9]*' "$OUT/load.log" | head -1) of 8" \
    || bad "the load script did not fill the synth's voices"
quiet load
cpu load 80 "$OUT/pm_load.txt"

echo "determinism"
"$EMU" --headless --script tools/emu/scripts/pm_slice.txt --wav "$OUT/again/slice.wav" --shot "$OUT/again/end.ppm" >/dev/null 2>&1
"$EMU" --headless --script tools/emu/scripts/pm_slice.txt --wav "$OUT/again/slice2.wav" --shot "$OUT/again/end2.ppm" >/dev/null 2>&1
cmp -s "$OUT/again/slice.wav" "$OUT/again/slice2.wav" && cmp -s "$OUT/slice.wav" "$OUT/again/slice.wav" \
    && ok "the same audio on every run" || bad "the audio differs between runs"
cmp -s "$OUT/again/end.ppm" "$OUT/again/end2.ppm" && ok "the same LCD on every run" || bad "the LCD differs between runs"

[ $fail -eq 0 ] && echo "PASS" || echo "FAIL"
exit $fail
