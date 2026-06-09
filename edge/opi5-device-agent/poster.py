"""HTTP poster with local spool on failure."""

import json
import os
import sys
import requests
from pathlib import Path

SPOOL_DIR = Path(__file__).resolve().parent / "spool"


class BackendPoster:
    def __init__(self, backend_url, device_id):
        self.backend_url = backend_url.rstrip("/")
        self.device_id = device_id
        SPOOL_DIR.mkdir(exist_ok=True)

    def post(self, path, payload):
        url = f"{self.backend_url}{path}"
        try:
            resp = requests.post(url, json=payload, timeout=10)
            if resp.status_code < 400:
                self._flush_spool()
                return True
            print(f"[poster] {url} returned {resp.status_code}", file=sys.stderr)
        except Exception as e:
            print(f"[poster] {url} error: {e}", file=sys.stderr)
        self._spool(path, payload)
        return False

    def _spool(self, path, payload):
        import time
        fname = SPOOL_DIR / f"{int(time.time()*1000)}.json"
        with open(fname, "w") as f:
            json.dump({"path": path, "payload": payload}, f)

    def _flush_spool(self):
        for f in sorted(SPOOL_DIR.glob("*.json")):
            try:
                with open(f) as fh:
                    data = json.load(fh)
                url = f"{self.backend_url}{data['path']}"
                resp = requests.post(url, json=data["payload"], timeout=10)
                if resp.status_code < 400:
                    f.unlink()
            except Exception:
                pass
