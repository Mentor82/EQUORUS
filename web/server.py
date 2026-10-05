#!/usr/bin/env python3
"""
EQUORUS Studio - Live Multi-Runtime Conformance Server
Supports local Windows workstation and native L.I.A.R.A. OS (VM 107 / VM 108).
Zero external dependencies (Python standard library only).
"""

import argparse
from http.server import HTTPServer, SimpleHTTPRequestHandler
import json
import os
from pathlib import Path
import subprocess
import sys
import time

# Resolve base directories
SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
SYS_LIB_DIR = Path("/usr/share/equorus/web")

WEB_ROOT = SYS_LIB_DIR if SYS_LIB_DIR.exists() else SCRIPT_DIR

# Add Python reference library to sys.path
PY_LIB = REPO_ROOT / "python"
if PY_LIB.exists():
    sys.path.insert(0, str(PY_LIB))

try:
    from equorus import Envelope, Limits, ContractError, compute_integrity
    HAS_PY_REF = True
except ImportError:
    try:
        from equorus_reference import Envelope, Limits, ContractError
        from equorus_integrity import compute_integrity
        HAS_PY_REF = True
    except ImportError:
        try:
            from equorus.equorus_reference import Envelope, Limits, ContractError
            from equorus.equorus_integrity import compute_integrity
            HAS_PY_REF = True
        except ImportError:
            HAS_PY_REF = False

def find_binary(candidates):
    for c in candidates:
        if c and Path(c).is_file() and os.access(c, os.X_OK):
            return str(Path(c).resolve())
        # On Windows, check with .exe
        if sys.platform == "win32" and c and Path(str(c) + ".exe").is_file():
            return str(Path(str(c) + ".exe").resolve())
    return None

# Candidate binary paths across Windows dev & L.I.A.R.A. OS
CPP_BIN = find_binary([
    "/usr/bin/equorus-integrity",
    "/usr/local/bin/equorus-integrity",
    REPO_ROOT / "build-msvc" / "equorus_integrity_cli.exe",
    REPO_ROOT / "build" / "equorus_integrity_cli",
    Path("/srv/liara/build/equorus-m1.Xt9JhOFs/m2/build/equorus_integrity_cli")
])

RUST_BIN = find_binary([
    "/usr/bin/equorus-rust-integrity",
    REPO_ROOT / "rust" / "target" / "release" / "equorus_integrity",
    REPO_ROOT / "rust" / "target" / "debug" / "equorus_integrity",
    Path("/srv/liara/build/equorus/rust/target/release/equorus_integrity")
])

GO_BIN = find_binary([
    "/usr/bin/equorus-go-integrity",
    REPO_ROOT / "build-msvc" / "equorus_go_integrity.exe",
    REPO_ROOT / "go" / "equorus-integrity",
    Path("/srv/liara/build/equorus/go/equorus-integrity")
])

class EquorusStudioHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_ROOT), **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

    def do_GET(self):
        if self.path == "/favicon.ico":
            self.send_response(204)
            self.end_headers()
            return

        if self.path == "/api/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            status = {
                "server": "EQUORUS Studio Conformance Daemon",
                "version": "0.1.0",
                "os": sys.platform,
                "runtimes": {
                    "python": HAS_PY_REF,
                    "cpp": CPP_BIN is not None,
                    "rust": RUST_BIN is not None,
                    "go": GO_BIN is not None
                },
                "paths": {
                    "cpp": CPP_BIN,
                    "rust": RUST_BIN,
                    "go": GO_BIN
                }
            }
            self.wfile.write(json.dumps(status, indent=2).encode("utf-8"))
            return
        return super().do_GET()

    def do_POST(self):
        if self.path == "/api/compare":
            content_length = int(self.headers.get("Content-Length", 0))
            raw_body = self.rfile.read(content_length)
            try:
                body = raw_body.decode("utf-8")
            except UnicodeDecodeError:
                try:
                    body = raw_body.decode("latin-1")
                except Exception:
                    self._send_json(400, {"error": "Invalid character encoding in request body"})
                    return
            try:
                req = json.loads(body)
                if isinstance(req, dict):
                    if "raw" in req:
                        raw_json = req.get("raw", "")
                        type_id = req.get("type_id")
                    elif "type_id" in req and "schema_version" in req:
                        # Direct EQUORUS envelope posted without outer wrapper
                        raw_json = body
                        type_id = req.get("type_id")
                    else:
                        raw_json = req.get("raw", "")
                        type_id = req.get("type_id")
                else:
                    raw_json = body
                    type_id = None
            except Exception as e:
                self._send_json(400, {"error": f"Invalid request body: {e}"})
                return

            if isinstance(raw_json, dict) and "value" in raw_json and isinstance(raw_json["value"], str):
                raw_json = raw_json["value"]

            if isinstance(raw_json, (dict, list)):
                raw_bytes = json.dumps(raw_json).encode("utf-8")
                if not type_id and isinstance(raw_json, dict):
                    type_id = raw_json.get("type_id")
            elif isinstance(raw_json, str):
                raw_bytes = raw_json.encode("utf-8")
                if not type_id:
                    try:
                        p = json.loads(raw_json)
                        if isinstance(p, dict):
                            type_id = p.get("type_id")
                    except Exception:
                        pass
            else:
                raw_bytes = str(raw_json).encode("utf-8")

            if not type_id:
                type_id = "vinox.provenance.snapshot"

            res = self._execute_compare(raw_bytes, type_id)
            self._send_json(200, res)
            return

        self._send_json(404, {"error": "Not found"})

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def _send_json(self, status, obj):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(obj).encode("utf-8"))

    def _execute_compare(self, raw_bytes, type_id):
        print(f"DEBUG: type_id={type_id}, len={len(raw_bytes)}, head={raw_bytes[:60]}", flush=True)
        results = {}
        digests = []

        # 1. Python Reference
        if HAS_PY_REF:
            t0 = time.perf_counter()
            try:
                env = Envelope.decode(raw_bytes, type_id, supported_versions=("0.1",), limits=Limits())
                rec = compute_integrity(env)
                dt_us = int((time.perf_counter() - t0) * 1_000_000)
                results["python"] = {"status": "OK", "digest": rec.digest, "micros": dt_us}
                digests.append(rec.digest)
            except Exception as e:
                results["python"] = {"status": "ERROR", "error": str(e)}
        else:
            results["python"] = {"status": "UNAVAILABLE"}

        # 2. C++ Binary
        if CPP_BIN:
            t0 = time.perf_counter()
            try:
                proc = subprocess.run([CPP_BIN, "hash", type_id], input=raw_bytes, capture_output=True, timeout=5)
                dt_us = int((time.perf_counter() - t0) * 1_000_000)
                if proc.returncode == 0:
                    out_rec = json.loads(proc.stdout.decode("utf-8"))
                    dig = out_rec.get("digest")
                    results["cpp"] = {"status": "OK", "digest": dig, "micros": dt_us}
                    digests.append(dig)
                else:
                    results["cpp"] = {"status": "ERROR", "error": proc.stdout.decode("utf-8").strip()}
            except Exception as e:
                results["cpp"] = {"status": "ERROR", "error": str(e)}
        else:
            results["cpp"] = {"status": "UNAVAILABLE"}

        # 3. Rust Binary
        if RUST_BIN:
            t0 = time.perf_counter()
            try:
                proc = subprocess.run([RUST_BIN, "hash", type_id], input=raw_bytes, capture_output=True, timeout=5)
                dt_us = int((time.perf_counter() - t0) * 1_000_000)
                if proc.returncode == 0:
                    out_rec = json.loads(proc.stdout.decode("utf-8"))
                    dig = out_rec.get("digest")
                    results["rust"] = {"status": "OK", "digest": dig, "micros": dt_us}
                    digests.append(dig)
                else:
                    results["rust"] = {"status": "ERROR", "error": proc.stdout.decode("utf-8").strip()}
            except Exception as e:
                results["rust"] = {"status": "ERROR", "error": str(e)}
        else:
            results["rust"] = {"status": "UNAVAILABLE"}

        # 4. Go Binary
        if GO_BIN:
            t0 = time.perf_counter()
            try:
                proc = subprocess.run([GO_BIN, "hash", type_id], input=raw_bytes, capture_output=True, timeout=5)
                dt_us = int((time.perf_counter() - t0) * 1_000_000)
                if proc.returncode == 0:
                    out_rec = json.loads(proc.stdout.decode("utf-8"))
                    dig = out_rec.get("digest")
                    results["go"] = {"status": "OK", "digest": dig, "micros": dt_us}
                    digests.append(dig)
                else:
                    results["go"] = {"status": "ERROR", "error": proc.stdout.decode("utf-8").strip()}
            except Exception as e:
                results["go"] = {"status": "ERROR", "error": str(e)}
        else:
            results["go"] = {"status": "UNAVAILABLE"}

        # Check equivalence across all available runtimes
        unique_digests = set(digests)
        all_match = (len(unique_digests) == 1) if digests else False
        consensus_digest = digests[0] if digests else None

        return {
            "all_match": all_match,
            "consensus_digest": consensus_digest,
            "runtimes": results
        }

def main():
    parser = argparse.ArgumentParser(description="EQUORUS Studio Conformance Server")
    parser.add_argument("--host", default="0.0.0.0", help="Binding host address (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8088, help="Binding port (default: 8088)")
    args = parser.parse_args()

    server_address = (args.host, args.port)
    httpd = HTTPServer(server_address, EquorusStudioHandler)
    print(f"[*] EQUORUS Studio Server running on http://{args.host}:{args.port}")
    print(f"[*] Serving web assets from: {WEB_ROOT}")
    print(f"[*] Available runtimes: Python={HAS_PY_REF}, C++={bool(CPP_BIN)}, Rust={bool(RUST_BIN)}, Go={bool(GO_BIN)}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[*] Shutting down server.")

if __name__ == "__main__":
    main()
