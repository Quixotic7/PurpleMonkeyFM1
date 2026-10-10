// SPDX-License-Identifier: GPL-3.0-only
// PurpleMonkey FM-1 in the browser: what differs from keymap.js (loaded before this). The computer keys are the
// same; the stickers are PurpleMonkey's (firmware/src/pm_ui.c: FX = DRUMS, SEL = SYNTH, ENV = KEYS, LFO = TUNE,
// EDIT = TALK, PLAY = BEAT; the buttons with no role do nothing), there is no chord block, nothing is stored.
Object.assign(globalThis.FM1, {
  BTN_ROLE: ["DRUMS", "SYNTH", "KEYS", "TUNE", "TALK", "", "HOME", "", "", "", "BEAT", "", "OCT\u2212", "OCT+"],
  ENC_ROLE: ["PET", "WORLD", "SOUND", "TONE\u00b7SPEED", "WOBBLE\u00b7BUSY", "SPACE\u00b7BOUNCE", "LENGTH\u00b7SQUISH", "VOLUME"],
  CHORD: {},
  APP: { wasm: "purplemonkey.wasm", db: "purplemonkey-fm1", block: "", running: "Press any key. Turn PET for another friend." },
});
