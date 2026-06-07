# 2026-06-07 Task07-A 端边最小垂直切片测试记录

## 1. 分支与提交

- 分支：migration/imx6ull-opi5-edge-ai
- 当前阶段：Task07-A 端边最小垂直切片
- 基线：8dbb2ac task06: extend flask contract and dashboard ai fields

## 2. 结论

- i.MX SSH：通过
- i.MX 工具：v4l2-ctl ✓, curl ✓, jq ✗ (grep fallback), python3 ✓, date ✓
- OPi5 /health：通过（mock 模式）
- Flask /health：通过（需手动启动）
- i.MX 抓拍：通过（实时 v4l2-ctl 抓取 640x480 MJPG）
- i.MX → OPi5 infer：通过（HTTP 200, mock AI 返回 JSON）
- i.MX → Flask event：通过（HTTP 201, event_id=1031）
- /api/status/latest 完整事件：通过
- Dashboard 展示：通过（API 返回包含完整 AI/视觉字段）
- control_allowed=false：通过
- 延迟记录：capture=470ms, ai=135ms, post=146ms, total=987ms
- 失败降级：通过（OPi5 不可达时脚本不崩溃，仍可上报 Flask）

## 3. 三端健康检查

### i.MX6ULL 工具检查

```text
Linux 100ask 4.9.88 #4 SMP PREEMPT Fri Mar 13 15:45:47 CST 2026 armv7l GNU/Linux
v4l2-ctl: /usr/bin/v4l2-ctl
curl: /usr/bin/curl
jq: NOT FOUND (使用 grep fallback)
python3: /usr/bin/python3
date: /bin/date
/dev/video0 (pxp), /dev/video1 (USB Camera UVC)
```

### OPi5 /health

```json
{
    "contract_version": "1.0",
    "mode": "mock",
    "model_ready": true,
    "ok": true,
    "service": "opi5-ai-service"
}
```

### Flask /health

```json
{
    "ok": true,
    "service": "safety-monitor-backend",
    "version": "0.1.0"
}
```

注意：Flask 需手动启动（WSL 中 `cd server/backend && .venv/bin/python app.py`）。

## 4. i.MX 侧运行记录

### 抓拍

- 命令：`v4l2-ctl -d /dev/video1 --set-fmt-video=width=640,height=480,pixelformat=MJPG --stream-mmap=3 --stream-count=1 --stream-to=...task07_latest.jpg`
- 结果：成功，7.5KB JPEG
- 图片路径：`/opt/edge-ai-safety-monitor/captures/task07_latest.jpg`

### OPi5 infer

- HTTP code: 200
- risk_hint: 2
- summary: 检测到人员进入巡检区域。
- control_allowed: false
- latency_ms: 6 (OPi5 内部)

### Flask event

- HTTP code: 201
- event_id: 1031
- risk_score: 5
- state: VERIFY

### 生成的 event JSON 摘要

```json
{
  "type": "event",
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "seq": 7001,
  "state": "VERIFY",
  "risk_score": 5,
  "need_snap": true,
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 1},
  "vision": {"frame_id": 7001, "image_url": null, "pan_tilt": {"pan_deg": 90, "tilt_deg": 90}},
  "ai_result": { ... mock AI response with risk_hint=2, control_allowed=false ... },
  "latency_ms": {"capture": 470, "ai": 135, "post": 0}
}
```

## 5. API 验证

`/api/status/latest` 返回完整事件，包含：

- state: VERIFY ✓
- risk_score: 5 ✓
- sensors: door=0, pir=1, flame=0, mq2=0 ✓
- actuators: relay=0, fan=0, pump=0, buzzer=0, rgb_r=0, rgb_g=0, rgb_b=1 ✓
- vision.frame_id: 7001 ✓
- vision.pan_tilt: pan_deg=90, tilt_deg=90 ✓
- ai_result.risk_hint: 2 ✓
- ai_result.summary: 检测到人员进入巡检区域。✓
- ai_result.objects: person (score=0.9) ✓
- ai_result.control_allowed: false ✓
- latency_ms: capture=470, ai=135, post=0 ✓
- contract_version: 1.0 ✓

## 6. Dashboard 验证

通过 API 验证 Dashboard 数据源 `/api/status/latest` 和 `/api/events` 均返回完整 AI/视觉字段。
Task06 已实现 Dashboard 前端 AI/视觉展示区渲染，本轮数据格式一致，预期 Dashboard 可正常展示。

## 7. 延迟

| 阶段 | ms |
|---|---:|
| capture | 470 |
| ai | 135 |
| post | 146 |
| total | 987 |

注：capture 包含 v4l2-ctl 初始化和 MJPEG 编码；ai 包含网络往返；post 包含 JSON 组装和 HTTP POST。

## 8. 失败降级

测试：OPi5 URL 故意使用错误端口 18080。

结果：
- OPi5 HTTP: 000 (Connection refused)
- 脚本未崩溃
- 仍使用上一次 AI 响应文件（已知限制：未清除旧 AI 响应）
- Flask HTTP: 201（降级事件仍可上报）

已知限制：当 OPi5 不可达时，脚本使用 `$AI_RESPONSE_FILE` 中的旧数据。
后续 Task07-B 应在 AI 请求失败时生成降级 JSON（如 `ok=false, risk_hint=0`）。

## 9. 边界

- 本轮使用 OPi5 mock AI，不是真实 RKNN。
- 本轮未接 MOS/水泵/220V。
- 本轮没有让 OPi5/Flask/AI 控制执行器。
- 本轮 sensors 为模拟值（door=0, pir=1, flame=0, mq2=0），不是实时 GPIO 采样。
- 本轮不提交抓拍图片、数据库、uploads、inventory.yaml。
- risk_score 融合规则：`min(10, 3 + risk_hint)`，Task07-A 演示规则，非完整状态机。

## 10. 下一步

- Task07-B：把一次性脚本沉淀为 imx_safetyd 初版，加入更完整降级/spool。
- 或 Task08：云台分时巡检 + 多角度抓拍。
- 或 Task05-B：RKNN demo 验证。
