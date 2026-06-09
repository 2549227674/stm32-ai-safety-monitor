"""Email notifier with cooldown and delivery logging."""

import os
import smtplib
import json
import threading
from datetime import datetime, timezone, timedelta
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart


class EmailNotifier:
    def __init__(self, db_get_connection):
        self._db = db_get_connection
        self._lock = threading.Lock()
        self._load_config()
        self._init_delivery_log()

    def _load_config(self):
        self.enabled = os.environ.get("ALERT_EMAIL_ENABLED", "0") == "1"
        self.smtp_host = os.environ.get("SMTP_HOST", "")
        self.smtp_port = int(os.environ.get("SMTP_PORT", "465"))
        self.smtp_user = os.environ.get("SMTP_USER", "")
        self.smtp_password = os.environ.get("SMTP_PASSWORD", "")
        self.smtp_from = os.environ.get("SMTP_FROM", self.smtp_user)
        self.alert_email_to = os.environ.get("ALERT_EMAIL_TO", "")
        self.cooldown_seconds = int(os.environ.get("ALERT_EMAIL_COOLDOWN_SECONDS", "300"))
        self.use_tls = os.environ.get("SMTP_USE_TLS", "1") == "1"

    def _init_delivery_log(self):
        with self._db() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS notification_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp TEXT NOT NULL,
                    alert_id INTEGER,
                    channel TEXT NOT NULL,
                    recipient TEXT,
                    subject TEXT,
                    status TEXT NOT NULL,
                    error TEXT,
                    raw_json TEXT
                )
            """)

    def get_settings(self):
        return {
            "enabled": self.enabled,
            "smtp_host": self.smtp_host,
            "smtp_port": self.smtp_port,
            "smtp_user": self.smtp_user,
            "smtp_from": self.smtp_from,
            "alert_email_to": self.alert_email_to,
            "cooldown_seconds": self.cooldown_seconds,
            "use_tls": self.use_tls,
            "configured": bool(self.smtp_host and self.smtp_user and self.alert_email_to),
        }

    def update_settings(self, settings):
        if "enabled" in settings:
            self.enabled = bool(settings["enabled"])
        if "smtp_host" in settings:
            self.smtp_host = str(settings["smtp_host"])
        if "smtp_port" in settings:
            self.smtp_port = int(settings["smtp_port"])
        if "smtp_user" in settings:
            self.smtp_user = str(settings["smtp_user"])
        if "smtp_password" in settings:
            self.smtp_password = str(settings["smtp_password"])
        if "smtp_from" in settings:
            self.smtp_from = str(settings["smtp_from"])
        if "alert_email_to" in settings:
            self.alert_email_to = str(settings["alert_email_to"])
        if "cooldown_seconds" in settings:
            self.cooldown_seconds = int(settings["cooldown_seconds"])
        if "use_tls" in settings:
            self.use_tls = bool(settings["use_tls"])

    def send_test_email(self):
        if not self._can_send():
            return {"ok": False, "error": "email not configured or disabled"}
        return self._send(
            subject="[Safety Monitor] Test Alert",
            body="This is a test email from Edge AI Safety Monitor.\n\nIf you received this, email notifications are working correctly.",
            alert_id=None,
        )

    def notify_alert(self, alert):
        if not self.enabled:
            return
        if not self._can_send():
            return
        alert_id = alert.get("id")
        device_id = alert.get("device_id", "unknown")
        level = alert.get("level", "info")
        metric = alert.get("metric", "")
        message = alert.get("message", "")
        if not self._should_send(device_id, metric, level):
            return
        subject = f"[Safety Monitor] {level.upper()} Alert: {metric or 'General'}"
        body = (
            f"Device: {device_id}\n"
            f"Level: {level}\n"
            f"Metric: {metric}\n"
            f"Message: {message}\n"
            f"Time: {alert.get('timestamp', '')}\n"
        )
        threading.Thread(
            target=self._send,
            args=(subject, body, alert_id),
            daemon=True,
        ).start()

    def get_logs(self, limit=50):
        with self._db() as conn:
            rows = conn.execute(
                "SELECT * FROM notification_log ORDER BY id DESC LIMIT ?", (limit,)
            ).fetchall()
        return [dict(r) for r in rows]

    def _can_send(self):
        return self.enabled and self.smtp_host and self.smtp_user and self.alert_email_to

    def _should_send(self, device_id, metric, level):
        key = f"{device_id}:{metric}:{level}"
        cutoff = (datetime.now(timezone.utc) - timedelta(seconds=self.cooldown_seconds)).isoformat()
        with self._db() as conn:
            row = conn.execute(
                "SELECT COUNT(*) as cnt FROM notification_log WHERE raw_json LIKE ? AND timestamp >= ? AND status='sent'",
                (f"%{key}%", cutoff),
            ).fetchone()
        return row["cnt"] == 0 if row else True

    def _send(self, subject, body, alert_id):
        now = datetime.now(timezone.utc).isoformat()
        msg = MIMEMultipart()
        msg["From"] = self.smtp_from
        msg["To"] = self.alert_email_to
        msg["Subject"] = subject
        msg.attach(MIMEText(body, "plain"))

        try:
            if self.use_tls:
                server = smtplib.SMTP_SSL(self.smtp_host, self.smtp_port, timeout=15)
            else:
                server = smtplib.SMTP(self.smtp_host, self.smtp_port, timeout=15)
            server.login(self.smtp_user, self.smtp_password)
            server.sendmail(self.smtp_from, [self.alert_email_to], msg.as_string())
            server.quit()
            self._log(alert_id, "email", self.alert_email_to, subject, "sent", None, body)
            return {"ok": True, "status": "sent"}
        except Exception as e:
            self._log(alert_id, "email", self.alert_email_to, subject, "failed", str(e), body)
            return {"ok": False, "error": str(e)}

    def _log(self, alert_id, channel, recipient, subject, status, error, raw):
        now = datetime.now(timezone.utc).isoformat()
        with self._db() as conn:
            conn.execute(
                "INSERT INTO notification_log (timestamp, alert_id, channel, recipient, subject, status, error, raw_json) VALUES (?,?,?,?,?,?,?,?)",
                (now, alert_id, channel, recipient, subject, status, error, json.dumps({"body": raw})),
            )
