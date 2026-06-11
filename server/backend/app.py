import json
import os
import time
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

from flask import Flask, jsonify, render_template, request, send_from_directory
from flask_cors import CORS
from werkzeug.exceptions import RequestEntityTooLarge
from werkzeug.utils import secure_filename

from database import (
    init_db, insert_event, insert_image, list_events, get_latest_event,
    upsert_device_status, get_device_status, insert_telemetry_batch,
    get_telemetry_series, insert_ai_observation, get_latest_ai_observation,
    list_ai_observations,
    get_thresholds, upsert_threshold, insert_alert, list_alerts,
    get_connection,
)
from email_notifier import EmailNotifier


BASE_DIR = Path(__file__).resolve().parent
UPLOAD_DIR = BASE_DIR / "uploads"
VERSION = "0.1.0"
MAX_LIMIT = 200
DEFAULT_LIMIT = 50
MAX_CONTENT_LENGTH = 4 * 1024 * 1024
ALLOWED_IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp"}

DEFAULT_SENSORS = {
    "door": 0,
    "pir": 0,
    "flame": 0,
    "mq2": 0,
}
DEFAULT_ACTUATORS = {
    "relay": 0,
    "pump": 0,
    "buzzer": 0,
    "rgb_r": 0,
    "rgb_g": 0,
    "rgb_b": 0,
}


def create_app():
    app = Flask(__name__)
    app.config["MAX_CONTENT_LENGTH"] = MAX_CONTENT_LENGTH
    app.config["UPLOAD_FOLDER"] = str(UPLOAD_DIR)
    CORS(app, resources={r"/api/*": {"origins": "*"}})

    UPLOAD_DIR.mkdir(parents=True, exist_ok=True)
    init_db()

    notifier = EmailNotifier(get_connection)

    # Start realtime cache background thread (fetches device-agent status once/sec)
    _realtime_cache.start(os.environ.get("DEVICE_ID", "edge-opi5-001"))

    @app.errorhandler(RequestEntityTooLarge)
    def handle_large_file(_error):
        return jsonify({"ok": False, "error": "uploaded file is too large"}), 413

    @app.get("/")
    def dashboard():
        return render_template("dashboard.html")

    @app.get("/health")
    def health():
        return jsonify(
            {
                "ok": True,
                "service": "safety-monitor-backend",
                "version": VERSION,
            }
        )

    @app.post("/api/events")
    def create_event():
        payload = request.get_json(silent=True)
        if payload is None:
            return jsonify({"ok": False, "error": "request body must be JSON"}), 400
        if not isinstance(payload, dict):
            return jsonify({"ok": False, "error": "JSON body must be an object"}), 400

        try:
            event = normalize_event(payload, request.remote_addr)
        except (TypeError, ValueError) as exc:
            return jsonify({"ok": False, "error": str(exc)}), 400

        event_id = insert_event(event)
        return (
            jsonify(
                {
                    "ok": True,
                    "event_id": event_id,
                    "timestamp": event["timestamp"],
                    "risk_score": event["risk_score"],
                }
            ),
            201,
        )

    @app.get("/api/events")
    def get_events():
        limit = parse_limit(request.args.get("limit"), max_limit=500)
        device_id = request.args.get("device_id")
        events = list_events(limit, device_id=device_id)
        return jsonify({"ok": True, "count": len(events), "events": events})

    @app.get("/api/status/latest")
    def latest_status():
        return jsonify({"ok": True, "event": get_latest_event()})

    @app.post("/api/images")
    def upload_image():
        if "image" not in request.files:
            return jsonify({"ok": False, "error": "missing image file"}), 400

        file = request.files["image"]
        if not file or not file.filename:
            return jsonify({"ok": False, "error": "empty image filename"}), 400

        original_name = secure_filename(file.filename)
        suffix = Path(original_name).suffix.lower()
        if suffix not in ALLOWED_IMAGE_EXTENSIONS:
            return jsonify({"ok": False, "error": "unsupported image extension"}), 400

        timestamp = utc_timestamp()
        device_id = clean_optional_text(request.form.get("device_id"))
        event_id = parse_optional_int(request.form.get("event_id"), "event_id")
        note = clean_optional_text(request.form.get("note"))

        safe_device = secure_filename(device_id or "unknown") or "unknown"
        compact_time = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
        filename = f"{compact_time}_{safe_device}_{uuid4().hex[:8]}{suffix}"
        file.save(UPLOAD_DIR / filename)

        url = f"/uploads/{filename}"
        image_id = insert_image(
            {
                "timestamp": timestamp,
                "device_id": device_id,
                "event_id": event_id,
                "filename": filename,
                "url": url,
                "note": note,
            }
        )

        return jsonify({"ok": True, "image_id": image_id, "filename": filename, "url": url})

    @app.get("/uploads/<path:filename>")
    def uploaded_file(filename):
        return send_from_directory(UPLOAD_DIR, filename)

    # --- Device heartbeat & status ---

    @app.post("/api/devices/heartbeat")
    def device_heartbeat():
        payload = request.get_json(silent=True)
        if not payload or not isinstance(payload, dict):
            return jsonify({"ok": False, "error": "JSON body required"}), 400
        device_id = payload.get("device_id")
        if not device_id:
            return jsonify({"ok": False, "error": "device_id required"}), 400
        upsert_device_status(device_id, payload)
        return jsonify({"ok": True, "device_id": device_id})

    @app.get("/api/devices")
    def list_devices():
        devices = get_device_status()
        for d in devices:
            d["online"] = _is_online(d.get("last_seen_at"))
        return jsonify({"ok": True, "devices": devices})

    @app.get("/api/devices/<device_id>")
    def get_device(device_id):
        d = get_device_status(device_id)
        if not d:
            return jsonify({"ok": False, "error": "device not found"}), 404
        d["online"] = _is_online(d.get("last_seen_at"))
        return jsonify({"ok": True, "device": d})

    # --- Telemetry ---

    @app.post("/api/telemetry/batch")
    def telemetry_batch():
        payload = request.get_json(silent=True)
        if not payload or not isinstance(payload, dict):
            return jsonify({"ok": False, "error": "JSON body required"}), 400
        device_id = payload.get("device_id")
        if not device_id:
            return jsonify({"ok": False, "error": "device_id required"}), 400
        window = payload.get("window", {})
        samples = payload.get("samples", [])
        summary = payload.get("summary", {})
        insert_telemetry_batch(
            device_id,
            window.get("start"),
            window.get("end"),
            payload,
            samples,
        )
        _check_telemetry_alerts(device_id, summary, notifier)
        return jsonify({"ok": True, "device_id": device_id, "sample_count": len(samples)})

    @app.get("/api/telemetry/series")
    def telemetry_series():
        device_id = request.args.get("device_id")
        metric = request.args.get("metric")
        seconds = int(request.args.get("seconds", 600))
        if not device_id or not metric:
            return jsonify({"ok": False, "error": "device_id and metric required"}), 400
        canonical = _resolve_metric_alias(metric)
        points = get_telemetry_series(device_id, canonical, seconds)
        threshold = _get_threshold_for_metric(device_id, canonical)
        return jsonify({"ok": True, "device_id": device_id, "metric": metric, "points": points, "threshold": threshold})

    # --- AI observations ---

    @app.post("/api/ai/observations")
    def create_ai_observation():
        payload = request.get_json(silent=True)
        if not payload or not isinstance(payload, dict):
            return jsonify({"ok": False, "error": "JSON body required"}), 400
        device_id = payload.get("device_id")
        if not device_id:
            return jsonify({"ok": False, "error": "device_id required"}), 400
        obs_id = insert_ai_observation(payload)
        _check_ai_alert(device_id, payload, obs_id, notifier)
        return jsonify({"ok": True, "id": obs_id, "device_id": device_id}), 201

    @app.get("/api/ai/observations/latest")
    def latest_ai_observation():
        device_id = request.args.get("device_id")
        if not device_id:
            return jsonify({"ok": False, "error": "device_id required"}), 400
        obs = get_latest_ai_observation(device_id)
        return jsonify({"ok": True, "device_id": device_id, "observation": obs})

    @app.get("/api/ai/observations")
    def list_ai_obs():
        device_id = request.args.get("device_id")
        if not device_id:
            return jsonify({"ok": False, "error": "device_id required"}), 400
        limit = parse_limit(request.args.get("limit"), max_limit=200)
        obs = list_ai_observations(device_id, limit)
        return jsonify({"ok": True, "device_id": device_id, "count": len(obs), "observations": obs})

    # --- Thresholds ---

    @app.get("/api/thresholds")
    def get_device_thresholds():
        device_id = request.args.get("device_id", "default")
        t = get_thresholds(device_id)
        if not t:
            t = _default_thresholds()
        return jsonify({"ok": True, "device_id": device_id, "thresholds": t})

    @app.put("/api/thresholds")
    def set_threshold():
        payload = request.get_json(silent=True)
        if not payload or not isinstance(payload, dict):
            return jsonify({"ok": False, "error": "JSON body required"}), 400
        device_id = payload.get("device_id", "default")
        thresholds = payload.get("thresholds", {})
        for metric, config in thresholds.items():
            upsert_threshold(device_id, metric, config)
        return jsonify({"ok": True, "device_id": device_id})

    # --- Alerts ---

    @app.get("/api/alerts")
    def get_alerts():
        device_id = request.args.get("device_id")
        limit = int(request.args.get("limit", 50))
        alerts = list_alerts(device_id, limit)
        return jsonify({"ok": True, "alerts": alerts})

    # --- Video proxy ---

    @app.get("/api/video/stream")
    def video_stream():
        device_id = request.args.get("device_id")
        if not device_id:
            return jsonify({"ok": False, "error": "device_id required"}), 400
        agent_url = _get_device_agent_url(device_id)
        if not agent_url:
            return jsonify({"ok": False, "error": "device agent not available"}), 503
        import requests as req
        try:
            resp = req.get(f"{agent_url}/api/video/stream", stream=True, timeout=(3, None))
            def generate():
                try:
                    for chunk in resp.iter_content(chunk_size=4096):
                        if chunk:
                            yield chunk
                finally:
                    resp.close()
            return app.response_class(
                generate(),
                content_type=resp.headers.get("Content-Type", "multipart/x-mixed-replace; boundary=frame"),
                headers={
                    "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
                    "Pragma": "no-cache",
                    "X-Accel-Buffering": "no",
                },
            )
        except Exception as e:
            return jsonify({"ok": False, "error": str(e)}), 503

    @app.get("/api/video/snapshot.jpg")
    def video_snapshot():
        device_id = request.args.get("device_id")
        if not device_id:
            return jsonify({"ok": False, "error": "device_id required"}), 400
        agent_url = _get_device_agent_url(device_id)
        if not agent_url:
            return jsonify({"ok": False, "error": "device agent not available", "camera_status": "offline"}), 503
        import requests as req
        try:
            resp = req.get(f"{agent_url}/api/video/snapshot.jpg", timeout=10)
            if resp.status_code == 503:
                return jsonify({"ok": False, "error": "no frame available from device", "camera_status": "offline"}), 503
            ct = resp.headers.get("Content-Type", "")
            if "image/jpeg" not in ct:
                return jsonify({"ok": False, "error": "device agent returned non-JPEG response", "camera_status": "unknown"}), 503
            return app.response_class(resp.content, content_type="image/jpeg")
        except Exception as e:
            return jsonify({"ok": False, "error": str(e), "camera_status": "offline"}), 503

    # --- SSE stream ---

    @app.get("/api/stream/events")
    def sse_stream():
        device_id = request.args.get("device_id")
        import time
        def generate():
            while True:
                data = _build_tick(device_id)
                yield f"data: {json.dumps(data)}\n\n"
                time.sleep(1)
        return app.response_class(generate(), mimetype="text/event-stream")

    # --- Notification ---

    @app.get("/api/notification/settings")
    def get_notification_settings():
        return jsonify({"ok": True, "settings": notifier.get_settings()})

    @app.put("/api/notification/settings")
    def update_notification_settings():
        payload = request.get_json(silent=True)
        if not payload or not isinstance(payload, dict):
            return jsonify({"ok": False, "error": "JSON body required"}), 400
        saved = notifier.update_settings(payload)
        result = {"ok": True, "settings": notifier.get_settings()}
        if not saved:
            result["warning"] = "settings updated in memory but failed to persist to disk"
        return jsonify(result)

    @app.post("/api/notification/test-email")
    def send_test_email():
        result = notifier.send_test_email()
        return jsonify(result)

    @app.get("/api/notification/logs")
    def get_notification_logs():
        limit = int(request.args.get("limit", 50))
        logs = notifier.get_logs(limit)
        return jsonify({"ok": True, "logs": logs})

    return app


def _is_online(last_seen_at, threshold_sec=15):
    if not last_seen_at:
        return False
    from datetime import datetime, timezone
    try:
        seen = datetime.fromisoformat(last_seen_at.replace("Z", "+00:00"))
        return (datetime.now(timezone.utc) - seen).total_seconds() < threshold_sec
    except Exception:
        return False


def _get_device_agent_url(device_id):
    import os
    url = os.environ.get("OPI5_DEVICE_AGENT_URL")
    if url:
        return url.rstrip("/")
    d = get_device_status(device_id)
    if not d:
        return None
    status = d.get("status", {})
    # status IS the heartbeat payload (raw heartbeat JSON stored directly)
    hb = status.get("heartbeat", status) or {}
    # 1. direct agent_url in heartbeat (skip "unknown", empty, or URLs with unknown host)
    for src in (hb, status):
        au = src.get("agent_url")
        if au and au not in ("unknown", "") and "unknown" not in au:
            return au.rstrip("/")
    # 2. infer from ip + agent_port (skip "unknown"/null/empty ip)
    ip = hb.get("ip") or status.get("ip")
    if ip and ip not in ("unknown", "", "null"):
        port = hb.get("agent_port") or status.get("agent_port") or 8090
        return f"http://{ip}:{port}"
    return None


def _default_thresholds():
    return {
        "risk_score": {"warn": 4, "danger": 7, "unit": "score"},
        "device.cpu_temp_c": {"warn": 75, "danger": 85, "unit": "°C"},
        "mpu6500.vibration_score": {"warn": 4, "danger": 7, "unit": "score"},
        "sensor_scores.smoke": {"warn": 3, "danger": 6, "unit": "score"},
    }


_METRIC_ALIASES = {
    "cpu_temp_c": "device.cpu_temp_c",
    "mem_used_mb": "device.mem_used_mb",
    "smoke_score": "sensor_scores.smoke",
    "smoke": "sensor_scores.smoke",
    "flame_score": "sensor_scores.flame",
    "mpu6500_vibration": "mpu6500.vibration_score",
    "vibration_score": "mpu6500.vibration_score",
    "pir": "safety.pir",
    "flame": "safety.flame",
    "mq2": "safety.mq2",
}


def _resolve_metric_alias(metric):
    return _METRIC_ALIASES.get(metric, metric)


def _get_threshold_for_metric(device_id, metric):
    t = get_thresholds(device_id)
    if metric in t:
        return t[metric]
    defaults = _default_thresholds()
    if metric in defaults:
        return defaults[metric]
    # Try alias resolution for threshold lookup
    canonical = _resolve_metric_alias(metric)
    if canonical in t:
        return t[canonical]
    return defaults.get(canonical)


def _check_telemetry_alerts(device_id, summary, notifier):
    from datetime import datetime, timezone
    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    thresholds = get_thresholds(device_id) or _default_thresholds()
    risk = summary.get("risk_score", {})
    if risk.get("latest", 0) >= thresholds.get("risk_score", {}).get("danger", 7):
        alert = {"device_id": device_id, "timestamp": now, "level": "danger", "metric": "risk_score", "message": f"Risk score {risk['latest']} exceeds danger threshold"}
        alert_id = insert_alert(alert)
        alert["id"] = alert_id
        notifier.notify_alert(alert)


def _check_ai_alert(device_id, obs, obs_id, notifier):
    from datetime import datetime, timezone
    risk_hint = obs.get("risk_hint")
    if risk_hint is not None and risk_hint >= 7:
        now = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
        alert = {
            "device_id": device_id,
            "timestamp": now,
            "level": "danger",
            "metric": "ai_risk_hint",
            "message": f"AI risk hint {risk_hint} exceeds threshold",
        }
        alert_id = insert_alert(alert)
        alert["id"] = alert_id
        notifier.notify_alert(alert)


def _build_tick(device_id):
    from datetime import datetime, timezone
    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    tick = {
        "type": "tick",
        "timestamp": now,
        "device": get_device_status(device_id) if device_id else None,
        "latest_event": get_latest_event(),
        "latest_ai_observation": get_latest_ai_observation(device_id) if device_id else None,
        "alerts": list_alerts(device_id, 5) if device_id else [],
    }
    # Realtime channel: read from background cache (non-blocking)
    tick["realtime"] = _realtime_cache.get()
    return tick


import threading as _threading

class _RealtimeCache:
    """Background thread fetches device-agent /api/status once per second.
    SSE clients read from cache without blocking."""

    def __init__(self):
        self._data = None
        self._ts = 0.0
        self._lock = _threading.Lock()
        self._agent_url = None
        self._device_id = None
        self._interval = float(os.environ.get("REALTIME_SSE_INTERVAL_SEC", "1"))
        self._enabled = os.environ.get("REALTIME_SSE_ENABLED", "1") != "0"
        self._timeout = 0.5

    def start(self, device_id):
        if not self._enabled:
            return
        self._device_id = device_id
        _threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        import requests as req
        from time import monotonic
        while True:
            try:
                if self._agent_url is None:
                    self._agent_url = _get_device_agent_url(self._device_id)
                if not self._agent_url:
                    time.sleep(self._interval)
                    continue

                t0 = monotonic()
                resp = req.get(f"{self._agent_url}/api/status", timeout=self._timeout)
                latency_ms = int((monotonic() - t0) * 1000)
                if resp.status_code != 200:
                    time.sleep(self._interval)
                    continue

                data = resp.json()
                m = data.get("metrics", {})
                sensors = m.get("sensors", {})
                device = m.get("device", {})
                sensors_source = data.get("sensors_source", m.get("sensors_source", {}))
                result = {
                    "ts": m.get("ts"),
                    "fetch_latency_ms": latency_ms,
                    "risk_score": m.get("risk_score"),
                    "sensors": {
                        "pir": sensors.get("safety", {}).get("pir"),
                        "flame": sensors.get("safety", {}).get("flame"),
                        "mq2": sensors.get("safety", {}).get("mq2"),
                        "vibration_score": sensors.get("mpu6500", {}).get("vibration_score"),
                        "accel_x": sensors.get("mpu6500", {}).get("accel_x"),
                        "accel_y": sensors.get("mpu6500", {}).get("accel_y"),
                        "accel_z": sensors.get("mpu6500", {}).get("accel_z"),
                        "temp_c": sensors.get("env", {}).get("temp_c"),
                        "humidity_pct": sensors.get("env", {}).get("humidity_pct"),
                        "light_lux": sensors.get("env", {}).get("light_lux"),
                    },
                    "sensors_source": sensors_source,
                    "device": {
                        "cpu_temp_c": device.get("cpu_temp_c"),
                        "mem_used_mb": device.get("mem_used_mb"),
                        "cpu_load_1m": device.get("cpu_load_1m"),
                        "disk_used_pct": device.get("disk_used_pct"),
                    },
                    "camera": data.get("camera"),
                    "ai": data.get("ai"),
                    "warnings": data.get("warnings", []),
                }
                with self._lock:
                    self._data = result
                    self._ts = monotonic()
            except Exception:
                pass
            time.sleep(self._interval)

    def get(self):
        with self._lock:
            if self._data is None:
                return None
            age_ms = int((time.monotonic() - self._ts) * 1000)
            result = dict(self._data)
            result["age_ms"] = age_ms
            return result

_realtime_cache = _RealtimeCache()


def normalize_event(payload, source):
    sensors = normalize_mapping(payload.get("sensors", {}), DEFAULT_SENSORS, "sensors")
    actuators = normalize_mapping(
        payload.get("actuators", {}), DEFAULT_ACTUATORS, "actuators"
    )
    risk_value = payload.get("risk_score", payload.get("risk", 0))

    return {
        "timestamp": utc_timestamp(),
        "type": clean_text(payload.get("type", "event"), "event"),
        "device_id": clean_text(payload.get("device_id", "unknown"), "unknown"),
        "seq": parse_optional_int(payload.get("seq"), "seq"),
        "state": clean_text(payload.get("state", "UNKNOWN"), "UNKNOWN"),
        "risk_score": clamp_int(risk_value, 0, 10, "risk_score"),
        "need_snap": parse_bool(payload.get("need_snap", False)),
        "sensors": sensors,
        "actuators": actuators,
        "raw_json": payload,
        "source": source,
    }


def normalize_mapping(value, defaults, field_name):
    if value is None:
        value = {}
    if not isinstance(value, dict):
        raise TypeError(f"{field_name} must be an object")
    merged = dict(defaults)
    merged.update(value)
    return merged


def clean_text(value, default):
    if value is None:
        return default
    text = str(value).strip()
    return text if text else default


def clean_optional_text(value):
    if value is None:
        return None
    text = str(value).strip()
    return text if text else None


def parse_optional_int(value, field_name):
    if value in (None, ""):
        return None
    try:
        return int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{field_name} must be an integer") from exc


def clamp_int(value, minimum, maximum, field_name):
    try:
        number = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{field_name} must be an integer") from exc
    return max(minimum, min(maximum, number))


def parse_bool(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off", ""}:
            return False
    return bool(value)


def parse_limit(value, max_limit=None):
    if value in (None, ""):
        return DEFAULT_LIMIT
    try:
        limit = int(value)
    except (TypeError, ValueError):
        return DEFAULT_LIMIT
    return max(1, min(max_limit or MAX_LIMIT, limit))


def utc_timestamp():
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


app = create_app()


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
