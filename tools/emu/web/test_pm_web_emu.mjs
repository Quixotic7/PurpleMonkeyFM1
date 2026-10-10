// SPDX-License-Identifier: GPL-3.0-only
// Headless checks of PurpleMonkey's browser emulator (node >= 20, no browser):
//   node tools/emu/web/test_pm_web_emu.mjs
// build/emu-web-pm/purplemonkey.wasm instantiated with the worklet's stub imports, driven through the exports
// worklet.js uses: it boots and reports its version, draws, lights a pressed key and sounds it, says a letter in
// TALK, changes pet, runs faster than real time, and the page's files are all there.
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.resolve(HERE, "../../../build/emu-web-pm");
await import(path.join(HERE, "keymap.js"));
await import(path.join(HERE, "pm_keymap.js"));
const { BTN, ENC, APP } = globalThis.FM1;

let fails = 0;
const ok = (c, msg) => { console.log(`  ${c ? "ok  " : "FAIL"}  ${msg}`); if (!c) fails++; return c; };

let mem;
const wasi = {
  clock_time_get: (id, prec, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; },
  fd_write: (fd, iov, n, out) => {
    const dv = new DataView(mem.buffer);
    let len = 0;
    for (let i = 0; i < n; i++) len += dv.getUint32(iov + 8 * i + 4, true);
    dv.setUint32(out, len, true);
    return 0;
  },
  proc_exit: () => {},
};
const stub = new Proxy(wasi, { get: (t, k) => (k in t ? t[k] : () => 0) });
const env = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(fs.readFileSync(path.join(OUT, APP.wasm)), { wasi_snapshot_preview1: stub, env });
const ex = instance.exports;
mem = ex.memory;
if (ex._initialize) ex._initialize();

// n ms of the device; -> the peak of the audio it made
function run(ms) {
  let peak = 0;
  for (let t = 0; t < ms; t += 10) {
    ex.web_step(10);
    for (let n; (n = Math.min(ex.web_avail(), 1024)) > 0; ) {
      ex.web_render(n);
      for (const v of new Float32Array(mem.buffer, ex.web_out_l(), n)) peak = Math.max(peak, Math.abs(v));
    }
  }
  return peak;
}
const btn = (name) => 1 << BTN.indexOf(name);
const tapBtn = (name) => { ex.web_buttons(btn(name)); run(60); ex.web_buttons(0); run(60); };
const led = (i) => new Uint8Array(mem.buffer, ex.web_leds(), 42)[i];
const fbSum = () => new Uint16Array(mem.buffer, ex.web_fb(), 240 * 240).reduce((a, v) => (a + v) >>> 0, 0);

ex.web_master(1023);
ex.web_boot();
const vb = new Uint8Array(mem.buffer, ex.web_version(), 64);
const version = String.fromCharCode(...vb.subarray(0, vb.indexOf(0)));
ok(/^PurpleMonkey /.test(version), `it reports "${version}"`);
const t0 = performance.now();
const boot = run(3000);                               // power-on: the pet says its name
const speed = 3000 / (performance.now() - t0);
ok(ex.web_lcd_writes() > 0 && fbSum() !== 0, `the screen is drawn (${ex.web_lcd_writes()} LCD writes)`);
ok(boot > 0.01, `power-on is heard: the pet's name (peak ${boot.toFixed(3)})`);
ok(speed > 2, `3 s of the device in ${(3000 / speed).toFixed(0)} ms (${speed.toFixed(0)} x real time)`);
run(2000);
ok(led(27 + BTN.indexOf("SEL")) === 2, "SYNTH's LED (SEL) is lit");

ex.web_keys(1 << 7);                                  // C4
const note = run(200);
ok(led(7) === 2, "a pressed key's LED is lit");
ex.web_keys(0);
ok(note > 0.01, `SYNTH: a key is a note (peak ${note.toFixed(3)})`);
run(4000);

tapBtn("EDIT");
ok(led(27 + BTN.indexOf("EDIT")) === 2, "TALK's LED (EDIT) is lit");
run(1500);
ex.web_keys(1 << 0);
run(100);
ex.web_keys(0);
const said = run(500);
ok(said > 0.05, `TALK: a key says its letter (peak ${said.toFixed(3)})`);
run(2500);
const quiet = run(500);
ok(quiet < 0.001, `and then it is quiet (peak ${quiet.toFixed(5)})`);

const before = fbSum();
ex.web_enc(ENC.indexOf("SELECT"), 1);
const name = run(1500);
ok(fbSum() !== before, "PET turned: the screen changed");
ok(name > 0.05, `the new pet says its name (peak ${name.toFixed(3)})`);

tapBtn("FX");
ex.web_keys(1 << 12);
const hit = run(150);
ex.web_keys(0);
ok(led(27 + BTN.indexOf("FX")) === 2 && hit > 0.01, `DRUMS: a key is a hit (peak ${hit.toFixed(3)})`);

for (const f of ["index.html", "emu.js", "keymap.js", "pm_keymap.js", "worklet.js", APP.wasm])
  ok(fs.existsSync(path.join(OUT, f)), `build/emu-web-pm/${f}`);
const page = fs.readFileSync(path.join(OUT, "index.html"), "utf8");
ok(/PurpleMonkey/.test(page) && !/ChoralRoot/.test(page), "the page is PurpleMonkey's");

console.log(fails ? `FAIL: ${fails}` : "PASS: PurpleMonkey's browser emulator");
process.exit(fails ? 1 : 0);
