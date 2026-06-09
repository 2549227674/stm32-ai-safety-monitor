# 07 端边 HTTP JSON Contract

## 7.1 Contract 目标

本文件是端边数据契约的唯一权威来源。所有代码、测试和 Dashboard 字段必须与本文件保持一致。

目标：

1. 保留旧事件 JSON 的核心语义：`risk_score`、`sensors`、`actuators`、`state`。
2. 将原 STM32↔ESP32-CAM UART JSON 演化为 i.MX6ULL↔OPi5↔Flask 的 HTTP/JSON Contract。
3. AI 只返回 `risk_hint` 和解释，绝不直接控制执行器。
4. 所有接口都能用 curl 单独测试。

## 7.2 i.MX6ULL → OPi5 推理请求

接口：`POST http://<OPI5_AI_URL>/api/infer/vision`

AI URL 按网络方案选择：

| 优先级 | 方案 | AI URL | Flask URL |
|---|---|---|---|
| 1（当前） | 全无线热点 | `http://<OPI5_WIFI_IP>:8080/api/infer/vision` | `http://<PC_WIFI_IP>:5000/api/events` |
| 2 | Windows portproxy 回退 | `http://192.168.137.1:18080/api/infer/vision` | `http://192.168.137.1:5000/api/events` |
| 3 | 全有线回退 | `http://10.0.1.120:8080/api/infer/vision` | `http://<PC_WIRED_IP>:5000/api/events` |

无论网络路径如何变化，JSON contract 不变。AI 仍只返回 `risk_hint` / `summary` / `explanation` / `labels` / `objects`，`control_allowed=false`，不直接控制执行器。OPi5 / Flask / Dashboard 不直接控制执行器。

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
  "actuators": {"relay": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 1},
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

## 7.10 P0 传感器/执行器字段使用说明

P0 阶段使用既有字段，不新增、不改库：

| 字段路径 | P0 用途 |
|---|---|
| `sensors.door` | 门磁 gpio118，0/1 |
| `sensors.pir` | PIR gpio117，0/1 |
| `sensors.flame` | 火焰 gpio119，0/1 |
| `sensors.mq2` | MQ-2 gpio120，0/1 |
| `actuators.buzzer` | 蜂鸣器 gpio122，0/1 |
| `actuators.rgb_r` | 红色 LED gpio121，0/1 |
| `actuators.rgb_g` | 绿色 LED gpio123，0/1 |
| `actuators.rgb_b` | 蓝色 LED gpio124，0/1 |

以下字段保留给 P1/P2，P0 不使用、不新增：
- `keys`：调试按键（P1）

`control_allowed` 仍必须为 `false`。AI/OPi5/Flask 不参与执行器控制。

## 7.11 P1 扩展字段（向后兼容）

P1 只做向后兼容扩展，不改 DB schema。所有新字段通过 `raw_json` 透出，旧字段兼容。

### sensors 新增字段

```json
"sensors": {
  "door": 0, "pir": 1, "flame": 0, "mq2": 0,
  "soc_temp": 47.3
}
```

| 字段 | 类型 | 单位 | 说明 |
|---|---|---|---|
| `soc_temp` | float or null | 摄氏度 | i.MX6ULL SoC 内部温度（Task11-C 已接入），不是环境温度；读取失败时为 null |

- 不加 `humidity`（本轮不做温湿度外设）。
- 不加环境 `temp`。
- 通过 `raw_json` 透出，旧后端可忽略。

### actuators 扩展

```json
"actuators": {
  "relay": 0,
  "pump": 0,
  "buzzer": 0,
  "rgb_r": 0,
  "rgb_g": 0,
  "rgb_b": 0
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `relay` | 0/1 | 继电器，Task11-B 已真实化（PCA9685 CH5，active high，ALARM=1） |
| `pump` | 0/1 | 水泵/水枪双负载，Task11-D 已真实化（PCA9685 CH6，active high，ALARM=1）；pump=1 表示 CH6 喷淋输出动作 |
| `buzzer` | 0/1 | 蜂鸣器，P0 已有 |
| `rgb_r/g/b` | 0/1 或 PWM duty | RGB，P0 已有 |

- AI/OPi5/Flask 不允许直接下发控制。
- `control_allowed=false` 保持不变。

### device_health 扩展（raw_json 透出，Task11-C 已接入）

当 `soc_temp` 超过阈值时，`device_health` 字段从 `NORMAL` 变为 `WARN`，上报 Dashboard/OLED 警告。此字段通过 `raw_json` 透出，不强制 DB/schema 改动。

| 值 | 条件 | 说明 |
|---|---|---|
| `NORMAL` | `soc_temp < SOC_TEMP_WARN_C` | 正常 |
| `WARN` | `soc_temp >= SOC_TEMP_WARN_C` | 过热警告 |
| `UNKNOWN` | soc_temp 读取失败或禁用 | 温度未知 |

## 7.10 OPi5 低速视觉追踪接口（Task12 新增）

### POST /api/track/frame

低延迟追踪端点，i.MX `imx_tracker` 调用，返回目标 bbox 和视觉偏移提示。

请求：`multipart/form-data`

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `image` | file (JPEG) | 是 | 摄像头抓拍帧 |
| `metadata` | JSON string | 是 | `{device_id, frame_id, resolution: {width, height}}` |

响应示例：

```json
{
  "ok": true,
  "contract_version": "1.1",
  "frame_id": 1,
  "latency_ms": 42,
  "objects": [
    {"label": "fire", "score": 0.83, "bbox": [120, 80, 260, 220]}
  ],
  "best_fire": {"label": "fire", "score": 0.83, "bbox": [120, 80, 260, 220]},
  "tracking_hint": {
    "target_label": "fire",
    "target_center": [190, 150],
    "frame_center": [320, 240],
    "offset_px": [-130, -90],
    "confidence": 0.83
  },
  "control_allowed": false
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `objects[].bbox` | [x1,y1,x2,y2] | 目标检测框，像素坐标 |
| `best_fire` | object/null | 最高置信度的 fire/smoke 目标 |
| `tracking_hint.target_center` | [x,y] | 目标中心像素坐标 |
| `tracking_hint.frame_center` | [x,y] | 画面中心像素坐标 |
| `tracking_hint.offset_px` | [dx,dy] | 目标相对画面中心偏移（像素） |
| `tracking_hint.confidence` | float | 目标置信度 |

约束：
- `tracking_hint` 是**感知提示**，不是动作命令
- 不得包含 `pan_delta_deg` / `tilt_delta_deg` / `pump` / `relay` 等控制字段
- `control_allowed` 必须为 `false`
- 角度增量由 i.MX `imx_tracker` 本地计算并限幅

### GET /api/track/stream

低帧率标注 MJPEG 流，Dashboard 展示用。

- 格式：`multipart/x-mixed-replace; boundary=frame`
- 帧率：≤5 fps，640×480
- 标注：bbox 叠框 + label + score
- **仅用于展示，不作为执行器控制源**

## 7.11 i.MX 双确认喷水事件字段（Task12 新增）

事件 `raw_json` 可包含 `alarm_phase` 和 `spray_confirm` 字段：

```json
{
  "alarm_phase": "AIMING",
  "spray_confirm": {
    "enabled": true,
    "local_hazard": true,
    "vision_hazard": false,
    "matched_label": null,
    "matched_score": null,
    "threshold": 0.50,
    "decision": "hold_pump"
  }
}
```

| 字段 | 类型 | 说明 |
|---|---|---|
| `alarm_phase` | string | `IDLE` / `AIMING` / `SUPPRESSING` |
| `spray_confirm.enabled` | bool | 双确认是否启用 |
| `spray_confirm.local_hazard` | bool | 本地传感器触发（flame \|\| mq2） |
| `spray_confirm.vision_hazard` | bool | 视觉确认 fire/smoke 且 score >= 阈值 |
| `spray_confirm.matched_label` | string/null | 视觉匹配的标签 |
| `spray_confirm.matched_score` | float/null | 视觉匹配的分数 |
| `spray_confirm.threshold` | float | 配置的最小 AI 分数阈值 |
| `spray_confirm.decision` | string | `pump_on` / `hold_pump` / `idle` |

喷水真值表：

| local_hazard | vision_hazard | pump | alarm_phase |
|:---:|:---:|:---:|:---:|
| 1 | 1 | ON | SUPPRESSING |
| 1 | 0 | OFF | AIMING |
| 0 | 1 | OFF | IDLE |
| 0 | 0 | OFF | IDLE |

安全约束：
- `pump` 仍只由 i.MX 本地 `imx_safetyd` 控制
- AI/OPi5/Flask 不直接控制 pump
- `SPRAY_MAX_MS` 限时强制停泵
- `SPRAY_COOLDOWN_MS` 冷却期不再触发

Task11-C 已验证通过，证据见 `tests/imx6ull/2026-06-07_p1_soc_temp_health.md`。
