# 09 OPi5 本地 AI 推理与解释方案

## 9.1 AI 服务定位

OPi5 AI 服务是独立的同板服务，运行在端口 8080。调用者可以是 `opi5_safetyd`（安全闭环触发）或 `opi5-device-agent`（30s 周期 observation）。

AI 服务只返回 `risk_hint` 和解释，`control_allowed=false`，不直接控制执行器。

## 9.2 当前主线模型：Qwen3-VL 2B

| 项目 | 说明 |
|---|---|
| 模型 | Qwen3-VL 2B（vision encoder + LLM W8A8） |
| 推理框架 | rknn-llm C++ runtime |
| NPU 使用 | vision encoder 使用 RKNN NPU 3核 |
| 端到端延迟 | ~13s（含模型加载，持久化进程可优化） |
| 输出 | 中文场景描述、risk_hint、vlm_result/explanation |

证据见 `tests/opi5/2026-06-07_qwen3vl_real_ai.md`。

## 9.3 AI 服务接口

### GET /health

```json
{"ok": true, "service": "opi5-ai-service", "model_ready": true, "mode": "qwen3vl"}
```

### POST /api/infer/vision

Content-Type: `multipart/form-data`

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `image` | file | 是 | JPEG/PNG/WebP |
| `metadata` | JSON string | 是 | 设备、帧号、传感器等 |

响应字段：

| 字段 | 类型 | 说明 |
|---|---|---|
| `summary` | string | 自然语言摘要 |
| `full_text` | string | AI 完整输出文本 |
| `risk_hint` | int 0–10 | AI 风险提示 |
| `objects[]` | list | 目标检测结果（VLM 可为空） |
| `vlm_result` | object | VLM 专属结果 |
| `run_metrics` | object | 推理耗时详情 |
| `model` | string | 模型名 |
| `control_allowed` | bool | 必须为 `false` |

完整契约见 `docs/07_端边HTTP_JSON_Contract.md`。

## 9.4 YOLO 目标检测（可选）

| 方案 | 优先级 | 说明 |
|---|---|---|
| Qwen3-VL VLM | 当前主线 | 视觉语言模型，中文场景描述 |
| YOLO 目标检测 | 可选 | person/fire/smoke 等，需模型转换 |
| 从零训练 | 不做 | 超出课程周期 |

## 9.5 AI 失败降级

| 失败 | 响应 |
|---|---|
| 模型未加载 | 返回 503 |
| 推理超时 | 调用方记录 `ai_status=timeout`，继续本地状态机 |
| 低置信度 | 只记录，不提升到 ALARM |
| 服务崩溃 | systemd 自动重启，调用方降级 |

## 9.6 AI 不控制执行器

AI 只返回 `risk_hint` 和解释。执行器由 OPi5 本地 `opi5_safetyd` 根据传感器和安全规则决策。`control_allowed=false` 硬编码。

## 9.7 模型文件与隐私

`.rknn`、`.rkllm`、`.onnx`、`.pt`、人脸库、抓拍图片、数据集不入库。只在 `edge/opi5-ai/models/README.md` 记录来源和本地路径。
