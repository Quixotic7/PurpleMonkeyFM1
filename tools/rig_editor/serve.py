#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""The pet animation editor's server: a page for keyframing the rigs by hand.

  python3 tools/rig_editor/serve.py [PORT]        then open http://127.0.0.1:8877/   (or $PORT)

Serves tools/rig_editor/index.html and, read-only, the rig art under assets/purplemonkey/. The one thing it
writes is a set's moves.json (POST /api/save/SET): PurpleMonkey's choreography, which tools/gen_pm_rig.py reads at
the next build (sh tools/emu/build_pm.sh). The first save of a session copies the file to moves.json.bak first.
Listens on 127.0.0.1 only. No dependencies beyond Python 3.
"""
import json
import os
import shutil
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
ART = ROOT / "assets" / "purplemonkey"
HERE = Path(__file__).resolve().parent
PETS = ["monkey", "cat", "dog", "llama"]
# a pet's own art revision shown in place of a set's (tools/gen_pm_rig.py OVERRIDES: the build takes the same): the
# choreography stays the set's moves.json, so the editor shows the revision's art under the set and hides the folder
OVERRIDES = {("rig-64-modular", "cat"): ART / "rig-64-cat-v4" / "cat"}
TYPES = {".html": "text/html; charset=utf-8", ".json": "application/json", ".png": "image/png", ".js": "text/javascript"}
backed_up = set()


def sets():
    """the art sets that hold rigs, newest first as tools/gen_pm_rig.py takes them"""
    order = ["rig-64-modular", "rig-64", "rig"]
    hidden = {o.parent.name for o in OVERRIDES.values()}
    found = [d.name for d in ART.iterdir() if d.is_dir() and d.name not in hidden and any((d / p / "rig.json").exists() for p in PETS)]
    return [s for s in order if s in found] + sorted(s for s in found if s not in order)


class H(BaseHTTPRequestHandler):
    def send(self, code, body, ctype="application/json"):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path in ("/", "/index.html"):
            return self.send(200, (HERE / "index.html").read_bytes(), TYPES[".html"])
        if path == "/api/sets":
            out = {}
            for s in sets():
                mv = ART / s / "moves.json"
                out[s] = {"pets": [p for p in PETS if (ART / s / p / "rig.json").exists()],
                          "moves": json.loads(mv.read_text()) if mv.exists() else {}}
            return self.send(200, json.dumps(out))
        if path.startswith("/art/"):
            parts = path[5:].split("/")
            base = OVERRIDES.get((parts[0], parts[1]) if len(parts) > 2 else None)
            f = ((base / "/".join(parts[2:])) if base else (ART / path[5:])).resolve()
            if ART in f.parents and f.is_file() and f.suffix in (".png", ".json"):
                return self.send(200, f.read_bytes(), TYPES[f.suffix])
        self.send(404, '{"error": "not found"}')

    def do_POST(self):
        if not self.path.startswith("/api/save/"):
            return self.send(404, '{"error": "not found"}')
        name = self.path[len("/api/save/"):]
        if name not in sets():
            return self.send(400, '{"error": "no such set"}')
        try:
            moves = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0"))))
            assert isinstance(moves, dict)
            for pet, v in moves.items():
                if pet in PETS:
                    for an, a in v.items():
                        assert isinstance(a["beats"], (int, float)) and a["keys"], f"{pet} {an}"
                        for k in a["keys"]:
                            assert k["ms"] > 0 and len(k["root"]) == 2 and isinstance(k["a"], dict), f"{pet} {an}"
                            assert all(len(o) == 2 for o in k.get("feet", {}).values()), f"{pet} {an}: feet"
                            assert all(len(o) == 2 for o in k.get("t", {}).values()), f"{pet} {an}: t"
            for pet, f in moves.get("faces", {}).items():
                for e, a in f.items():
                    assert a["keys"] and all(k["ms"] >= 20 and isinstance(k.get("layers", {}), dict) for k in a["keys"]), f"{pet} face {e}"
            for pet, r in moves.get("reactions", {}).items():
                for n in ("note_l", "note_r", "snare"):
                    assert isinstance(r[n].get("a", {}), dict) and abs(r[n].get("dip", 0)) <= 127, f"{pet} reaction {n}"
                    assert all(len(o) == 2 and max(map(abs, o)) <= 127 for o in r[n].get("t", {}).values()), f"{pet} reaction {n}: t"
                    assert all(0 <= r[n].get(x, 0) <= 5000 for x in ("rise", "hold", "fade")), f"{pet} reaction {n}: times"
                bob = r.get("bob", {})
                if "beat" in bob:                 # the older shape
                    assert all(abs(bob[x]) <= 90 for x in ("beat", "bar")), f"{pet} bob"
                else:
                    for an, b in bob.items():
                        assert an in ("idle", "play", "dance") and all(len(b[k]) == 2 and max(map(abs, b[k])) <= 127 for k in ("head", "neck", "x", "y")), f"{pet} bob {an}"
        except Exception as e:                    # nothing is written unless the whole file is sound
            return self.send(400, json.dumps({"error": f"not saved: {e}"}))
        f = ART / name / "moves.json"
        if f.exists() and name not in backed_up:
            shutil.copyfile(f, f.with_suffix(".json.bak"))
        backed_up.add(name)
        f.write_text(json.dumps(moves, indent=1) + "\n")
        self.send(200, json.dumps({"saved": str(f.relative_to(ROOT))}))

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else int(os.environ.get("PORT", "8877"))
    print(f"pet animation editor: http://127.0.0.1:{port}/   (sets: {', '.join(sets())})", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()
