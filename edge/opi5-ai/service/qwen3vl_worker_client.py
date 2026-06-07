"""Client for persistent Qwen3-VL worker process.

Communicates via stdin/stdout JSON-line protocol.
Manages worker lifecycle: start, health check, infer, stop.
"""

import json
import os
import subprocess
import tempfile
import threading
import time

DEFAULT_WORKER_BINARY = "/opt/edge-ai-safety-monitor/opi5-ai/qwen3vl_worker"


class Qwen3VLWorkerClient:
    def __init__(self, binary_path=DEFAULT_WORKER_BINARY):
        self.binary_path = binary_path
        self.proc = None
        self._lock = threading.Lock()
        self._ready = False

    def start(self, timeout=60):
        """Start worker process and wait for ready signal."""
        if self.proc and self.proc.poll() is None:
            return True

        env = os.environ.copy()
        self.proc = subprocess.Popen(
            [self.binary_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            text=True,
            bufsize=1,  # line-buffered
        )

        # Wait for ready signal
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self.proc.stdout.readline()
            if not line:
                # Process died
                stderr = self.proc.stderr.read()
                raise RuntimeError(f"Worker died: {stderr[:500]}")
            line = line.strip()
            if not line:
                continue
            try:
                msg = json.loads(line)
                if msg.get("action") == "ready" and msg.get("model_loaded"):
                    self._ready = True
                    return True
                if not msg.get("ok"):
                    raise RuntimeError(f"Worker init failed: {msg}")
            except json.JSONDecodeError:
                continue

        raise RuntimeError("Worker ready timeout")

    def health(self):
        """Check worker health."""
        if not self._ensure_alive():
            return {"ok": False, "error": "worker not running"}
        return self._send_recv({"action": "health"})

    def infer(self, image_path, question=None):
        """Run inference on an image file."""
        if not self._ensure_alive():
            return {"ok": False, "error": "worker not running"}

        req = {"image_path": image_path}
        if question:
            req["question"] = question
        return self._send_recv(req)

    def infer_bytes(self, image_bytes, question=None):
        """Run inference on image bytes (writes to temp file)."""
        with tempfile.NamedTemporaryFile(suffix=".jpg", delete=False) as f:
            f.write(image_bytes)
            tmp_path = f.name
        try:
            return self.infer(tmp_path, question)
        finally:
            os.unlink(tmp_path)

    def stop(self):
        """Gracefully stop worker."""
        if self.proc and self.proc.poll() is None:
            try:
                self._send_recv({"action": "quit"}, timeout=5)
            except Exception:
                pass
            self.proc.terminate()
            self.proc.wait(timeout=5)
        self._ready = False

    def _ensure_alive(self):
        if self.proc and self.proc.poll() is None:
            return True
        # Try to restart
        try:
            self.start()
            return True
        except Exception:
            return False

    def _send_recv(self, msg, timeout=120):
        """Send JSON line, receive JSON line."""
        with self._lock:
            line = json.dumps(msg, ensure_ascii=False) + "\n"
            self.proc.stdin.write(line)
            self.proc.stdin.flush()

            deadline = time.time() + timeout
            while time.time() < deadline:
                # Check if process is still alive
                if self.proc.poll() is not None:
                    return {"ok": False, "error": "worker process died"}
                # Read with timeout using select-like approach
                import select
                ready, _, _ = select.select([self.proc.stdout], [], [], min(1.0, deadline - time.time()))
                if ready:
                    resp_line = self.proc.stdout.readline()
                    if not resp_line:
                        return {"ok": False, "error": "worker EOF"}
                    resp_line = resp_line.strip()
                    if resp_line:
                        try:
                            return json.loads(resp_line)
                        except json.JSONDecodeError:
                            continue
            return {"ok": False, "error": "worker response timeout"}


# Module-level singleton
_worker_client = None


def get_worker_client():
    global _worker_client
    if _worker_client is None:
        _worker_client = Qwen3VLWorkerClient()
    return _worker_client


def run_inference(image_bytes, question=None):
    """Inference interface matching qwen3vl_backend.run_inference signature."""
    client = get_worker_client()
    try:
        client.start()
        return client.infer_bytes(image_bytes, question)
    except Exception as e:
        return {"ok": False, "error": str(e)}
