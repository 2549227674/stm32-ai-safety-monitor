#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Orange Pi 5 AI 推理服务
======================
支持两种后端（由环境变量 AI_BACKEND 控制）：
  - mock：用于联调演示，不依赖模型
  - qwen3vl：调用 Qwen3-VL 2B 真实 RKNN/RKLLM 模型

接口（与 docs/07 一致）：
  GET  /health            -> 服务健康
  POST /api/infer/vision  -> 视觉推理（multipart: image 文件 + metadata JSON 字符串）

安全约束（不可改）：
  - 响应中的 control_allowed 必须恒为 False；AI 只给 risk_hint，不控制执行器。

运行：
  AI_BACKEND=mock   OPI5_AI_PORT=8080 python3 app.py
  AI_BACKEND=qwen3vl OPI5_AI_PORT=8080 python3 app.py
"""

import json
import os
import time

from flask import Flask, jsonify, request

CONTRACT_VERSION = "1.0"
SERVICE_NAME = "opi5-ai-service"

AI_BACKEND = os.environ.get("AI_BACKEND", "mock")  # "mock" or "qwen3vl"

app = Flask(__name__)

# Lazy-load qwen3vl backend to avoid import errors when in mock mode
_qwen3vl_backend = None
_risk_mapping = None


def _get_qwen3vl():
    global _qwen3vl_backend, _risk_mapping
    if _qwen3vl_backend is None:
        import qwen3vl_backend
        import risk_mapping
        _qwen3vl_backend = qwen3vl_backend
        _risk_mapping = risk_mapping
    return _qwen3vl_backend, _risk_mapping


@app.get("/health")
def health():
    if AI_BACKEND == "qwen3vl":
        try:
            # Verify binary and models exist
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

    # 1) 取图片（必填）
    image = request.files.get("image")
    if image is None:
        return jsonify({"ok": False, "error": "missing image field"}), 400

    # 2) 取并解析 metadata（必填，JSON 字符串）
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
    else:
        resp = _infer_mock(raw, meta, device_id, frame_id, sensors, t0)

    return jsonify(resp), 200


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
        # Fallback to error response
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
        summary = "检测到人员，且传感器提示烟雾/火焰，建议复核。"
        action_hint = "keep_alarm"
    else:
        summary = "检测到人员进入巡检区域。"
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
