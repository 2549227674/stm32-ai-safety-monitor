"""Telemetry batcher - accumulate samples and flush periodically."""

import time
from datetime import datetime, timezone


class TelemetryBatcher:
    def __init__(self, batch_interval_sec, device_id="edge-opi5-001"):
        self.interval = batch_interval_sec
        self.device_id = device_id
        self._samples = []
        self._window_start = None
        self._last_flush = time.time()

    def add_sample(self, sample):
        if not self._window_start:
            self._window_start = sample.get("ts", datetime.now(timezone.utc).isoformat())
        self._samples.append(sample)

    def should_flush(self):
        return (time.time() - self._last_flush) >= self.interval

    def flush(self):
        now = datetime.now(timezone.utc).isoformat()
        batch = {
            "device_id": self.device_id,
            "window": {
                "start": self._window_start or now,
                "end": now,
                "sample_count": len(self._samples),
                "sample_interval_ms": 1000,
            },
            "samples": list(self._samples),
            "summary": self._compute_summary(),
        }
        self._samples = []
        self._window_start = None
        self._last_flush = time.time()
        return batch

    def get_summary(self):
        return self._compute_summary()

    def _compute_summary(self):
        if not self._samples:
            return {}
        risk_scores = [s.get("risk_score", 0) for s in self._samples if s.get("risk_score") is not None]
        temps = [s.get("device", {}).get("cpu_temp_c") for s in self._samples if s.get("device", {}).get("cpu_temp_c") is not None]
        vibes = [s.get("sensors", {}).get("mpu6500", {}).get("vibration_score") for s in self._samples if s.get("sensors", {}).get("mpu6500", {}).get("vibration_score") is not None]
        summary = {}
        if risk_scores:
            summary["risk_score"] = {"min": min(risk_scores), "avg": round(sum(risk_scores)/len(risk_scores), 2), "max": max(risk_scores), "latest": risk_scores[-1]}
        if temps:
            summary["cpu_temp_c"] = {"min": min(temps), "avg": round(sum(temps)/len(temps), 1), "max": max(temps), "latest": temps[-1]}
        if vibes:
            summary["mpu6500_vibration"] = {"min": min(vibes), "avg": round(sum(vibes)/len(vibes), 2), "max": max(vibes), "latest": vibes[-1]}
        return summary
