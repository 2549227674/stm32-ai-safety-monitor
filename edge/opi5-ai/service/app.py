#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Orange Pi 5 本地 AI 推理服务 —— MOCK 版本
=========================================
本文件提供 docs/07_端边HTTP_JSON_Contract.md 定义的接口的可运行 mock 实现，
目的是在不依赖真实 RKNN / NPU / 模型权重的情况下，先把
i.MX6ULL -> OPi5 -> Flask -> Dashboard 的端边垂直切片打通（对应 Task05 / Task07）。

接口（与 docs/07 一致）：
  GET  /health            -> 服务健康
  POST /api/infer/vision  -> 视觉推理（multipart: image 文件 + metadata JSON 字符串）

安全约束（不可改）：
  - 响应中的 control_allowed 必须恒为 False；AI 只给 risk_hint，不控制执行器。

运行：
  pip install flask          # OPi5 上：python3 -m pip install flask
  OPI5_AI_PORT=8080 python3 app.py
本地冒烟测试：
  curl http://127.0.0.1:8080/health
  curl -F image=@test.jpg \
       -F 'metadata={"device_id":"labbox_001","frame_id":1,"sensors":{"pir":1}}' \
       http://127.0.0.1:8080/api/infer/vision

替换为真实 RKNN：见文件底部 run_inference() 处的 TODO。
"""

import json
import os
import time

from flask import Flask, jsonify, request

CONTRACT_VERSION = "1.0"
SERVICE_NAME = "opi5-ai-service"
MODE = "mock"  # 真实接入后改为 "rknn"

app = Flask(__name__)


@app.get("/health")
def health():
    return jsonify(
        {
            "ok": True,
            "service": SERVICE_NAME,
            "model_ready": True,   # mock 始终就绪；真实 RKNN 未加载时应返回 False
            "mode": MODE,
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

    # 读取图片字节只为模拟真实开销；mock 不做真实解码
    raw = image.read()
    _ = len(raw)

    # 3) 推理（mock）
    objects, faces, risk_hint, summary, action_hint = run_inference(raw, meta, sensors)

    latency_ms = int((time.time() - t0) * 1000)

    resp = {
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
        "control_allowed": False,  # 安全红线：恒为 False，绝不允许 AI 控制执行器
    }
    return jsonify(resp), 200


def run_inference(image_bytes, meta, sensors):
    """
    MOCK 推理逻辑。返回 (objects, faces, risk_hint, summary, action_hint)。

    规则（仅用于联调演示，可观测、可解释）：
      - 默认检测到一个 person（便于打通链路）。
      - 若 metadata.sensors.mq2 或 flame 为真，则追加 smoke 目标并抬高 risk_hint。
      - risk_hint 仅为提示，i.MX6ULL 本地状态机可参考但不盲从。

    >>> [CLAUDE_CODE_TODO | INVESTIGATE] 用真实 RKNN 推理替换本函数
    >>> - 为何 GPT 给不了：沙箱无 NPU、无 .rknn 模型、无 OPi5 运行环境。
    >>> - 期望产物/操作：盘点 PC/OPi5 上已有 rknn-toolkit2 / 现有 demo，
    >>>   用 RKNN Lite/Runtime 加载一个目标检测模型，把推理结果整理成与此处相同的
    >>>   objects/faces/risk_hint 结构；保持 control_allowed=False 不变。
    >>> - 回填位置：edge/opi5-ai/service/app.py run_inference()；docs/09；Task05
    >>> - 验收：上传真实图片返回真实检测框，latency_ms 为真实耗时。
    """
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

    return objects, faces, risk_hint, summary, action_hint


if __name__ == "__main__":
    port = int(os.environ.get("OPI5_AI_PORT", "8080"))
    # 0.0.0.0 便于 i.MX6ULL 通过局域网访问；生产可按需收紧
    app.run(host="0.0.0.0", port=port)
