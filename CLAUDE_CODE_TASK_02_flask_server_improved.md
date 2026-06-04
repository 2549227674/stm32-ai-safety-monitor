# Claude Code 任务 02：搭建 Flask 服务器骨架（改进版）

> 用法：复制本文件正文给本地 Claude Code / Codex 执行  
> 任务类型：新建服务器后端，不涉及 STM32 硬件  
> 位置：`server/backend/`  
> 目标：为 ESP32-CAM HTTP 上报和 Web Dashboard 提供最小可运行第三层

---

## 0. 审核改进摘要

### 0.1 原任务主要问题

1. 原任务只写 `flask>=3.0`，未包含 CORS。ESP32-CAM 直接 HTTP POST 本身不受浏览器 CORS 限制；CORS 主要服务浏览器跨端口调试或未来前端独立端口开发。
2. 原任务未说明服务器要监听 `0.0.0.0`，ESP32-CAM 可能无法从局域网访问 `localhost`。
3. 原任务未限制上传文件大小，也未做文件名安全处理。
4. 原任务未定义数据库表结构，Claude Code 可能各自发挥，导致后续字段不统一。
5. 原任务没有 `GET /api/status/latest` 无数据时的返回约定。
6. 原任务没有 `/health` 接口，不利于 ESP32-CAM 和调试脚本快速判断服务器在线。
7. 原任务没有说明 `risk_score`、`need_snap`、`sensors`、`actuators` 的默认值与校验逻辑。
8. 原任务没有数据库并发/课程项目级性能注意事项，例如 SQLite WAL、每请求连接、limit 上限。

### 0.2 本版补充内容

1. 增加统一事件 JSON Contract，和 DEVPLAN / ESP32-CAM 文档一致。
2. 增加 `flask-cors`，仅对 `/api/*` 开启开发期 CORS。
3. 增加 `/health` 接口。
4. 增加 SQLite 表结构、字段默认值和 JSON 存储策略。
5. 增加安全上传：`MAX_CONTENT_LENGTH`、扩展名白名单、`secure_filename()`。
6. 增加 Dashboard 自动刷新和 API 错误显示要求。
7. 增加 curl 测试、局域网访问和防火墙检查清单。

---

## 1. 项目约束

1. 使用 Flask + SQLite，不用 FastAPI/Django。
2. 第一版不做用户认证。
3. 不做 MQTT。
4. 不做 WebSocket。
5. AI 解释功能只留占位说明，不实现。
6. 前端用最简单的 HTML + CSS + 原生 JavaScript，不用 React/Vue。
7. 所有代码放在 `server/backend/` 目录下。
8. 当前服务器只用于课程项目局域网演示，不作为公网生产服务。

---

## 2. 目标

搭建一个可运行的最简 Flask 应用：

```text
GET  /health             健康检查
POST /api/events         接收事件 JSON，存入 SQLite
GET  /api/events         返回最近 N 条事件，默认 50，最大 200
GET  /api/status/latest  返回最新一条事件
POST /api/images         接收图片上传，multipart/form-data，存到 uploads/
GET  /uploads/<filename> 访问已上传图片
GET  /                   Web Dashboard 页面
```

---

## 3. 目录结构

请创建或更新：

```text
server/backend/
├── app.py
├── database.py
├── requirements.txt
├── uploads/
│   └── .gitkeep
├── templates/
│   └── dashboard.html
├── static/
│   ├── style.css
│   └── dashboard.js
└── README.md
```

---

## 4. requirements.txt

请写入：

```text
Flask>=3.0,<4.0
Flask-Cors>=4.0
```

说明：SQLite 是 Python 标准库，不需要额外依赖。

---

## 5. 统一事件 JSON Contract

ESP32-CAM 或测试工具发送的事件格式：

```json
{
  "type": "event",
  "device_id": "labbox_001",
  "seq": 12,
  "state": "DOOR",
  "risk_score": 3,
  "need_snap": false,
  "sensors": {
    "door": 1,
    "pir": 0,
    "flame": 0,
    "mq2": 0
  },
  "actuators": {
    "relay": 0,
    "fan": 0,
    "pump": 0,
    "buzzer": 0,
    "rgb_r": 0,
    "rgb_g": 0,
    "rgb_b": 0
  }
}
```

服务器收到后自动添加：

```text
event_id：SQLite 自增主键
timestamp：服务器 UTC 或本地 ISO 时间
```

字段默认值：

| 字段 | 默认值 |
|---|---|
| `type` | `event` |
| `device_id` | `unknown` |
| `seq` | `null` |
| `state` | `UNKNOWN` |
| `risk_score` | `0` |
| `need_snap` | `false` |
| `sensors` | `{}` |
| `actuators` | `{}` |

风险分约定：当前统一使用 0-10 分制，`0-2=NORMAL`、`3-5=PRE_ALARM`、`6-8=ALARM`、`>=9=DANGER`。示例值：TEST=0、DOOR=3、SMOKE=6、FLAME/DANGER=9。

兼容性要求：

1. 若收到旧字段 `risk`，可作为 `risk_score` 的 fallback，但返回时统一使用 `risk_score`。
2. `sensors` 和 `actuators` 允许缺字段，但返回时尽量补齐常用字段。
3. 不要因为未知字段拒绝整条事件，未知字段保存在 `raw_json`。

---

## 6. SQLite 数据库设计

数据库文件：

```text
server/backend/safety_monitor.db
```

### 6.1 events 表

```sql
CREATE TABLE IF NOT EXISTS events (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    device_id TEXT NOT NULL,
    seq INTEGER,
    type TEXT NOT NULL,
    state TEXT NOT NULL,
    risk_score INTEGER NOT NULL,
    need_snap INTEGER NOT NULL,
    sensors_json TEXT NOT NULL,
    actuators_json TEXT NOT NULL,
    raw_json TEXT NOT NULL,
    source TEXT
);
```

建议索引：

```sql
CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);
CREATE INDEX IF NOT EXISTS idx_events_device_id ON events(device_id);
```

说明：`sensors_json` 和 `actuators_json` 第一版可以先存 JSON 字符串，便于保持接口简单。如后续需要按单个传感器或执行器筛选事件，可再增加 `door`、`pir`、`flame`、`mq2`、`relay`、`fan`、`pump` 等独立列。

### 6.2 images 表

```sql
CREATE TABLE IF NOT EXISTS images (
    image_id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    device_id TEXT,
    event_id INTEGER,
    filename TEXT NOT NULL,
    url TEXT NOT NULL,
    note TEXT,
    FOREIGN KEY(event_id) REFERENCES events(event_id)
);
```

### 6.3 SQLite 使用要求

1. 应用启动时调用 `init_db()`。
2. 使用每请求独立连接或短生命周期连接，不使用全局共享 cursor。
3. 设置 `row_factory = sqlite3.Row`，便于 JSON 输出。
4. 可执行 `PRAGMA journal_mode=WAL;` 提升课程项目中的读写体验。
5. `GET /api/events` 的 `limit` 最大限制为 200，防止一次返回过多数据。

---

## 7. API 详细要求

### 7.1 GET /health

返回：

```json
{
  "ok": true,
  "service": "safety-monitor-backend",
  "version": "0.1.0"
}
```

用途：ESP32-CAM 或浏览器快速确认服务器在线。

---

### 7.2 POST /api/events

请求：`Content-Type: application/json`

成功返回：

```json
{
  "ok": true,
  "event_id": 1,
  "timestamp": "2026-06-03T23:59:59"
}
```

错误返回示例：

```json
{
  "ok": false,
  "error": "request body must be JSON"
}
```

状态码：

| 情况 | HTTP 状态码 |
|---|---:|
| 成功 | 201 |
| 非 JSON | 400 |
| JSON 格式错误或字段类型不可处理 | 400 |
| 服务器内部错误 | 500 |

实现要求：

1. 读取 JSON 时使用 `request.get_json(silent=True)`。
2. `risk_score` 转为 int，并限制在 0~10。
3. `need_snap` 转为 bool 后以 0/1 存入数据库。
4. `sensors` / `actuators` 用 `json.dumps(..., ensure_ascii=False)` 存储。
5. API 返回时还原为对象，不要返回 JSON 字符串。

---

### 7.3 GET /api/events

请求参数：

```text
limit：可选，默认 50，最大 200
```

返回：

```json
{
  "ok": true,
  "count": 2,
  "events": [
    {
      "event_id": 2,
      "timestamp": "2026-06-03T23:59:59",
      "device_id": "labbox_001",
      "seq": 12,
      "type": "event",
      "state": "DOOR",
      "risk_score": 3,
      "need_snap": false,
      "sensors": {"door": 1, "pir": 0, "flame": 0, "mq2": 0},
      "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0}
    }
  ]
}
```

排序：按 `event_id DESC` 或 `timestamp DESC` 返回最近事件。

---

### 7.4 GET /api/status/latest

有事件时返回：

```json
{
  "ok": true,
  "event": {
    "event_id": 2,
    "state": "DOOR",
    "risk_score": 3,
    "sensors": {"door": 1, "pir": 0, "flame": 0, "mq2": 0},
    "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0}
  }
}
```

无事件时返回：

```json
{
  "ok": true,
  "event": null
}
```

---

### 7.5 POST /api/images

请求：`multipart/form-data`

字段：

| 字段 | 必选 | 说明 |
|---|---|---|
| `image` | 是 | 图片文件 |
| `device_id` | 否 | 设备编号 |
| `event_id` | 否 | 关联事件 ID |
| `note` | 否 | 备注 |

安全要求：

1. 使用 `werkzeug.utils.secure_filename()`。
2. 允许扩展名：`.jpg`、`.jpeg`、`.png`。
3. `MAX_CONTENT_LENGTH = 4 * 1024 * 1024`。
4. 文件名加时间戳或 UUID，避免覆盖。
5. 保存到 `uploads/`。

成功返回：

```json
{
  "ok": true,
  "filename": "20260603_235959_labbox_001.jpg",
  "url": "/uploads/20260603_235959_labbox_001.jpg"
}
```

---

## 8. Flask app.py 要求

1. 应用启动时自动创建 `uploads/`。
2. 应用启动时调用 `init_db()`。
3. 开发期 CORS：

```python
from flask_cors import CORS
CORS(app, resources={r"/api/*": {"origins": "*"}})
```

说明：ESP32-CAM 直接 HTTP POST 不依赖 CORS；CORS 主要服务于浏览器调试或未来前端独立端口开发。

4. 运行入口：

```python
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
```

注意：`0.0.0.0` 是为了让 ESP32-CAM 从局域网访问。不要在公网暴露该开发服务器。

---

## 9. Dashboard 页面要求

`templates/dashboard.html` + `static/style.css` + `static/dashboard.js`。

页面内容：

1. 顶部标题：`安全巡检系统 Dashboard`。
2. 当前状态卡片：STATE、RISK_SCORE、DEVICE、SEQ、TIMESTAMP。
3. 传感器状态：DOOR/PIR/FLAME/MQ2。
4. 执行器状态：RELAY/FAN/PUMP/BUZZER/RGB。
5. 最近 20 条事件列表：时间、状态、风险评分、传感器摘要。
6. API 错误提示区：服务器无响应或 JSON 错误时显示。
7. 每 3 秒自动刷新。

前端实现要求：

1. 使用原生 `fetch()`。
2. 不使用 React/Vue。
3. 如果没有事件，显示 `暂无事件`。
4. 根据 `state` 增加简单颜色 class：
   - `IDLE`：绿色
   - `PIR` / `DOOR`：黄色
   - `SMOKE` / `ALARM`：红色
   - `TEST`：蓝色或灰色

---

## 10. README.md 要求

`server/backend/README.md` 至少包含：

1. 环境要求：Python 3.10+ 建议。
2. 安装命令。
3. 启动命令。
4. 如何查看本机局域网 IP。
5. curl 测试事件。
6. ESP32-CAM 需要配置的 `SERVER_HOST` 和 `SERVER_PORT`。
7. 常见问题：
   - ESP32-CAM 访问不到服务器：检查同一 WiFi、IP、防火墙、`host=0.0.0.0`。
   - 端口 5000 被占用：改端口或关闭占用进程。
   - Dashboard 无数据：先用 curl POST 测试。

---

## 11. 测试方法

### 11.1 启动服务器

```bash
cd server/backend
python -m venv .venv
# Windows PowerShell:
# .venv\Scripts\Activate.ps1
# Git Bash / macOS / Linux:
# source .venv/bin/activate
pip install -r requirements.txt
python app.py
```

### 11.2 发送测试事件

```bash
curl -X POST http://localhost:5000/api/events \
  -H "Content-Type: application/json" \
  -d '{"type":"event","device_id":"test","seq":1,"state":"DOOR","risk_score":3,"need_snap":false,"sensors":{"door":1,"pir":0,"flame":0,"mq2":0},"actuators":{"relay":0,"fan":0,"pump":0,"buzzer":0,"rgb_r":0,"rgb_g":0,"rgb_b":0}}'
```

预期返回：

```json
{"ok": true, "event_id": 1, "timestamp": "..."}
```

### 11.3 查看事件

```bash
curl http://localhost:5000/api/events
curl http://localhost:5000/api/status/latest
curl http://localhost:5000/health
```

### 11.4 浏览器访问

```text
http://localhost:5000/
```

ESP32-CAM 访问时使用电脑局域网 IP：

```text
http://<your-pc-lan-ip>:5000/api/events
```

不要在 ESP32-CAM 中使用 `localhost`。

---

## 12. 禁止事项

1. 不要引入 React/Vue/Vite。
2. 不要引入用户登录系统。
3. 不要引入 MQTT/WebSocket。
4. 不要在第一版实现 AI 调用。
5. 不要把数据库设计成必须依赖外部 MySQL/PostgreSQL。
6. 不要把 Flask debug server 暴露到公网。
7. 不要把上传文件保存到源码根目录。

---

## 13. 完成后请输出

Claude Code / Codex 完成后请输出：

1. 创建的文件列表。
2. 数据库表结构摘要。
3. 如何启动服务器。
4. 如何发送测试事件。
5. Dashboard 页面功能说明。
6. ESP32-CAM 对接地址示例。
7. 已知限制和后续扩展点。
8. 建议 commit message。

建议 commit message：

```text
feat(server): add Flask event API and dashboard skeleton
```

---

## 14. 验证检查清单

- [ ] `pip install -r requirements.txt` 成功。
- [ ] `python app.py` 成功启动。
- [ ] `GET /health` 返回 ok。
- [ ] `POST /api/events` 返回 201 和 `event_id`。
- [ ] `GET /api/events` 能看到事件。
- [ ] `GET /api/status/latest` 能看到最新事件。
- [ ] 浏览器打开 `/` 能看到 Dashboard。
- [ ] Dashboard 每 3 秒刷新。
- [ ] 无事件时 Dashboard 不崩溃。
- [ ] 非 JSON 请求返回 400。
- [ ] 上传非图片文件被拒绝。
- [ ] ESP32-CAM 使用局域网 IP 可以访问 `/health`。
