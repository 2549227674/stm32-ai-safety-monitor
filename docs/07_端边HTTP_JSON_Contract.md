# 07 端边 HTTP JSON Contract

## 7.1 Contract 目标

本文件是端边数据契约的唯一权威来源。所有代码、测试和 Dashboard 字段必须与本文件保持一致。

目标：

1. 保留旧事件 JSON 的核心语义：`risk_score`、`sensors`、`actuators`、`state`。
2. 将原 STM32↔ESP32-CAM UART JSON 演化为 i.MX6ULL↔OPi5↔Flask 的 HTTP/JSON Contract。
3. AI 只返回 `risk_hint` 和解释，绝不直接控制执行器。
4. 所有接口都能用 curl 单独测试。

## 7.2 i.MX6ULL → OPi5 推理请求

接口：`POST http://<OPI5_HOST_TODO_FILL>:<OPI5_AI_PORT_TODO_FILL>/api/infer/vision`

Content-Type：`multipart/form-data`

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `image` | file | 是 | JPEG/PNG/WebP，MVP 优先 JPEG |
| `metadata` | JSON string | 是 | 设备、帧号、传感器、云台角度等 |

`metadata` 示例：

```json
{
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "frame_id": 42,
  "timestamp": "<TODO:FILL_BY_RUNTIME>",
  "source": "imx6ull_v4l2",
  "image_format": "jpeg",
  "resolution": {"width": 640, "height": 480},
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "state_before_ai": "VERIFY",
  "risk_score_before_ai": 3,
  "pan_tilt": {"pan_deg": 90, "tilt_deg": 90}
}
```

## 7.3 OPi5 → i.MX6ULL 推理响应

```json
{
  "ok": true,
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "frame_id": 42,
  "latency_ms": 86,
  "models": [{"name": "yolo_person_fire", "version": "rknn-0.1"}],
  "objects": [
    {"label": "person", "score": 0.91, "bbox": [120, 80, 260, 430]},
    {"label": "smoke", "score": 0.67, "bbox": [300, 120, 500, 260]}
  ],
  "faces": [
    {"identity": "unknown", "score": 0.78, "bbox": [150, 90, 230, 190]}
  ],
  "risk_hint": 4,
  "summary": "检测到人员进入巡检区域，疑似烟雾区域需要复核。",
  "action_hint": "keep_alarm",
  "control_allowed": false
}
```

字段要求：

| 字段 | 类型 | 要求 |
|---|---|---|
| `ok` | bool | 请求是否成功 |
| `latency_ms` | int | OPi5 服务内部耗时，真实值由服务填充 |
| `objects` | list | 每个对象包含 `label/score/bbox` |
| `faces` | list | 可为空；身份只用于展示和风险提示 |
| `risk_hint` | int 0–10 | AI 风险提示，i.MX6ULL 可参考但不盲从 |
| `summary` | string | Dashboard 展示 |
| `action_hint` | string | 只作提示，不作为执行器命令 |
| `control_allowed` | bool | 必须为 `false` |

### 向后兼容新增字段（VLM 类模型）

对于 VLM 类模型（如 Qwen3-VL），可额外返回以下字段。这些字段不删除原有 `objects[].bbox` 规范；目标检测模型仍应提供 bbox。

| 字段 | 类型 | 说明 |
|---|---|---|
| `explanation` | string | VLM 完整输出文本（与 summary 可重复） |
| `vlm_result` | object | VLM 专属结果，包含以下子字段 |
| `vlm_result.model` | string | 模型名，如 `"qwen3-vl-2b"` |
| `vlm_result.description` | string | VLM 描述文本 |
| `vlm_result.labels` | list[string] | 从文本提取的标签，如 `["person","smoke"]` |
| `vlm_result.confidence` | float\|null | VLM 通常无精确置信度，可为 null |
| `vlm_result.raw_text` | string | 原始输出文本 |
| `vlm_result.timing` | object | 推理耗时详情 |

注意：VLM 类模型的 `objects` 可为空数组，因为 VLM 不提供像素级 bbox。不得伪造 bbox。

## 7.4 i.MX6ULL → Flask 事件上报

接口：`POST http://<BACKEND_HOST_TODO_FILL>:5000/api/events`

兼容旧字段，新增视觉字段。示例：

```json
{
  "type": "event",
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "seq": 1001,
  "state": "VERIFY",
  "risk_score": 5,
  "need_snap": true,
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 1},
  "vision": {
    "frame_id": 42,
    "image_url": "/uploads/20260604_labbox_001_xxxx.jpg",
    "pan_tilt": {"pan_deg": 90, "tilt_deg": 90}
  },
  "ai_result": {
    "ok": true,
    "risk_hint": 4,
    "summary": "检测到人员进入巡检区域。",
    "objects": [{"label": "person", "score": 0.91, "bbox": [120,80,260,430]}],
    "faces": []
  },
  "latency_ms": {"capture": 120, "ai": 86, "post": 20}
}
```

## 7.5 图片上传

现有 Flask 已支持 `POST /api/images`，字段：

| 字段 | 类型 | 说明 |
|---|---|---|
| `image` | file | 图片文件，扩展名 jpg/jpeg/png/webp |
| `device_id` | form | 设备 ID |
| `event_id` | form | 可选，关联事件 |
| `note` | form | 说明 |

MVP 可选两种顺序：

1. 先上传图片拿 `url`，再上报事件带 `image_url`。
2. 先上报事件拿 `event_id`，再上传图片带 `event_id`。

推荐 MVP 使用第 1 种，Dashboard 实现更直观。

## 7.6 错误码

| 场景 | HTTP | JSON |
|---|---|---|
| 请求成功 | 200/201 | `{"ok": true}` |
| 请求体错误 | 400 | `{"ok": false, "error": "..."}` |
| 文件过大 | 413 | `{"ok": false, "error": "uploaded file is too large"}` |
| AI 模型未加载 | 503 | `{"ok": false, "error": "model not ready"}` |
| 推理超时 | 504 | `{"ok": false, "error": "inference timeout"}` |

## 7.7 超时与重试

| 链路 | MVP 超时 | 重试 | 降级 |
|---|---|---|---|
| i.MX→OPi5 AI | `<TODO:MEASURE>` ms，初始建议 1500ms | 0–1 次 | 使用本地传感器结果 |
| i.MX→Flask event | `<TODO:MEASURE>` ms，初始建议 1000ms | 本地 spool 后补发 | 本地继续控制 |
| i.MX 图片上传 | `<TODO:MEASURE>` ms | 可跳过图片 | 事件仍上报 |

> **[CLAUDE_CODE_TODO | MEASURE]** 标定 HTTP 超时与端到端延迟
> - 为何 GPT 给不了：真实网络、图片大小、OPi5 推理速度只能实测。
> - 期望产物/操作：Task07 中记录 capture/ai/post 总耗时，按结果调整超时。
> - 回填位置：docs/07 第 7.7；docs/11；tests/integration
> - 验收：至少 20 次请求统计，并给出最终超时参数。


## 7.8 字段命名规范

- snake_case。
- bool 使用 true/false。
- `risk_score` 与 `risk_hint` 范围都是 0–10。
- bbox 使用 `[x1, y1, x2, y2]` 像素坐标。
- 时间戳使用 ISO 8601 字符串。
- 所有新增字段必须能被旧后端忽略或存入 `raw_json`。

## 7.9 版本号与兼容策略

`contract_version` 初始为 `1.0`。字段只能向后兼容新增，不允许删除旧字段。Flask 扩展时优先保留 `raw_json`，避免 DB schema 变更导致旧事件不可读。
