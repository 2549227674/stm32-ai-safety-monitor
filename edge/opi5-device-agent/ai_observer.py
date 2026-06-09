"""AI observation - periodic inference with snapshot + telemetry summary."""

import sys
import time
import requests
from datetime import datetime, timezone


class AiObserver:
    def __init__(self, ai_url, backend_url, device_id, interval_sec):
        self.ai_url = ai_url
        self.backend_url = backend_url
        self.device_id = device_id
        self.interval = interval_sec
        self._latest = None

    def observe(self, snapshot, telemetry_summary):
        if not snapshot:
            print("[ai] no snapshot available, skipping", file=sys.stderr)
            return

        try:
            prompt = (
                "请根据当前摄像头画面和过去 30 秒设备遥测摘要，输出巡检观察。"
                "重点判断：是否有人员/烟雾/火焰/异常震动/设备过热/通信异常。"
                "请给出：1）一句话摘要；2）风险提示 0-10；3）需要人工关注的原因；4）不要输出任何执行器控制命令。"
            )
            metadata = {
                "device_id": self.device_id,
                "frame_id": f"frame_{int(time.time())}",
                "telemetry_window": "30s",
                "summary": telemetry_summary,
                "prompt": prompt,
            }
            resp = requests.post(
                f"{self.ai_url}/api/infer/vision",
                files={"image": ("snapshot.jpg", snapshot, "image/jpeg")},
                data={"metadata": __import__("json").dumps(metadata)},
                timeout=60,
            )
            ai_result = resp.json()
            self._latest = self._build_observation(ai_result, telemetry_summary)
            self._post_to_backend(self._latest)
        except Exception as e:
            print(f"[ai] inference error: {e}", file=sys.stderr)
            self._latest = {"ok": False, "error": str(e), "timestamp": datetime.now(timezone.utc).isoformat()}

    def get_latest(self):
        return self._latest

    def _build_observation(self, ai_result, telemetry_summary):
        return {
            "device_id": self.device_id,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "window_sec": 30,
            "model": {
                "name": ai_result.get("model", "qwen3-vl-2b"),
                "backend": ai_result.get("backend", "rknn-llm"),
                "mode": ai_result.get("mode", "worker"),
                "model_ready": ai_result.get("ok", False),
            },
            "run_metrics": ai_result.get("run_metrics", {}),
            "risk_hint": ai_result.get("risk_hint"),
            "summary": ai_result.get("summary", ai_result.get("text", "")),
            "full_text": ai_result.get("full_text", ai_result.get("text", "")),
            "labels": ai_result.get("labels", []),
            "input": {
                "telemetry_summary": telemetry_summary,
            },
            "ok": ai_result.get("ok", False),
            "error": ai_result.get("error"),
        }

    def _post_to_backend(self, observation):
        try:
            requests.post(f"{self.backend_url}/api/ai/observations", json=observation, timeout=10)
        except Exception as e:
            print(f"[ai] backend post error: {e}", file=sys.stderr)
