#!/usr/bin/env python3
"""Seed Flask backend with mock data for frontend preview."""
import random, requests
from datetime import datetime, timezone, timedelta

BASE = "http://localhost:5000"
DEVICE_ID = "edge-opi5-001"

def seed():
    print("Seeding mock data to", BASE)

    # 1. Device heartbeat (with camera/video fields)
    agent_ip = "192.168.1.100"
    agent_port = 8090
    hb = {
        "device_id": DEVICE_ID,
        "agent_version": "0.2.0",
        "contract_version": "1.1",
        "ip": agent_ip,
        "agent_url": f"http://{agent_ip}:{agent_port}",
        "agent_port": agent_port,
        "uptime_s": 86400 * 3 + 7240,
        "online": True,
        "services": {
            "device_agent": "running",
            "opi5-ai-qwen3vl": "running",
            "opi5-safetyd": "running",
        },
        "health": {
            "cpu_temp_c": 52.3,
            "mem_used_mb": 5450,
            "mem_total_mb": 15876,
            "cpu_load_1m": 1.25,
            "disk_used_pct": 41.2,
        },
        "ai": {
            "ok": True,
            "mode": "qwen3vl",
            "model_ready": True,
            "error": None,
        },
        "camera": {
            "status": "offline",
            "mode": "offline",
            "available": False,
            "device": None,
            "width": 1280,
            "height": 720,
            "fps": 12,
            "mock": True,
        },
        "camera_status": "offline",
        "video_mode": "offline",
        "video_available": False,
    }
    r = requests.post(f"{BASE}/api/devices/heartbeat", json=hb)
    print(f"  heartbeat: {r.status_code}")

    # 2. Telemetry batch (30 samples, 1 per second over past 30s)
    now = datetime.now(timezone.utc)
    samples = []
    for i in range(30):
        ts = (now - timedelta(seconds=29 - i)).replace(microsecond=0).isoformat()
        samples.append({
            "ts": ts,
            "risk_score": round(random.uniform(1.0, 3.0), 1),
            "device": {
                "cpu_temp_c": round(random.uniform(48, 56), 1),
                "mem_used_mb": round(random.uniform(5200, 5800)),
                "mem_total_mb": 15876,
                "cpu_load_1m": round(random.uniform(0.8, 2.0), 2),
                "disk_used_pct": round(random.uniform(40, 42), 1),
            },
            "sensors": {
                "mpu6500": {
                    "accel_x": round(random.uniform(-0.02, 0.02), 4),
                    "accel_y": round(random.uniform(-0.02, 0.02), 4),
                    "accel_z": round(random.uniform(0.96, 1.0), 4),
                    "gyro_x": round(random.uniform(-0.1, 0.1), 4),
                    "gyro_y": round(random.uniform(-0.1, 0.1), 4),
                    "gyro_z": round(random.uniform(-0.1, 0.1), 4),
                    "vibration_score": round(random.uniform(0.3, 2.0), 2),
                },
                "env": {
                    "temp_c": round(random.uniform(23, 27), 1),
                    "humidity_pct": round(random.uniform(45, 55), 1),
                    "light_lux": int(random.uniform(250, 350)),
                },
                "safety": {
                    "pir": 1 if random.random() < 0.05 else 0,
                    "flame": 0,
                    "mq2": 0,
                },
            },
            "sensor_scores": {
                "smoke": round(random.uniform(0, 0.5), 2),
                "flame": round(random.uniform(0, 0.2), 2),
                "mpu6500_vibration": round(random.uniform(0.3, 2.0), 2),
                "cpu_temp": round(random.uniform(1, 3), 2),
            },
        })
    batch = {
        "device_id": DEVICE_ID,
        "window": {
            "start": (now - timedelta(seconds=29)).replace(microsecond=0).isoformat(),
            "end": now.replace(microsecond=0).isoformat(),
            "sample_count": len(samples),
            "sample_interval_ms": 1000,
        },
        "samples": samples,
        "summary": {
            "risk_score": {
                "min": round(min(s["risk_score"] for s in samples), 1),
                "avg": round(sum(s["risk_score"] for s in samples) / len(samples), 2),
                "max": round(max(s["risk_score"] for s in samples), 1),
                "latest": samples[-1]["risk_score"],
            },
            "cpu_temp_c": {
                "min": round(min(s["device"]["cpu_temp_c"] for s in samples), 1),
                "avg": round(sum(s["device"]["cpu_temp_c"] for s in samples) / len(samples), 1),
                "max": round(max(s["device"]["cpu_temp_c"] for s in samples), 1),
                "latest": samples[-1]["device"]["cpu_temp_c"],
            },
            "mpu6500_vibration": {
                "min": round(min(s["sensors"]["mpu6500"]["vibration_score"] for s in samples), 2),
                "avg": round(sum(s["sensors"]["mpu6500"]["vibration_score"] for s in samples) / len(samples), 2),
                "max": round(max(s["sensors"]["mpu6500"]["vibration_score"] for s in samples), 2),
                "latest": samples[-1]["sensors"]["mpu6500"]["vibration_score"],
            },
        },
    }
    r = requests.post(f"{BASE}/api/telemetry/batch", json=batch)
    print(f"  telemetry: {r.status_code} ({len(samples)} samples)")

    # 3. AI observation
    obs = {
        "device_id": DEVICE_ID,
        "timestamp": now.isoformat(),
        "status": "ok",
        "risk_hint": 2,
        "summary": "画面为室内巡检区域，未发现人员、烟雾或火焰，整体无异常。",
        "full_text": "根据当前摄像头画面与过去 30 秒遥测摘要：画面中为固定巡检区域，可见设备机柜与走线，无遮挡。未检测到人员驻留、烟雾扩散或火焰高亮区域。震动分数与环境温湿度均处于正常区间，设备 CPU 温度稳定。综合判断风险等级为低。",
        "labels": [],
        "run_metrics": {
            "ttft_ms": 4200,
            "imgenc_ms": 2400,
            "llm_prefill_ms": 1800,
            "decode_ms": 5500,
            "total_ms": 9900,
            "tokens_out": 85,
            "tokens_per_s": 10.8,
            "mem_rss_mb": 3150,
        },
        "control_allowed": False,
        "model": {"name": "qwen3-vl-2b", "backend": "rknn-llm", "mode": "worker"},
    }
    r = requests.post(f"{BASE}/api/ai/observations", json=obs)
    print(f"  ai observation: {r.status_code}")

    print("Done! Open http://localhost:5000/ to see the console.")
    print("Note: video stream requires a running opi5-device-agent (mock or real).")
    print("  Start mock: VIDEO_MOCK=1 python3 edge/opi5-device-agent/app.py")

if __name__ == "__main__":
    seed()
