#!/usr/bin/env python3
"""Seed Flask backend with mock data for frontend preview."""
import time, random, requests

BASE = "http://localhost:5000"
DEVICE_ID = "edge-opi5-001"

def seed():
    print("Seeding mock data to", BASE)

    # 1. Device heartbeat
    hb = {
        "device_id": DEVICE_ID,
        "agent_version": "0.2.0",
        "contract_version": "1.1",
        "ip": "192.168.1.100",
        "uptime_s": 86400 * 3 + 7240,
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
        "camera": "online",
    }
    r = requests.post(f"{BASE}/api/devices/heartbeat", json=hb)
    print(f"  heartbeat: {r.status_code}")

    # 2. Telemetry batch (nested structure matching insert_telemetry_batch)
    now_iso = __import__("datetime").datetime.now(__import__("datetime").timezone.utc).isoformat()
    samples = []
    for i in range(30):
        samples.append({
            "ts": now_iso,
            "risk_score": round(random.uniform(1.0, 3.0), 1),
            "device": {
                "cpu_temp_c": round(random.uniform(48, 56), 1),
                "mem_used_mb": round(random.uniform(5200, 5800)),
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
                "safety": {
                    "pir": 1 if random.random() < 0.05 else 0,
                    "flame": 0,
                    "mq2": 0,
                },
            },
            "sensor_scores": {
                "smoke_score": round(random.uniform(0, 0.5), 2),
            },
        })
    batch = {
        "device_id": DEVICE_ID,
        "window": {"start": now_iso, "end": now_iso},
        "samples": samples,
        "summary": {
            "risk_score": {"latest": samples[-1]["risk_score"]},
        },
    }
    r = requests.post(f"{BASE}/api/telemetry/batch", json=batch)
    print(f"  telemetry: {r.status_code} ({len(samples)} samples)")

    # 3. AI observation
    obs = {
        "device_id": DEVICE_ID,
        "timestamp": now_iso,
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

if __name__ == "__main__":
    seed()
