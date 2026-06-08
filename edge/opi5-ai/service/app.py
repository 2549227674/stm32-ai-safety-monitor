#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Orange Pi 5 AI Service
======================
Supports three backends (controlled by AI_BACKEND env var):
  - mock: for integration testing, no model dependency
  - qwen3vl: Qwen3-VL 2B real RKNN/RKLLM model
  - yolo_rknn: YOLO object detection via RKNN (fire/smoke bbox)

API (matches docs/07):
  GET  /health            -> service health
  POST /api/infer/vision  -> vision inference (multipart: image file + metadata JSON string)
  POST /api/track/frame   -> low-latency tracking (multipart: image + metadata)
  GET  /api/track/stream  -> low-framerate annotated MJPEG stream

Safety constraint (immutable):
  - control_allowed must always be False; AI only gives risk_hint, never controls actuators.

Run:
  AI_BACKEND=mock      OPI5_AI_PORT=8080 python3 app.py
  AI_BACKEND=qwen3vl   OPI5_AI_PORT=8080 python3 app.py
  AI_BACKEND=yolo_rknn OPI5_AI_PORT=8080 python3 app.py
"""

import io
import json
import os
import threading
import time

from flask import Flask, Response, jsonify, request

CONTRACT_VERSION = "1.1"
SERVICE_NAME = "opi5-ai-service"

AI_BACKEND = os.environ.get("AI_BACKEND", "mock")  # "mock", "qwen3vl", or "yolo_rknn"

app = Flask(__name__)

# Lazy-loaded backends
_qwen3vl_backend = None
_risk_mapping = None
_yolo_backend = None

# Ring buffer for annotated stream frames (track/stream)
_stream_lock = threading.Lock()
_stream_frame = None  # (jpeg_bytes, timestamp)
_stream_clients = 0


def _get_qwen3vl():
    global _qwen3vl_backend, _risk_mapping
    if _qwen3vl_backend is None:
        import qwen3vl_backend
        import risk_mapping
        _qwen3vl_backend = qwen3vl_backend
        _risk_mapping = risk_mapping
    return _qwen3vl_backend, _risk_mapping


def _get_yolo():
    global _yolo_backend
    if _yolo_backend is None:
        import yolo_rknn_backend
        _yolo_backend = yolo_rknn_backend
    return _yolo_backend


@app.get("/health")
def health():
    if AI_BACKEND == "qwen3vl":
        try:
            binary = os.environ.get(
                "QWEN3VL_BINARY",
                "/opt/edge-ai-safety-monitor/opi5-ai/qwen3vl_single_shot",
            )
            encoder = os.environ.get(
                "QWEN3VL_ENCODER",
                "/home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn",
            )
            llm = os.environ.get(
                "QWEN3VL_LLM",
                "/home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_base.rkllm",
            )
            ready = (
                os.path.isfile(binary)
                and os.path.isfile(encoder)
                and os.path.isfile(llm)
            )
            return jsonify(
                {
                    "ok": ready,
                    "service": SERVICE_NAME,
                    "mode": "qwen3vl",
                    "model_ready": ready,
                    "contract_version": CONTRACT_VERSION,
                    "models": [
                        {
                            "name": "qwen3-vl-2b",
                            "backend": "rknn-llm",
                            "version": "local",
                        }
                    ],
                }
            )
        except Exception as e:
            return jsonify(
                {
                    "ok": False,
                    "service": SERVICE_NAME,
                    "mode": "qwen3vl",
                    "model_ready": False,
                    "error": str(e),
                }
            )
    elif AI_BACKEND == "yolo_rknn":
        try:
            yolo = _get_yolo()
            status = yolo.model_status()
            return jsonify(
                {
                    "ok": True,
                    "service": SERVICE_NAME,
                    "mode": "yolo_rknn",
                    "model_ready": status.get("model_ready", False),
                    "contract_version": CONTRACT_VERSION,
                    "models": [
                        {
                            "name": "yolo-fire-smoke",
                            "backend": status.get("backend", "rknn"),
                            "version": "local",
                        }
                    ],
                    "model_status": status,
                }
            )
        except Exception as e:
            return jsonify(
                {
                    "ok": False,
                    "service": SERVICE_NAME,
                    "mode": "yolo_rknn",
                    "model_ready": False,
                    "error": str(e),
                }
            )
    else:
        return jsonify(
            {
                "ok": True,
                "service": SERVICE_NAME,
                "model_ready": True,
                "mode": "mock",
                "contract_version": CONTRACT_VERSION,
            }
        )


@app.post("/api/infer/vision")
def infer_vision():
    t0 = time.time()

    image = request.files.get("image")
    if image is None:
        return jsonify({"ok": False, "error": "missing image field"}), 400

    meta_raw = request.form.get("metadata", "")
    try:
        meta = json.loads(meta_raw) if meta_raw else {}
    except json.JSONDecodeError:
        return jsonify({"ok": False, "error": "metadata is not valid JSON"}), 400

    device_id = meta.get("device_id", "unknown")
    frame_id = meta.get("frame_id", 0)
    sensors = meta.get("sensors", {}) or {}

    raw = image.read()

    if AI_BACKEND == "qwen3vl":
        resp = _infer_qwen3vl(raw, meta, device_id, frame_id, sensors, t0)
    elif AI_BACKEND == "yolo_rknn":
        resp = _infer_yolo(raw, meta, device_id, frame_id, sensors, t0)
    else:
        resp = _infer_mock(raw, meta, device_id, frame_id, sensors, t0)

    return jsonify(resp), 200


@app.post("/api/track/frame")
def track_frame():
    """Low-latency tracking endpoint: detect objects and return tracking_hint."""
    t0 = time.time()

    image = request.files.get("image")
    if image is None:
        return jsonify({"ok": False, "error": "missing image field"}), 400

    meta_raw = request.form.get("metadata", "")
    try:
        meta = json.loads(meta_raw) if meta_raw else {}
    except json.JSONDecodeError:
        return jsonify({"ok": False, "error": "metadata is not valid JSON"}), 400

    raw = image.read()
    frame_id = meta.get("frame_id", 0)

    img_w = int(meta.get("resolution", {}).get("width", 640)) if isinstance(meta.get("resolution"), dict) else 640
    img_h = int(meta.get("resolution", {}).get("height", 480)) if isinstance(meta.get("resolution"), dict) else 480

    yolo = _get_yolo()
    result = yolo.detect(raw, metadata=meta)

    latency_ms = int((time.time() - t0) * 1000)

    objects = result.get("objects", [])
    best_fire = result.get("best_fire")

    tracking_hint = _compute_tracking_hint(objects, best_fire, img_w, img_h)

    resp = {
        "ok": True,
        "contract_version": CONTRACT_VERSION,
        "frame_id": frame_id,
        "latency_ms": latency_ms,
        "objects": objects,
        "best_fire": best_fire,
        "tracking_hint": tracking_hint,
        "control_allowed": False,
    }

    # Update stream frame (annotate with bboxes)
    _update_stream_frame(raw, objects, img_w, img_h)

    return jsonify(resp), 200


def _compute_tracking_hint(objects, best_fire, img_w, img_h):
    """Compute tracking_hint from detected objects. Pure perception, no commands."""
    frame_cx, frame_cy = img_w // 2, img_h // 2

    if best_fire and best_fire.get("bbox"):
        bbox = best_fire["bbox"]
        target_cx = (bbox[0] + bbox[2]) // 2
        target_cy = (bbox[1] + bbox[3]) // 2
        return {
            "target_label": best_fire.get("label", "fire"),
            "target_center": [target_cx, target_cy],
            "frame_center": [frame_cx, frame_cy],
            "offset_px": [target_cx - frame_cx, target_cy - frame_cy],
            "confidence": best_fire.get("score", 0),
        }

    # No fire detected; find best scoring object
    if objects:
        best = max(objects, key=lambda o: o.get("score", 0))
        bbox = best.get("bbox", [0, 0, 0, 0])
        target_cx = (bbox[0] + bbox[2]) // 2
        target_cy = (bbox[1] + bbox[3]) // 2
        return {
            "target_label": best.get("label", "unknown"),
            "target_center": [target_cx, target_cy],
            "frame_center": [frame_cx, frame_cy],
            "offset_px": [target_cx - frame_cx, target_cy - frame_cy],
            "confidence": best.get("score", 0),
        }

    return {
        "target_label": None,
        "target_center": [frame_cx, frame_cy],
        "frame_center": [frame_cx, frame_cy],
        "offset_px": [0, 0],
        "confidence": 0,
    }


def _update_stream_frame(jpeg_bytes, objects, img_w, img_h):
    """Annotate frame with bboxes and store in ring buffer for /api/track/stream."""
    global _stream_frame
    try:
        from PIL import Image, ImageDraw, ImageFont
        img = Image.open(io.BytesIO(jpeg_bytes))
        draw = ImageDraw.Draw(img)

        for obj in objects:
            bbox = obj.get("bbox", [])
            if len(bbox) == 4:
                label = obj.get("label", "?")
                score = obj.get("score", 0)
                color = "red" if label in ("fire", "smoke") else "blue"
                draw.rectangle(bbox, outline=color, width=2)
                draw.text((bbox[0], bbox[1] - 12), f"{label} {score:.2f}", fill=color)

        # Draw frame center crosshair
        cx, cy = img_w // 2, img_h // 2
        draw.line([(cx - 10, cy), (cx + 10, cy)], fill="green", width=1)
        draw.line([(cx, cy - 10), (cx, cy + 10)], fill="green", width=1)

        buf = io.BytesIO()
        img.save(buf, format="JPEG", quality=80)
        with _stream_lock:
            _stream_frame = (buf.getvalue(), time.time())
    except ImportError:
        # PIL not available; store raw frame
        with _stream_lock:
            _stream_frame = (jpeg_bytes, time.time())
    except Exception:
        pass


@app.get("/api/track/stream")
def track_stream():
    """MJPEG multipart stream of annotated frames. ≤5 fps."""
    boundary = "frame"

    def generate():
        global _stream_clients
        _stream_clients += 1
        last_sent = 0
        try:
            while True:
                frame_data = None
                with _stream_lock:
                    if _stream_frame is not None:
                        frame_data = _stream_frame[0]

                now = time.time()
                if frame_data and (now - last_sent) >= 0.2:  # ≤5fps
                    last_sent = now
                    yield (
                        f"--{boundary}\r\n"
                        f"Content-Type: image/jpeg\r\n"
                        f"Content-Length: {len(frame_data)}\r\n\r\n"
                    ).encode() + frame_data + b"\r\n"
                else:
                    # Push placeholder or sleep
                    time.sleep(0.1)
        except GeneratorExit:
            pass
        finally:
            _stream_clients -= 1

    return Response(
        generate(),
        mimetype=f"multipart/x-mixed-replace; boundary={boundary}",
    )


def _infer_yolo(image_bytes, meta, device_id, frame_id, sensors, t0):
    """YOLO RKNN inference."""
    yolo = _get_yolo()
    result = yolo.detect(image_bytes, metadata=meta)

    latency_ms = int((time.time() - t0) * 1000)
    objects = result.get("objects", [])
    best_fire = result.get("best_fire")

    # Map label to risk_hint (0-10 scale)
    risk_hint = 0
    summary_parts = []
    for obj in objects:
        label = obj.get("label", "")
        if label == "fire":
            risk_hint = max(risk_hint, 7)
            summary_parts.append(f"fire ({obj['score']:.0%})")
        elif label == "smoke":
            risk_hint = max(risk_hint, 6)
            summary_parts.append(f"smoke ({obj['score']:.0%})")
        elif label == "person":
            risk_hint = max(risk_hint, 2)
            summary_parts.append(f"person ({obj['score']:.0%})")

    # Boost risk if local sensors confirm
    hazard = bool(sensors.get("mq2")) or bool(sensors.get("flame"))
    if hazard and risk_hint >= 6:
        risk_hint = min(10, risk_hint + 2)

    if summary_parts:
        summary = "YOLO detected: " + ", ".join(summary_parts) + "."
    else:
        summary = "YOLO: no objects detected."

    action_hint = "none"
    if risk_hint >= 6:
        action_hint = "confirm_local_alarm"
    elif risk_hint >= 3:
        action_hint = "verify"

    img_w = int(meta.get("resolution", {}).get("width", 640)) if isinstance(meta.get("resolution"), dict) else 640
    img_h = int(meta.get("resolution", {}).get("height", 480)) if isinstance(meta.get("resolution"), dict) else 480
    tracking_hint = _compute_tracking_hint(objects, best_fire, img_w, img_h)

    return {
        "ok": True,
        "contract_version": CONTRACT_VERSION,
        "device_id": device_id,
        "frame_id": frame_id,
        "latency_ms": latency_ms,
        "models": [{"name": "yolo-fire-smoke", "backend": "rknn", "version": "local"}],
        "objects": objects,
        "faces": [],
        "risk_hint": risk_hint,
        "summary": summary,
        "action_hint": action_hint,
        "tracking_hint": tracking_hint,
        "control_allowed": False,
    }


def _infer_qwen3vl(image_bytes, meta, device_id, frame_id, sensors, t0):
    """Qwen3-VL real inference."""
    qvl, risk_map = _get_qwen3vl()

    question = os.environ.get(
        "QWEN3VL_QUESTION",
        "请用一句话描述画面内容，并判断是否存在人员、烟雾、火焰或明显安全异常。",
    )

    result = qvl.run_inference(image_bytes, question=question)

    latency_ms = int((time.time() - t0) * 1000)

    if not result.get("ok"):
        return {
            "ok": True,
            "contract_version": CONTRACT_VERSION,
            "device_id": device_id,
            "frame_id": frame_id,
            "latency_ms": latency_ms,
            "models": [{"name": "qwen3-vl-2b", "version": "local", "backend": "rknn-llm"}],
            "objects": [],
            "faces": [],
            "risk_hint": 0,
            "summary": f"Qwen3-VL inference failed: {result.get('error', 'unknown')}",
            "action_hint": "none",
            "control_allowed": False,
            "vlm_result": {"ok": False, "error": result.get("error", "unknown")},
        }

    vlm_text = result.get("text", "")
    risk_info = risk_map.map_risk(vlm_text)
    action_hint = risk_map.map_action(risk_info["risk_hint"])

    return {
        "ok": True,
        "contract_version": CONTRACT_VERSION,
        "device_id": device_id,
        "frame_id": frame_id,
        "latency_ms": latency_ms,
        "models": [{"name": "qwen3-vl-2b", "version": "local", "backend": "rknn-llm"}],
        "objects": [],
        "faces": [],
        "risk_hint": risk_info["risk_hint"],
        "summary": vlm_text[:200] if vlm_text else "no output",
        "explanation": vlm_text,
        "vlm_result": {
            "model": "qwen3-vl-2b",
            "description": vlm_text,
            "labels": risk_info["labels"],
            "confidence": None,
            "raw_text": vlm_text,
            "timing": {
                "total_ms": result.get("total_ms", 0),
                "imgenc_ms": result.get("imgenc_ms", 0),
                "llm_ms": result.get("llm_ms", 0),
            },
        },
        "action_hint": action_hint,
        "control_allowed": False,
    }


def _infer_mock(image_bytes, meta, device_id, frame_id, sensors, t0):
    """Mock inference (original behavior)."""
    objects = [{"label": "person", "score": 0.90, "bbox": [120, 80, 260, 430]}]
    faces = []
    risk_hint = 2

    hazard = bool(sensors.get("mq2")) or bool(sensors.get("flame"))
    if hazard:
        objects.append({"label": "smoke", "score": 0.66, "bbox": [300, 120, 500, 260]})
        risk_hint = 4
        summary = "Mock: person detected, sensors suggest smoke/flame, manual review needed."
        action_hint = "keep_alarm"
    else:
        summary = "Mock: person detected in inspection area."
        action_hint = "none"

    latency_ms = int((time.time() - t0) * 1000)

    return {
        "ok": True,
        "contract_version": CONTRACT_VERSION,
        "device_id": device_id,
        "frame_id": frame_id,
        "latency_ms": latency_ms,
        "models": [{"name": "mock-detector", "version": "mock-0.1"}],
        "objects": objects,
        "faces": faces,
        "risk_hint": risk_hint,
        "summary": summary,
        "action_hint": action_hint,
        "control_allowed": False,
    }


if __name__ == "__main__":
    port = int(os.environ.get("OPI5_AI_PORT", "8080"))
    app.run(host="0.0.0.0", port=port)
