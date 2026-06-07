# 2026-06-07 Task06 Flask Backend Contract 扩展测试记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前阶段：Task06 Flask 契约扩展
- 基线：`414e3f0 task05-a: bring up opi5 mock ai service`

## 2. 结论

- 旧事件兼容：通过
- 新 AI 事件 POST：通过
- `/api/status/latest` 扩展字段：通过
- `/api/events` 扩展字段：通过
- Dashboard AI/视觉展示：Playwright headless 浏览器检查通过
- DB schema：未修改
- `safety_monitor.db`：未提交

## 3. 修改摘要

- `database.py`：从 `raw_json` 透出 `contract_version`、`vision`、`ai_result`、`image_url`、`latency_ms`；旧事件缺字段时返回 `null`。
- `dashboard.html/js/css`：新增 AI/视觉展示区，展示 AI 状态、`risk_hint`、summary、objects、faces、image 链接/预览、latency、pan/tilt、`control_allowed`。
- `app.py`：无改动；现有 `normalize_event()` 已把完整 payload 保存到 `raw_json`，无需拆字段或改 schema。

## 4. 命令与输出摘要

### 4.1 语法检查

默认 `python3` 未安装 Flask，已使用仓库现有虚拟环境：

```text
server/backend/.venv/bin/python -m py_compile server/backend/app.py server/backend/database.py
结果：通过，无输出。

node --check server/backend/static/dashboard.js
结果：通过，无输出。
```

虚拟环境版本：

```text
flask 3.1.3
flask_cors 6.0.2
```

### 4.2 启动 Flask

```bash
cd server/backend
.venv/bin/python app.py
```

输出摘要：

```text
* Serving Flask app 'app'
* Debug mode: off
* Running on http://127.0.0.1:5000
* Running on http://172.27.158.164:5000
```

### 4.3 `/health`

```bash
curl -sS http://127.0.0.1:5000/health | python3 -m json.tool
```

输出：

```json
{
  "ok": true,
  "service": "safety-monitor-backend",
  "version": "0.1.0"
}
```

### 4.4 旧事件 POST

命令摘要：

```bash
curl -sS -X POST http://127.0.0.1:5000/api/events \
  -H 'Content-Type: application/json' \
  -d '{"type":"event","device_id":"labbox_001","seq":9001,"state":"NORMAL","risk_score":0,"need_snap":false,"sensors":{"door":0,"pir":0,"flame":0,"mq2":0},"actuators":{"relay":0,"fan":0,"pump":0,"buzzer":0,"rgb_r":0,"rgb_g":1,"rgb_b":0}}'
```

输出摘要：

```json
{
  "event_id": 1029,
  "ok": true,
  "risk_score": 0,
  "timestamp": "2026-06-07T02:41:40+00:00"
}
```

### 4.5 旧事件 `/api/status/latest`

输出摘要：

```json
{
  "ok": true,
  "event": {
    "event_id": 1029,
    "device_id": "labbox_001",
    "seq": 9001,
    "state": "NORMAL",
    "risk_score": 0,
    "need_snap": false,
    "sensors": {"door": 0, "flame": 0, "mq2": 0, "pir": 0},
    "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 1, "rgb_b": 0},
    "contract_version": null,
    "vision": null,
    "ai_result": null,
    "image_url": null,
    "latency_ms": null
  }
}
```

### 4.6 新 AI 事件 POST

命令：

```bash
curl -sS -X POST http://127.0.0.1:5000/api/events \
  -H 'Content-Type: application/json' \
  -d @tests/payloads/event_with_ai.json \
  | python3 -m json.tool
```

输出：

```json
{
  "event_id": 1030,
  "ok": true,
  "risk_score": 5,
  "timestamp": "2026-06-07T02:41:47+00:00"
}
```

### 4.7 新 AI 事件 `/api/status/latest`

输出摘要：

```json
{
  "ok": true,
  "event": {
    "event_id": 1030,
    "contract_version": "1.0",
    "device_id": "labbox_001",
    "seq": 1,
    "state": "VERIFY",
    "risk_score": 5,
    "vision": {
      "frame_id": 1,
      "image_url": "/uploads/example.jpg",
      "pan_tilt": {"pan_deg": 90, "tilt_deg": 90}
    },
    "ai_result": {
      "ok": true,
      "risk_hint": 4,
      "summary": "mock: detected person",
      "objects": [{"label": "person", "score": 0.9, "bbox": [10, 20, 100, 200]}],
      "faces": [],
      "control_allowed": false
    },
    "image_url": "/uploads/example.jpg",
    "latency_ms": {"ai": 0, "capture": 0, "post": 0}
  }
}
```

### 4.8 `/api/events?limit=5`

输出摘要：

```text
count=5
最新事件 event_id=1030 含 vision/ai_result/image_url/latency_ms
上一条旧事件 event_id=1029 扩展字段为 null
```

## 5. API 字段核验

- `vision`：出现
- `ai_result`：出现
- `image_url`：出现
- `latency_ms`：出现
- `ai_result.control_allowed=false`：出现且保持 false

## 6. Dashboard 核验

页面 HTML 可访问：

```bash
curl -sS http://127.0.0.1:5000/
```

已确认 HTML 包含：

- 当前状态区
- AI / 视觉分析区
- 传感器/执行器区
- 最近事件表 AI 摘要列

Node DOM smoke：

```text
apiState=API OK
currentState=VERIFY
aiState=已接收 AI/视觉结果
riskHint=4
controlAllowed=false
eventsRowsHasAi=true
```

Playwright headless 浏览器检查：

```text
apiState=API OK
currentState=VERIFY
riskScore=5
aiState=已接收 AI/视觉结果
riskHint=4
controlAllowed=false
aiSummary=mock: detected person
latencyMs=capture:0ms ai:0ms post:0ms
objects=person 0.90
eventsHasAi=true
pageErrors=[]
console=404 for /uploads/example.jpg and browser resource request; no JS exception
```

浏览器查看结论：

- 状态区是否正常：Playwright headless 通过
- 传感器/执行器是否正常：Playwright headless 页面加载通过，API 数据正常渲染
- AI/视觉区是否正常：Playwright headless 通过
- 旧事件缺字段时是否不报错：旧事件 API 通过；前端代码判空；页面无 JS `pageerror`
- 图片 URL 不存在时是否不崩溃：Playwright headless 触发 404 后无 JS `pageerror`，页面显示图片缺失提示

## 7. 边界

- 本轮未进入 Task07 端边垂直切片。
- 本轮未修改 OPi5 mock/RKNN 服务。
- 本轮未接真实 RKNN。
- 本轮未让 Flask/AI 直接控制执行器。
- 本轮未提交 `safety_monitor.db`、`uploads/`、图片、模型、`inventory.yaml`。

## 8. 下一步

- Task07：i.MX6ULL → OPi5 mock AI → Flask Dashboard 垂直切片。
- 或 Task05-B：RKNN demo 资产盘点与最小 demo 运行验证。
