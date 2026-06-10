#!/usr/bin/env python3
"""OPi5 Device Agent - video, telemetry, heartbeat, AI observation."""

import json
import os
import sys
import time
import threading
from datetime import datetime, timezone
from flask import Flask, jsonify, Response
import requests

from video import VideoCapture
from metrics import collect_metrics
from mock_sensors import MockSensors
from batcher import TelemetryBatcher
from ai_observer import AiObserver
from poster import BackendPoster

DEVICE_ID = os.environ.get("DEVICE_ID", "edge-opi5-001")
BACKEND_URL = os.environ.get("BACKEND_URL", "http://127.0.0.1:5000")
AI_URL = os.environ.get("AI_URL", "http://127.0.0.1:8080")
AGENT_PORT = int(os.environ.get("AGENT_PORT", "8090"))

HEARTBEAT_SEC = int(os.environ.get("HEARTBEAT_SEC", "5"))
TELEMETRY_SAMPLE_SEC = int(os.environ.get("TELEMETRY_SAMPLE_SEC", "1"))
TELEMETRY_BATCH_SEC = int(os.environ.get("TELEMETRY_BATCH_SEC", "30"))
AI_OBSERVATION_INTERVAL_SEC = int(os.environ.get("AI_OBSERVATION_INTERVAL_SEC", "30"))

SAFETYD_STATUS_PATH = os.environ.get("SAFETYD_STATUS_PATH", "/opt/edge-ai-safety-monitor/run/opi5_safetyd_status.json")

app = Flask(__name__)
video = VideoCapture()
sensors = MockSensors(SAFETYD_STATUS_PATH)
batcher = TelemetryBatcher(TELEMETRY_BATCH_SEC, DEVICE_ID)
poster = BackendPoster(BACKEND_URL, DEVICE_ID)
ai_observer = AiObserver(AI_URL, BACKEND_URL, DEVICE_ID, AI_OBSERVATION_INTERVAL_SEC)

latest_metrics = {}
metrics_lock = threading.Lock()


def heartbeat_loop():
    while True:
        try:
            services = {"device_agent": "running"}
            for svc in ("opi5-ai-qwen3vl", "opi5-safetyd"):
                try:
                    r = os.popen(f"systemctl is-active {svc} 2>/dev/null").read().strip()
                    services[svc] = r or "unknown"
                except Exception:
                    services[svc] = "unknown"

            health = collect_metrics()
            ai_health = {}
            try:
                resp = requests.get(f"{AI_URL}/health", timeout=3)
                ai_health = resp.json()
            except Exception:
                ai_health = {"ok": False, "error": "unreachable"}

            cam = video.get_status()
            agent_ip = _get_ip()
            agent_url = f"http://{agent_ip}:{AGENT_PORT}" if agent_ip not in ("unknown", "") else None

            payload = {
                "device_id": DEVICE_ID,
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "agent_version": "0.2.0",
                "ip": agent_ip,
                "agent_url": agent_url,
                "agent_port": AGENT_PORT,
                "uptime_s": _get_uptime(),
                "online": True,
                "services": services,
                "health": health,
                "ai": ai_health,
                "camera": cam,
                "camera_status": cam["status"],
                "video_mode": cam["mode"],
                "video_available": cam["available"],
            }
            poster.post("/api/devices/heartbeat", payload)
        except Exception as e:
            print(f"[heartbeat] error: {e}", file=sys.stderr)
        time.sleep(HEARTBEAT_SEC)


def telemetry_loop():
    global latest_metrics
    while True:
        try:
            m = sensors.sample()
            device_metrics = collect_metrics()
            m["device"] = device_metrics
            with metrics_lock:
                latest_metrics = m
            batcher.add_sample(m)
            if batcher.should_flush():
                batch = batcher.flush()
                poster.post("/api/telemetry/batch", batch)
        except Exception as e:
            print(f"[telemetry] error: {e}", file=sys.stderr)
        time.sleep(TELEMETRY_SAMPLE_SEC)


def ai_loop():
    while True:
        try:
            snapshot = video.get_snapshot()
            summary = batcher.get_summary()
            ai_observer.observe(snapshot, summary)
        except Exception as e:
            print(f"[ai] error: {e}", file=sys.stderr)
        time.sleep(AI_OBSERVATION_INTERVAL_SEC)


@app.get("/health")
def health():
    return jsonify({"ok": True, "service": "opi5-device-agent", "device_id": DEVICE_ID})


@app.get("/api/status")
def status():
    with metrics_lock:
        m = dict(latest_metrics)
    cam = video.get_status()
    return jsonify({
        "ok": True,
        "device_id": DEVICE_ID,
        "ip": _get_ip(),
        "metrics": m,
        "video_available": video.is_available(),
        "camera": cam,
        "camera_status": cam["status"],
        "video_mode": cam["mode"],
        "ai_url": AI_URL,
        "backend_url": BACKEND_URL,
    })


@app.get("/api/metrics/current")
def metrics_current():
    with metrics_lock:
        m = dict(latest_metrics)
    return jsonify({"ok": True, "device_id": DEVICE_ID, "metrics": m})


@app.get("/api/video/stream")
def video_stream():
    def generate():
        while True:
            frame = video.wait_frame(timeout=2.0)
            if frame is None:
                continue
            yield (b"--frame\r\n"
                   b"Content-Type: image/jpeg\r\n\r\n" + frame + b"\r\n")
    return Response(generate(), mimetype="multipart/x-mixed-replace; boundary=frame")


@app.get("/api/video/snapshot.jpg")
def video_snapshot():
    frame = video.get_snapshot()
    if frame is None:
        return jsonify({"ok": False, "error": "no frame available"}), 503
    return Response(frame, mimetype="image/jpeg")


@app.get("/api/ai/latest")
def ai_latest():
    obs = ai_observer.get_latest()
    return jsonify({"ok": True, "device_id": DEVICE_ID, "observation": obs})


def _get_ip():
    import socket
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "unknown"


def _get_uptime():
    try:
        with open("/proc/uptime") as f:
            return float(f.read().split()[0])
    except Exception:
        return 0


def main():
    print(f"[agent] starting device-agent for {DEVICE_ID}")
    print(f"[agent] backend={BACKEND_URL} ai={AI_URL} port={AGENT_PORT}")

    for t in [heartbeat_loop, telemetry_loop, ai_loop]:
        th = threading.Thread(target=t, daemon=True)
        th.start()

    app.run(host="0.0.0.0", port=AGENT_PORT, threaded=True)


if __name__ == "__main__":
    main()
