"""DHT11 reader for OPi5 — calls C helper or falls back to mock."""

import json
import os
import subprocess
import time


class Dht11Reader:
    """Read temperature/humidity from DHT11 via C helper binary."""

    def __init__(self):
        self._enabled = os.environ.get("DHT11_ENABLE", "0") == "1"
        self._gpio = int(os.environ.get("DHT11_GPIO", "148"))
        self._retries = int(os.environ.get("DHT11_READ_RETRIES", "3"))
        self._helper = os.environ.get("DHT11_HELPER", "")
        self._available = False
        self._last_ok = None
        self._last_error = None

        if not self._enabled:
            return

        # Auto-locate helper binary
        if not self._helper:
            base = os.path.dirname(os.path.abspath(__file__))
            candidates = [
                os.path.join(base, "..", "tools", "dht11_read"),
                os.path.join(base, "dht11_read"),
                "/opt/edge-ai-safety-monitor/tools/dht11_read",
            ]
            for c in candidates:
                if os.path.isfile(c) and os.access(c, os.X_OK):
                    self._helper = c
                    break

        if not self._helper or not os.path.isfile(self._helper):
            self._last_error = "dht11_read helper binary not found"
            print(f"[dht11] {self._last_error}")
            return

        # Verify helper works (dry run will timeout = expected without sensor)
        self._available = True
        print(f"[dht11] enabled gpio={self._gpio} helper={self._helper}")

    @property
    def enabled(self):
        return self._enabled

    @property
    def available(self):
        return self._available

    @property
    def last_error(self):
        return self._last_error

    @property
    def last_ok_at(self):
        return self._last_ok

    def read(self):
        """Read DHT11. Returns (temp_c, humidity_pct) or None on failure."""
        if not self._available:
            return None

        for attempt in range(self._retries):
            try:
                result = subprocess.run(
                    ["sudo", self._helper, str(self._gpio), "1"],
                    capture_output=True, text=True, timeout=10,
                )
                if result.returncode != 0:
                    self._last_error = result.stderr.strip() or f"exit code {result.returncode}"
                    continue

                line = result.stdout.strip()
                if not line:
                    self._last_error = "empty output"
                    continue

                data = json.loads(line)
                if data.get("error"):
                    self._last_error = data["error"]
                    continue

                temp = data.get("temperature_c")
                humid = data.get("humidity_pct")
                if temp is not None and humid is not None:
                    self._last_ok = time.time()
                    self._last_error = None
                    return float(temp), float(humid)

                self._last_error = "null values in response"
            except subprocess.TimeoutExpired:
                self._last_error = "read timeout (10s)"
            except json.JSONDecodeError as e:
                self._last_error = f"JSON parse error: {e}"
            except Exception as e:
                self._last_error = str(e)

            if attempt < self._retries - 1:
                time.sleep(2)  # DHT11 needs ≥2s between reads

        return None

    def get_env_detail(self):
        """Return diagnostics dict for sensors_source.env_detail."""
        return {
            "gpio": self._gpio if self._enabled else None,
            "enabled": self._enabled,
            "available": self._available,
            "last_error": self._last_error,
            "last_ok_at": self._last_ok,
        }
