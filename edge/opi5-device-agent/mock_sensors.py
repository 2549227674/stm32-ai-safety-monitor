"""Mock sensor data generator for demo/display purposes."""

import json
import math
import os
import random
import time


class MockSensors:
    def __init__(self, safetyd_status_path):
        self.safetyd_path = safetyd_status_path
        self._t0 = time.time()

    def sample(self):
        t = time.time() - self._t0
        safety = self._read_safetyd()
        return {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "risk_score": self._risk_score(t, safety),
            "sensors": {
                "mpu6500": self._mpu6500(t),
                "env": self._env(t),
                "safety": safety,
            },
            "device": {},
            "sensor_scores": self._sensor_scores(t, safety),
        }

    def _mpu6500(self, t):
        base_x = 0.01 * math.sin(t * 0.5) + random.gauss(0, 0.005)
        base_y = 0.02 * math.cos(t * 0.3) + random.gauss(0, 0.005)
        base_z = 0.98 + 0.01 * math.sin(t * 0.1) + random.gauss(0, 0.003)
        vib = abs(base_x) + abs(base_y) + abs(base_z - 0.98)
        spike = random.random() < 0.02
        if spike:
            base_x += random.uniform(0.3, 0.8)
            vib += 2.0
        return {
            "accel_x": round(base_x, 4),
            "accel_y": round(base_y, 4),
            "accel_z": round(base_z, 4),
            "gyro_x": round(0.1 * math.sin(t * 0.7) + random.gauss(0, 0.05), 3),
            "gyro_y": round(0.05 * math.cos(t * 0.4) + random.gauss(0, 0.03), 3),
            "gyro_z": round(0.2 * math.sin(t * 0.2) + random.gauss(0, 0.04), 3),
            "vibration_score": round(min(vib, 10), 2),
        }

    def _env(self, t):
        return {
            "temp_c": round(25 + 2 * math.sin(t * 0.01) + random.gauss(0, 0.3), 1),
            "humidity_pct": round(50 + 5 * math.sin(t * 0.008) + random.gauss(0, 1), 1),
            "light_lux": int(300 + 100 * math.sin(t * 0.02) + random.gauss(0, 20)),
        }

    def _read_safetyd(self):
        try:
            with open(self.safetyd_path) as f:
                data = json.load(f)
                sensors = data.get("sensors", {})
                return {
                    "pir": sensors.get("pir", 0),
                    "flame": sensors.get("flame", 0),
                    "mq2": sensors.get("mq2", 0),
                }
        except Exception:
            return {
                "pir": 1 if random.random() < 0.01 else 0,
                "flame": 0,
                "mq2": 0,
            }

    def _risk_score(self, t, safety):
        base = 1.0 + 0.5 * math.sin(t * 0.03)
        if safety.get("flame"):
            base += 5
        if safety.get("mq2"):
            base += 4
        if safety.get("pir"):
            base += 1
        return round(min(max(base + random.gauss(0, 0.3), 0), 10), 1)

    def _sensor_scores(self, t, safety):
        return {
            "mpu6500_vibration": round(min(abs(0.01 * math.sin(t * 0.5)) + random.gauss(0, 0.1) + 0.5, 10), 2),
            "cpu_temp": round(min(2.0 + random.gauss(0, 0.5), 10), 2),
            "smoke": round(4.0 if safety.get("mq2") else random.gauss(0, 0.2), 2),
            "flame": round(6.0 if safety.get("flame") else random.gauss(0, 0.1), 2),
        }
