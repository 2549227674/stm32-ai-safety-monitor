"""Sensor data: real safetyd + real MPU-6500 I2C + real DHT11 + mock fallback."""

import json
import math
import os
import random
import time

from mpu6500_reader import MPU6500Reader
from dht11_reader import Dht11Reader


class MockSensors:
    def __init__(self, safetyd_status_path):
        self.safetyd_path = safetyd_status_path
        self._t0 = time.time()
        self._mpu = MPU6500Reader()
        self._dht11 = Dht11Reader()
        self._dht11_cache = None
        self._dht11_last_read = 0
        self._dht11_interval = int(os.environ.get("DHT11_SAMPLE_INTERVAL_SEC", "3"))

    def sample(self):
        t = time.time() - self._t0
        safety, safety_source = self._read_safetyd()
        mpu_data, mpu_source = self._read_mpu6500(t)
        env_data, env_source = self._read_env(t)
        return {
            "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "risk_score": self._risk_score(t, safety),
            "sensors": {
                "mpu6500": mpu_data,
                "env": env_data,
                "safety": safety,
            },
            "sensors_source": {
                "safety": safety_source,
                "mpu6500": mpu_source,
                "env": env_source,
                "env_detail": self._dht11.get_env_detail() if self._dht11.enabled else None,
            },
            "device": {},
            "sensor_scores": self._sensor_scores(t, safety),
        }

    def _read_mpu6500(self, t):
        """Try real I2C read, fallback to mock."""
        if self._mpu.available:
            result = self._mpu.read()
            if result is not None:
                ax, ay, az, gx, gy, gz = result
                vib = abs(ax) + abs(ay) + abs(az - 1.0)
                return {
                    "accel_x": ax, "accel_y": ay, "accel_z": az,
                    "gyro_x": gx, "gyro_y": gy, "gyro_z": gz,
                    "vibration_score": round(min(vib, 10), 2),
                }, "real_i2c"
        # Fallback: mock sine wave
        base_x = 0.01 * math.sin(t * 0.5) + random.gauss(0, 0.005)
        base_y = 0.02 * math.cos(t * 0.3) + random.gauss(0, 0.005)
        base_z = 0.98 + 0.01 * math.sin(t * 0.1) + random.gauss(0, 0.003)
        vib = abs(base_x) + abs(base_y) + abs(base_z - 0.98)
        spike = random.random() < 0.02
        if spike:
            base_x += random.uniform(0.3, 0.8)
            vib += 2.0
        return {
            "accel_x": round(base_x, 4), "accel_y": round(base_y, 4),
            "accel_z": round(base_z, 4),
            "gyro_x": round(0.1 * math.sin(t * 0.7) + random.gauss(0, 0.05), 3),
            "gyro_y": round(0.05 * math.cos(t * 0.4) + random.gauss(0, 0.03), 3),
            "gyro_z": round(0.2 * math.sin(t * 0.2) + random.gauss(0, 0.04), 3),
            "vibration_score": round(min(vib, 10), 2),
        }, "mock"

    def _read_env(self, t):
        """Try real DHT11 read, fallback to mock."""
        now = time.time()
        if self._dht11.available and (now - self._dht11_last_read) >= self._dht11_interval:
            result = self._dht11.read()
            self._dht11_last_read = now
            if result is not None:
                temp, humid = result
                mock = self._env_mock(t)
                return {
                    "temp_c": round(temp, 1),
                    "humidity_pct": round(humid, 1),
                    "light_lux": mock["light_lux"],  # no real light sensor
                }, "real_dht11_gpio"
            # DHT11 enabled but read failed
            mock = self._env_mock(t)
            return mock, "dht11_error_fallback_mock"
        if self._dht11.enabled and self._dht11.last_error:
            # DHT11 enabled, interval not reached yet, use cache or mock
            mock = self._env_mock(t)
            return mock, "dht11_error_fallback_mock"
        # DHT11 not enabled — pure mock
        return self._env_mock(t), "mock"

    def _env_mock(self, t):
        return {
            "temp_c": round(25 + 2 * math.sin(t * 0.01) + random.gauss(0, 0.3), 1),
            "humidity_pct": round(50 + 5 * math.sin(t * 0.008) + random.gauss(0, 1), 1),
            "light_lux": int(300 + 100 * math.sin(t * 0.02) + random.gauss(0, 20)),
        }

    def _read_safetyd(self):
        try:
            with open(self.safetyd_path) as f:
                data = json.load(f)
                # Support two safetyd status file formats:
                #   New:  {"sensors": {"pir": 1, "flame": 0, "mq2": 0}}
                #   OPi5: {"last_pir": 1, "last_flame": 0, "last_mq2": 0}
                sensors = data.get("sensors", {})
                if sensors:
                    return {
                        "pir": sensors.get("pir", 0),
                        "flame": sensors.get("flame", 0),
                        "mq2": sensors.get("mq2", 0),
                    }, "safetyd_status_file"
                return {
                    "pir": data.get("last_pir", 0),
                    "flame": data.get("last_flame", 0),
                    "mq2": data.get("last_mq2", 0),
                }, "safetyd_status_file"
        except Exception:
            return {
                "pir": 1 if random.random() < 0.01 else 0,
                "flame": 0,
                "mq2": 0,
            }, "fallback_mock"

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
