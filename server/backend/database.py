import json
import sqlite3
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent
DB_PATH = BASE_DIR / "safety_monitor.db"


def get_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    with get_connection() as conn:
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS events (
                event_id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                device_id TEXT NOT NULL,
                seq INTEGER,
                type TEXT NOT NULL,
                state TEXT NOT NULL,
                risk_score INTEGER NOT NULL,
                need_snap INTEGER NOT NULL,
                sensors_json TEXT NOT NULL,
                actuators_json TEXT NOT NULL,
                raw_json TEXT NOT NULL,
                source TEXT
            );
            """
        )
        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);"
        )
        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_events_device_id ON events(device_id);"
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS images (
                image_id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                device_id TEXT,
                event_id INTEGER,
                filename TEXT NOT NULL,
                url TEXT NOT NULL,
                note TEXT,
                FOREIGN KEY(event_id) REFERENCES events(event_id)
            );
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS device_status (
                device_id TEXT PRIMARY KEY,
                last_seen_at TEXT NOT NULL,
                status_json TEXT NOT NULL
            );
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS telemetry_samples (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                metric TEXT NOT NULL,
                value REAL,
                unit TEXT,
                raw_json TEXT
            );
            """
        )
        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_telemetry_metric_time ON telemetry_samples(device_id, metric, timestamp);"
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS telemetry_batches (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                window_start TEXT,
                window_end TEXT,
                batch_json TEXT NOT NULL
            );
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS ai_observations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                model_name TEXT,
                risk_hint INTEGER,
                total_ms INTEGER,
                first_token_ms INTEGER,
                observation_json TEXT NOT NULL
            );
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS thresholds (
                device_id TEXT NOT NULL,
                metric TEXT NOT NULL,
                config_json TEXT NOT NULL,
                PRIMARY KEY(device_id, metric)
            );
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS alerts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                level TEXT NOT NULL,
                metric TEXT,
                message TEXT NOT NULL,
                alert_json TEXT NOT NULL
            );
            """
        )


def insert_event(event):
    with get_connection() as conn:
        cursor = conn.execute(
            """
            INSERT INTO events (
                timestamp, device_id, seq, type, state, risk_score, need_snap,
                sensors_json, actuators_json, raw_json, source
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
            """,
            (
                event["timestamp"],
                event["device_id"],
                event["seq"],
                event["type"],
                event["state"],
                event["risk_score"],
                1 if event["need_snap"] else 0,
                json.dumps(event["sensors"], ensure_ascii=False),
                json.dumps(event["actuators"], ensure_ascii=False),
                json.dumps(event["raw_json"], ensure_ascii=False),
                event.get("source"),
            ),
        )
        return cursor.lastrowid


def list_events(limit=50, device_id=None):
    with get_connection() as conn:
        if device_id:
            rows = conn.execute(
                "SELECT * FROM events WHERE device_id=? ORDER BY event_id DESC LIMIT ?;",
                (device_id, limit),
            ).fetchall()
        else:
            rows = conn.execute(
                "SELECT * FROM events ORDER BY event_id DESC LIMIT ?;",
                (limit,),
            ).fetchall()
    return [event_row_to_dict(row) for row in rows]


def get_latest_event():
    with get_connection() as conn:
        row = conn.execute(
            """
            SELECT * FROM events
            ORDER BY event_id DESC
            LIMIT 1;
            """
        ).fetchone()
    return event_row_to_dict(row) if row else None


def insert_image(image):
    with get_connection() as conn:
        cursor = conn.execute(
            """
            INSERT INTO images (
                timestamp, device_id, event_id, filename, url, note
            ) VALUES (?, ?, ?, ?, ?, ?);
            """,
            (
                image["timestamp"],
                image.get("device_id"),
                image.get("event_id"),
                image["filename"],
                image["url"],
                image.get("note"),
            ),
        )
        return cursor.lastrowid


def safe_json_loads(value, default):
    if not value:
        return default
    try:
        return json.loads(value)
    except (TypeError, json.JSONDecodeError):
        return default


def upsert_device_status(device_id, status_json):
    from datetime import datetime, timezone
    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    with get_connection() as conn:
        conn.execute(
            "INSERT OR REPLACE INTO device_status (device_id, last_seen_at, status_json) VALUES (?, ?, ?)",
            (device_id, now, json.dumps(status_json, ensure_ascii=False)),
        )


def get_device_status(device_id=None):
    with get_connection() as conn:
        if device_id:
            row = conn.execute("SELECT * FROM device_status WHERE device_id=?", (device_id,)).fetchone()
            return _device_status_row(row) if row else None
        rows = conn.execute("SELECT * FROM device_status ORDER BY last_seen_at DESC").fetchall()
        return [_device_status_row(r) for r in rows]


def _device_status_row(row):
    return {
        "device_id": row["device_id"],
        "last_seen_at": row["last_seen_at"],
        "status": safe_json_loads(row["status_json"], {}),
    }


def insert_telemetry_batch(device_id, window_start, window_end, batch_json, samples):
    with get_connection() as conn:
        conn.execute(
            "INSERT INTO telemetry_batches (device_id, window_start, window_end, batch_json) VALUES (?, ?, ?, ?)",
            (device_id, window_start, window_end, json.dumps(batch_json, ensure_ascii=False)),
        )
        for s in samples:
            ts = s.get("ts", "")
            _insert_sample(conn, device_id, ts, "risk_score", s.get("risk_score"))
            dev = s.get("device", {})
            _insert_sample(conn, device_id, ts, "device.cpu_temp_c", dev.get("cpu_temp_c"))
            _insert_sample(conn, device_id, ts, "device.mem_used_mb", dev.get("mem_used_mb"))
            mpu = (s.get("sensors") or {}).get("mpu6500", {})
            for k in ("accel_x", "accel_y", "accel_z", "gyro_x", "gyro_y", "gyro_z", "vibration_score"):
                _insert_sample(conn, device_id, ts, f"mpu6500.{k}", mpu.get(k))
            safety = (s.get("sensors") or {}).get("safety", {})
            for k in ("pir", "flame", "mq2"):
                _insert_sample(conn, device_id, ts, f"safety.{k}", safety.get(k))
            for k, v in (s.get("sensor_scores") or {}).items():
                _insert_sample(conn, device_id, ts, f"sensor_scores.{k}", v)


def _insert_sample(conn, device_id, ts, metric, value):
    if value is None:
        return
    conn.execute(
        "INSERT INTO telemetry_samples (device_id, timestamp, metric, value) VALUES (?, ?, ?, ?)",
        (device_id, ts, metric, float(value)),
    )


def get_telemetry_series(device_id, metric, seconds=600):
    from datetime import datetime, timezone, timedelta
    cutoff = (datetime.now(timezone.utc) - timedelta(seconds=seconds)).isoformat()
    with get_connection() as conn:
        rows = conn.execute(
            "SELECT timestamp, value FROM telemetry_samples WHERE device_id=? AND metric=? AND timestamp>=? ORDER BY timestamp",
            (device_id, metric, cutoff),
        ).fetchall()
    return [{"t": r["timestamp"], "v": r["value"]} for r in rows]


def insert_ai_observation(obs):
    with get_connection() as conn:
        cursor = conn.execute(
            "INSERT INTO ai_observations (device_id, timestamp, model_name, risk_hint, total_ms, first_token_ms, observation_json) VALUES (?, ?, ?, ?, ?, ?, ?)",
            (
                obs["device_id"],
                obs["timestamp"],
                obs.get("model_name"),
                obs.get("risk_hint"),
                obs.get("total_ms"),
                obs.get("first_token_ms"),
                json.dumps(obs, ensure_ascii=False),
            ),
        )
        return cursor.lastrowid


def get_latest_ai_observation(device_id):
    with get_connection() as conn:
        row = conn.execute(
            "SELECT * FROM ai_observations WHERE device_id=? ORDER BY id DESC LIMIT 1",
            (device_id,),
        ).fetchone()
    if not row:
        return None
    return safe_json_loads(row["observation_json"], {})


def list_ai_observations(device_id, limit=24):
    with get_connection() as conn:
        rows = conn.execute(
            "SELECT * FROM ai_observations WHERE device_id=? ORDER BY id DESC LIMIT ?",
            (device_id, limit),
        ).fetchall()
    return [safe_json_loads(r["observation_json"], {}) for r in rows]


def get_thresholds(device_id):
    with get_connection() as conn:
        rows = conn.execute("SELECT metric, config_json FROM thresholds WHERE device_id=?", (device_id,)).fetchall()
    return {r["metric"]: safe_json_loads(r["config_json"], {}) for r in rows}


def upsert_threshold(device_id, metric, config):
    with get_connection() as conn:
        conn.execute(
            "INSERT OR REPLACE INTO thresholds (device_id, metric, config_json) VALUES (?, ?, ?)",
            (device_id, metric, json.dumps(config, ensure_ascii=False)),
        )


def insert_alert(alert):
    with get_connection() as conn:
        cursor = conn.execute(
            "INSERT INTO alerts (device_id, timestamp, level, metric, message, alert_json) VALUES (?, ?, ?, ?, ?, ?)",
            (
                alert["device_id"],
                alert["timestamp"],
                alert["level"],
                alert.get("metric"),
                alert["message"],
                json.dumps(alert, ensure_ascii=False),
            ),
        )
        return cursor.lastrowid


def list_alerts(device_id=None, limit=50):
    with get_connection() as conn:
        if device_id:
            rows = conn.execute("SELECT * FROM alerts WHERE device_id=? ORDER BY id DESC LIMIT ?", (device_id, limit)).fetchall()
        else:
            rows = conn.execute("SELECT * FROM alerts ORDER BY id DESC LIMIT ?", (limit,)).fetchall()
    return [safe_json_loads(r["alert_json"], {}) for r in rows]


def event_row_to_dict(row):
    raw = safe_json_loads(row["raw_json"], {})
    vision = raw.get("vision") if isinstance(raw, dict) else None

    out = {
        "event_id": row["event_id"],
        "timestamp": row["timestamp"],
        "device_id": row["device_id"],
        "seq": row["seq"],
        "type": row["type"],
        "state": row["state"],
        "risk_score": row["risk_score"],
        "need_snap": bool(row["need_snap"]),
        "sensors": safe_json_loads(row["sensors_json"], {}),
        "actuators": safe_json_loads(row["actuators_json"], {}),
        "source": row["source"],
    }

    out["contract_version"] = raw.get("contract_version") if isinstance(raw, dict) else None
    out["device_health"] = raw.get("device_health") if isinstance(raw, dict) else None
    out["vision"] = vision
    out["ai_result"] = raw.get("ai_result") if isinstance(raw, dict) else None
    out["image_url"] = (
        raw.get("image_url") if isinstance(raw, dict) else None
    ) or ((vision or {}).get("image_url") if isinstance(vision, dict) else None)
    out["latency_ms"] = raw.get("latency_ms") if isinstance(raw, dict) else None
    return out
