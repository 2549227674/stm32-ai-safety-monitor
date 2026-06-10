# 07 端边 HTTP JSON Contract

> 最后更新：2026-06-10
> 当前主线：OPi5 一板主控 + Flask 后端

## 7.1 Contract 目标

本文件是端边数据契约的唯一权威来源。所有代码、测试和 Dashboard 字段必须与本文件保持一致。

目标：

1. 保留旧事件 JSON 的核心语义：`risk_score`、`sensors`、`actuators`、`state`。
2. 定义 OPi5 safetyd → Flask、OPi5 device-agent → Flask、OPi5 AI → safetyd 的完整契约。
3. AI 只返回 `risk_hint` 和解释，绝不直接控制执行器。
4. 所有接口都能用 curl 单独测试。

## 7.2 当前 API 总览

### Device Management

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/api/devices/heartbeat` | 设备心跳（5s） |
| GET | `/api/devices` | 设备列表 |
| GET | `/api/devices/<device_id>` | 设备详情 |

### Telemetry

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/api/telemetry/batch` | 遥测批量上报（30s） |
| GET | `/api/telemetry/series` | 遥测时序查询 |

### AI Observations

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/api/ai/observations` | AI observation 上报（30s） |
| GET | `/api/ai/observations/latest` | 最新 AI observation |

### Thresholds & Alerts

| 方法 | 路径 | 说明 |
|---|---|---|
| GET/PUT | `/api/thresholds` | 阈值配置 |
| GET | `/api/alerts` | 告警列表 |

### Video

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/video/stream` | 视频流代理 |
| GET | `/api/video/snapshot.jpg` | 快照代理 |

### SSE

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/stream/events` | SSE 事件流 |

### Notification

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/notification/settings` | 通知配置 |
| PUT | `/api/notification/settings` | 更新通知配置 |
| POST | `/api/notification/test-email` | 测试邮件 |
| GET | `/api/notification/logs` | 通知日志 |

### Legacy（保持兼容）

| 方法 | 路径 | 说明 |
|---|---|---|
| POST/GET | `/api/events` | 旧事件接口 |
| GET | `/api/status/latest` | 最新状态 |
| POST | `/api/images` | 图片上传 |
| GET | `/uploads/<filename>` | 图片访问 |

## 7.3 OPi5 AI 推理请求

接口：`POST http://<OPI5_AI_URL>/api/infer/vision`

AI 服务运行在 OPi5 本地（端口 8080），由 `opi5_safetyd` 或 `opi5-device-agent` 调用。

Content-Type：`multipart/form-data`

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `image` | file | 是 | JPEG/PNG/WebP，MVP 优先 JPEG |
| `metadata` | JSON string | 是 | 设备、帧号、传感器等 |

`metadata` 示例：

```json
{
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "frame_id": 42,
  "timestamp": "2026-06-10T12:00:00Z",
  "source": "opi5_usb_camera",
  "image_format": "jpeg",
  "resolution": {"width": 640, "height": 480},
  "sensors": {"pir": 1, "flame": 0, "mq2": 0},
  "state_before_ai": "VERIFY",
  "risk_score_before_ai": 3
}
```

## 7.4 OPi5 AI 推理响应

```json
{
  "ok": true,
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "frame_id": 42,
  "latency_ms": 86,
  "models": [{"name": "qwen3-vl-2b", "version": "rkllm-0.1"}],
  "objects": [
    {"label": "person", "score": 0.91, "bbox": [120, 80, 260, 430]}
  ],
  "faces": [],
  "risk_hint": 4,
  "summary": "检测到人员进入巡检区域。",
  "action_hint": "keep_alarm",
  "control_allowed": false
}
```

字段要求：

| 字段 | 类型 | 要求 |
|---|---|---|
| `ok` | bool | 请求是否成功 |
| `latency_ms` | int | OPi5 服务内部耗时 |
| `objects` | list | 每个对象包含 `label/score/bbox`（VLM 可为空） |
| `faces` | list | 可为空 |
| `risk_hint` | int 0–10 | AI 风险提示 |
| `summary` | string | Dashboard 展示 |
| `action_hint` | string | 只作提示，不作为执行器命令 |
| `control_allowed` | bool | 必须为 `false` |

### VLM 模型扩展字段

对于 VLM 类模型（如 Qwen3-VL），可额外返回：

| 字段 | 类型 | 说明 |
|---|---|---|
| `explanation` | string | VLM 完整输出文本 |
| `vlm_result` | object | VLM 专属结果 |
| `vlm_result.model` | string | 模型名 |
| `vlm_result.description` | string | VLM 描述文本 |
| `vlm_result.labels` | list[string] | 提取的标签 |
| `vlm_result.confidence` | float\|null | VLM 通常无精确置信度 |
| `vlm_result.raw_text` | string | 原始输出文本 |
| `vlm_result.timing` | object | 推理耗时详情 |

## 7.5 OPi5 safetyd → Flask 事件上报

接口：`POST http://<BACKEND_HOST>:5000/api/events`

```json
{
  "type": "event",
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "seq": 1001,
  "state": "VERIFY",
  "risk_score": 5,
  "need_snap": true,
  "sensors": {"pir": 1, "flame": 0, "mq2": 0},
  "actuators": {"buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 1, "pump": 0},
  "vision": {
    "frame_id": 42,
    "image_url": "/uploads/20260610_labbox_001_xxxx.jpg"
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

## 7.6 device-agent 心跳上报

接口：`POST http://<BACKEND_HOST>:5000/api/devices/heartbeat`

```json
{
  "device_id": "labbox_001",
  "timestamp": "2026-06-10T12:00:00Z",
  "services": {
    "safetyd": "running",
    "ai_service": "running",
    "device_agent": "running"
  },
  "video_available": true,
  "video_url": "http://127.0.0.1:5000/api/video/stream?device_id=labbox_001"
}
```

## 7.7 device-agent 遥测批量上报

接口：`POST http://<BACKEND_HOST>:5000/api/telemetry/batch`

```json
{
  "device_id": "labbox_001",
  "series": [
    {
      "timestamp": "2026-06-10T12:00:00Z",
      "sensors": {"pir": 0, "flame": 0, "mq2": 0},
      "state": "NORMAL",
      "risk_score": 0,
      "soc_temp": 47.3
    }
  ]
}
```

## 7.8 device-agent AI observation 上报

接口：`POST http://<BACKEND_HOST>:5000/api/ai/observations`

```json
{
  "device_id": "labbox_001",
  "timestamp": "2026-06-10T12:00:00Z",
  "summary": "当前环境正常，未检测到异常。",
  "full_text": "画面中未发现人员或异常物体...",
  "risk_hint": 0,
  "objects": [],
  "run_metrics": {"latency_ms": 86},
  "model": "qwen3-vl-2b"
}
```

## 7.9 图片上传

Flask 已支持 `POST /api/images`：

| 字段 | 类型 | 说明 |
|---|---|---|
| `image` | file | 图片文件 |
| `device_id` | form | 设备 ID |
| `event_id` | form | 可选，关联事件 |
| `note` | form | 说明 |

## 7.10 错误码

| 场景 | HTTP | JSON |
|---|---|---|
| 请求成功 | 200/201 | `{"ok": true}` |
| 请求体错误 | 400 | `{"ok": false, "error": "..."}` |
| 文件过大 | 413 | `{"ok": false, "error": "uploaded file is too large"}` |
| AI 模型未加载 | 503 | `{"ok": false, "error": "model not ready"}` |
| 推理超时 | 504 | `{"ok": false, "error": "inference timeout"}` |

## 7.11 超时与重试

| 链路 | 超时 | 重试 | 降级 |
|---|---|---|---|
| safetyd→AI | ~5-13s（Qwen3-VL） | 0–1 次 | 使用本地传感器结果 |
| safetyd→Flask event | ~1000ms | 本地 spool 后补发 | 本地继续控制 |
| device-agent→Flask | ~1000ms | 重试一次 | 本地缓存 |

## 7.12 字段命名规范

- snake_case。
- bool 使用 true/false。
- `risk_score` 与 `risk_hint` 范围都是 0–10。
- bbox 使用 `[x1, y1, x2, y2]` 像素坐标。
- 时间戳使用 ISO 8601 字符串。
- 所有新增字段必须能被旧后端忽略或存入 `raw_json`。

## 7.13 版本号与兼容策略

`contract_version` 初始为 `1.0`。字段只能向后兼容新增，不允许删除旧字段。

## 7.14 当前传感器/执行器字段

OPi5 GPIO 直控：

| 字段路径 | 用途 | OPi5 GPIO |
|---|---|---|
| `sensors.pir` | PIR motion | pin13 / gpio139 |
| `sensors.flame` | 火焰检测 | pin15 / gpio28 |
| `sensors.mq2` | 烟雾/气体 | pin19 / gpio49 |
| `actuators.buzzer` | 蜂鸣器 | pin26 / gpio35 |
| `actuators.rgb_r` | 红色 LED | pin21 / gpio48 |
| `actuators.rgb_g` | 绿色 LED | pin23 / gpio50 |
| `actuators.rgb_b` | 蓝色 LED | pin24 / gpio52 |
| `actuators.pump` | Water MOS | pin11 / gpio138 |

AI 服务 / Flask / 前端不控制执行器；只有 OPi5 本地 `opi5_safetyd` 控制执行器，`control_allowed=false`。

## 7.15 可选/未来接口

以下接口为可选或未来扩展，不在当前主线契约中：

### /api/track/frame（可选）

低延迟追踪端点，用于 AI 返回目标 bbox 和视觉偏移提示。角度增量由 safetyd 本地计算并限幅。`control_allowed=false`。

### /api/track/stream（可选）

低帧率标注 MJPEG 流，仅用于 Dashboard 展示，不作为执行器控制源。

## 7.16 历史 P1 扩展字段（i.MX 阶段，已归档）

> 以下字段属于历史 i.MX 阶段 Task11 P1 扩展，不再作为当前主线使用。

历史 i.MX 阶段曾使用以下扩展字段，通过 `raw_json` 透出：

- `sensors.door`：门磁（gpio118）
- `sensors.soc_temp`：SoC 温度
- `actuators.relay`：继电器（PCA9685 CH5）
- `actuators.pump`：水泵（PCA9685 CH6）
- `device_health`：NORMAL/WARN/UNKNOWN

这些字段在历史事件数据中可能存在，后端通过 `raw_json` 兼容读取。当前 OPi5 主线不使用 `door`、`relay` 字段。
