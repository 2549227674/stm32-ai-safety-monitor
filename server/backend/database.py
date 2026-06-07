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


def list_events(limit=50):
    with get_connection() as conn:
        rows = conn.execute(
            """
            SELECT * FROM events
            ORDER BY event_id DESC
            LIMIT ?;
            """,
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
    out["vision"] = vision
    out["ai_result"] = raw.get("ai_result") if isinstance(raw, dict) else None
    out["image_url"] = (
        raw.get("image_url") if isinstance(raw, dict) else None
    ) or ((vision or {}).get("image_url") if isinstance(vision, dict) else None)
    out["latency_ms"] = raw.get("latency_ms") if isinstance(raw, dict) else None
    return out
