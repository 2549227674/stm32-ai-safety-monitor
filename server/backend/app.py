from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

from flask import Flask, jsonify, render_template, request, send_from_directory
from flask_cors import CORS
from werkzeug.exceptions import RequestEntityTooLarge
from werkzeug.utils import secure_filename

from database import init_db, insert_event, insert_image, list_events, get_latest_event


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
        limit = parse_limit(request.args.get("limit"))
        events = list_events(limit)
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

    return app


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


def parse_limit(value):
    if value in (None, ""):
        return DEFAULT_LIMIT
    try:
        limit = int(value)
    except (TypeError, ValueError):
        return DEFAULT_LIMIT
    return max(1, min(MAX_LIMIT, limit))


def utc_timestamp():
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


app = create_app()


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
