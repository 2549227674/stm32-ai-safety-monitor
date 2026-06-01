# 08 ESP32-CAM 抓拍、Web Dashboard 与服务器方案

## 8.1 ESP32-CAM 职责边界

| 可以做 | 不可以做 |
|---|---|
| 接收 STM32 的 SNAP_REQ 并抓拍 | 直接控制水泵、继电器、蜂鸣器、RGB |
| 可选保存 microSD | 替代 STM32 做风险评分 |
| 提供 Web Dashboard 页面 | 决定是否进入 ALARM/DANGER |
| HTTP 上传事件和图片至服务器 | 作为安全动作前置条件 |
| 请求云端 AI JSON 并中转结果 | 远程控制关键执行器 |
| 返回网络/SD/摄像头状态给 STM32 | |

---

## 8.2 抓拍链路

```text
STM32 进入 ALARM/DANGER
→ 生成 event_id → UART 发送 SNAP_REQ(event_id, risk_score)
→ ESP32-CAM 返回 ACK
→ ESP32-CAM 调用 camera_fb_get() 拍照
→ 成功：返回 SNAP_OK(event_id, img_id)
    ├→ 可选：保存到 microSD
    ├→ 可选：缓存为 Web latest.jpg
    ├→ 可选：HTTP POST 上传至服务器
    └→ 可选：请求云端 AI → 回传 AI_RESULT
→ 失败：返回 SNAP_FAIL(event_id, err_code)
→ STM32 OLED 显示抓拍结果
```

---

## 8.3 心跳机制

| 参数 | 建议 |
|---|---|
| 周期 | STM32 每 2 秒发送 PING |
| 返回 | ESP 返回 PONG，包含 cam_ok、wifi_ok、sd_ok |
| 离线判定 | 连续 3 次失败 |
| 恢复判定 | 连续 2 次成功 |
| OLED | CAM_OK / CAM_OFFLINE / NET_OFFLINE |

---

## 8.4 拍照失败处理

| 失败类型 | ESP 返回 | STM32 处理 |
|---|---|---|
| 摄像头初始化失败 | CAM_INIT_FAIL | OLED 显示 CAM_FAIL，本地报警继续 |
| 拍照失败 | SNAP_FAIL_CAMERA | 记录事件，本地报警继续 |
| ESP 正忙 | BUSY | STM32 延迟重试一次 |
| ACK 超时 | 无响应 | STM32 重传 3 次后 CAM_OFFLINE |

---

## 8.5 三种 Web/服务器实现梯度

### 方案 A：局域网 Web Dashboard（最容易落地）

ESP32-CAM 自身作为 HTTP 服务器提供页面。

实现方式：ESP32-CAM 启动 HTTP Server，提供 `/` 主页面、`/status` 状态 JSON、`/latest.jpg` 最近图片。手机和电脑通过同一 Wi-Fi 用浏览器访问 ESP32-CAM 的 IP。

页面内容：设备状态卡片（state, risk_level, risk_score）、传感器数值列表、最近抓拍图片预览、最近事件记录（内存中最近 5 条）、CAM/NET/AI 状态标志。

优势：不需要外部服务器，ESP32-CAM 自带一切。适合时间紧张时快速实现。手机浏览器直接访问，可以表述为"手机端查看"。

局限：ESP32-CAM 资源有限，存储和历史查询能力弱。

### 方案 B：本地服务器 + Web Dashboard（推荐冲 A 类）

笔记本电脑运行 Flask/FastAPI 轻量后端，ESP32-CAM 将事件和图片通过 HTTP POST 上传。

服务器功能：
- `POST /api/event` 接收事件 JSON
- `POST /api/upload` 接收抓拍图片
- `GET /api/status` 返回最新设备状态
- `GET /api/events` 返回历史事件列表
- `GET /api/events/{id}` 返回事件详情（含 AI 结果）
- `POST /api/ai_analyze` 调用云端 AI 并存储结果
- `GET /` 提供 Web Dashboard HTML 页面

存储方式：SQLite 或 JSON 文件。图片保存在 `uploads/` 目录。

手机端访问：手机和笔记本连同一 Wi-Fi，手机浏览器访问 `http://<笔记本IP>:5000`。

优势：历史查询、图片持久化、AI 集成、正式的 REST API 设计。答辩可以展示完整端云数据流。

### 方案 C：MQTT + 可选云端（加分项）

在方案 B 基础上增加 MQTT 上报通道。

MQTT Topic：
```text
labbox/labbox_001/status      → 周期状态 JSON
labbox/labbox_001/event       → 异常事件 JSON
labbox/labbox_001/ai_result   → AI 解释结果 JSON
```

可选接入公有云 MQTT Broker（如 EMQX Cloud 免费版）或本地 Mosquitto。

优势：展示物联网通信能力，手机端 MQTT 客户端也能订阅查看。

局限：增加调试成本，公网 MQTT 有网络依赖。

推荐策略：如果时间紧，至少做方案 A；如果想冲 A 类，争取做方案 B；方案 C 作为加分项。

---

## 8.6 Web Dashboard 页面设计

### 首页（dashboard.html）

```text
┌─────────────────────────────────────────────┐
│  🏭 端云协同 AI 安全巡检系统                     │
│  设备: labbox_001    状态: ● ALARM              │
├─────────────────────────────────────────────┤
│ ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│ │ 风险等级  │  │ 烟雾     │  │ 温度     │    │
│ │ ALARM    │  │ 1850     │  │ 46.2°C   │    │
│ │ 分数: 7  │  │ ▓▓▓▓░░   │  │ ▓▓▓░░░   │    │
│ └──────────┘  └──────────┘  └──────────┘    │
│                                             │
│ 传感器状态                                    │
│   火焰: 正常  PIR: 触发  门磁: 关闭            │
│   湿度: 55%  震动RMS: 0.12g                  │
│                                             │
│ 执行器状态                                    │
│   蜂鸣器: 快响  RGB: 红色  风扇: 开启           │
│   继电器: 正常  水泵: 关闭                      │
│                                             │
│ 最近抓拍                                      │
│ ┌────────────────────────────┐               │
│ │  [E000012.jpg 预览图]       │               │
│ │  2026-06-01 10:30:00       │               │
│ └────────────────────────────┘               │
│                                             │
│ AI 分析结果                                    │
│   类型: 疑似烟雾    置信度: 82%                │
│   场景: 图像中疑似存在轻微烟雾                  │
│   建议: 检查设备柜内部是否有过热元件             │
│   AI 状态: ✅ 可用                             │
│                                             │
│ 最近事件                                      │
│  E000012 | SMOKE_ALARM | 10:30 | SNAP_OK     │
│  E000011 | DOOR_OPEN   | 10:25 | SNAP_OK     │
│  E000010 | PIR_DETECT  | 10:20 | SNAP_FAIL   │
├─────────────────────────────────────────────┤
│ 设备状态: CAM ✅  NET ✅  AI ✅  SD ⚠️         │
└─────────────────────────────────────────────┘
```

### 历史事件页面

点击"历史事件"可查看完整事件列表，每条事件包含：事件 ID、事件类型、时间戳、风险等级、传感器快照、抓拍图片缩略图、AI 解释结果。

### 手机端适配

Web 页面使用响应式布局，手机浏览器竖屏可正常查看。不需要开发原生 APP，手机浏览器/PWA 即可表述为"手机端查看"。

---

## 8.7 服务器 REST API 设计（方案 B）

| 方法 | 路径 | 说明 | 请求体 | 返回 |
|---|---|---|---|---|
| GET | `/` | Web Dashboard 首页 | - | HTML |
| GET | `/api/status` | 最新设备状态 | - | 状态 JSON |
| POST | `/api/event` | 上报异常事件 | 事件 JSON | {ok, event_id} |
| POST | `/api/upload` | 上传抓拍图片 | multipart/form-data | {ok, filename} |
| GET | `/api/events` | 历史事件列表 | ?limit=20 | [事件数组] |
| GET | `/api/events/{id}` | 事件详情 | - | 事件+AI 结果 JSON |
| POST | `/api/ai_analyze` | 触发 AI 分析 | {event_id, image_path} | AI 结果 JSON |
| GET | `/api/health` | 服务器健康检查 | - | {status: "ok"} |
| GET | `/uploads/{filename}` | 获取图片文件 | - | 图片 |

---

## 8.8 服务器简化部署

```bash
# 安装依赖
pip install flask

# 启动服务器
python server.py
# 默认监听 0.0.0.0:5000

# 手机端访问
# 手机连同一 Wi-Fi，浏览器打开 http://<笔记本IP>:5000
```

服务器可以是答辩时笔记本上运行的 Python 脚本，不需要云服务器。数据存储在本地 SQLite 或 JSON 文件。

---

## 8.9 ESP32-CAM 上传流程

```text
ESP32-CAM 抓拍成功后：
1. 构造事件 JSON（event_id, sensor_snapshot, risk_level 等）
2. HTTP POST /api/event 上传事件 JSON
3. HTTP POST /api/upload 上传图片文件（multipart）
4. 服务器存储后返回 ok
5. 可选：ESP32-CAM 或服务器调用 /api/ai_analyze 触发 AI 分析
6. ESP32-CAM UART 返回 UPLOAD_OK 或 UPLOAD_FAIL 给 STM32

上传失败处理：
- HTTP 超时或错误 → ESP32-CAM 返回 UPLOAD_FAIL
- STM32 OLED 显示上传失败标志
- 本地报警不受影响
- 事件可缓存在 ESP32-CAM RAM 中等待重试（可选）
```

---

## 8.10 数据字段说明

事件上传 JSON 字段：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| event_id | string | 是 | 事件唯一 ID，如 "E000012" |
| device_id | string | 是 | 设备编号 |
| timestamp | string | 推荐 | ISO8601 格式 |
| event_type | string | 是 | SMOKE_ALARM / FLAME / DOOR_OPEN / VIBRATION / PIR 等 |
| risk_level_local | string | 是 | normal / warning / alarm / danger |
| risk_score | int | 是 | 0-15 |
| sensor_snapshot | object | 是 | 传感器数据快照 |
| local_action | string | 是 | 本地执行器动作 |
| system_state | string | 是 | 系统状态 |
| image_filename | string | 是 | 图片文件名 |
| cam_status | string | 推荐 | online / offline |

---

## 8.11 图片上传方式

推荐使用 HTTP multipart/form-data 上传：

```text
POST /api/upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----boundary

------boundary
Content-Disposition: form-data; name="event_id"

E000012
------boundary
Content-Disposition: form-data; name="image"; filename="E000012.jpg"
Content-Type: image/jpeg

<图片二进制数据>
------boundary--
```

服务器保存到 `uploads/E000012.jpg`。

如果 multipart 实现复杂，可简化为：ESP32-CAM 将图片 base64 编码放入 JSON POST；但文件大小会增大约 33%。

---

## 8.12 断网/离线降级演示

演示流程：

1. 正常触发报警，ESP32-CAM 抓拍成功，Web Dashboard 显示事件和 AI 结果。
2. 拔掉 ESP32-CAM 电源或断开 UART。
3. 再次触发烟雾/火焰/门磁异常。
4. OLED 显示 CAM_OFFLINE。
5. 蜂鸣器、RGB、风扇/继电器/水泵动作仍由 STM32 执行。
6. Web Dashboard 显示设备离线或最后已知状态。

答辩说明：摄像头、网络、服务器、AI 是增强模块，不参与本地安全闭环。

---

## 8.13 不建议实现的功能

| 功能 | 原因 |
|---|---|
| ESP 直接控制继电器/水泵 | 违反 STM32 主控边界 |
| Web 页面远程控制关键执行器 | 安全边界不清 |
| 复杂公网强依赖 | 现场网络不可控 |
| 在 ESP32-CAM 上跑复杂图像识别 | 算力不够 |
| 原生手机 APP | 手机浏览器 Web/PWA 已足够 |
| USB 免驱摄像头替代 ESP32-CAM | 只能用于 PC 辅助测试 |
