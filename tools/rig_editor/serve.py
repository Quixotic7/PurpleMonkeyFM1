#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PurpleMonkey FM-1 contributors
"""The pet animation editor's server: a page for keyframing the rigs by hand.

  python3 tools/rig_editor/serve.py [PORT]        then open http://127.0.0.1:8877/

Serves tools/rig_editor/index.html and, read-only, the rig art under assets/purplemonkey/. The one thing it
writes is a set's moves.json (POST /api/save/SET): PurpleMonkey's choreography, which tools/gen_pm_rig.py reads at
the next build (sh tools/emu/build_pm.sh). The first save of a session copies the file to moves.json.bak first.
Listens on 127.0.0.1 only. No dependencies beyond Python 3.
"""
import json
import shutil
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
ART = ROOT / "assets" / "purplemonkey"
HERE = Path(__file__).resolve().parent
PETS = ["monkey", "cat", "dog", "llama"]
TYPES = {".html": "text/html; charset=utf-8", ".json": "application/json", ".png": "image/png", ".js": "text/javascript"}
backed_up = set()


def sets():
    """the art sets that hold rigs, newest first as tools/gen_pm_rig.py takes them"""
    order = ["rig-64-modular", "rig-64", "rig"]
    found = [d.name for d in ART.iterdir() if d.is_dir() and any((d / p / "rig.json").exists() for p in PETS)]
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
            f = (ART / path[5:]).resolve()
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
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8877
    print(f"pet animation editor: http://127.0.0.1:{port}/   (sets: {', '.join(sets())})", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()
