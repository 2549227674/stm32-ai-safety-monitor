# 全局唯一事实源 — Canonical Decisions (OPi5 Mainline)

> 最后更新：2026-06-10
> 用途：本文件是全部文档的共同事实源。`README.md`、`docs/00_README.md`、`CLAUDE.md` 与 Task 文档必须与本文件保持一致。

## 0. 当前状态

- **当前主线**：Orange Pi 5 (RK3588S) 一板主控 + PC/Flask/React console
- **当前分支**：`migration/imx6ull-opi5-edge-ai`
- **当前有效代码模块**：`edge/opi5-controller`, `edge/opi5-ai`, `edge/opi5-device-agent`, `server/backend`, `server/frontend`
- **i.MX6ULL / STM32 / ESP32-CAM**：历史阶段，已归档到 `docs/archive/` 和 `legacy/`

## 1. 运行时架构

```text
Orange Pi 5 (RK3588S)
├── opi5_safetyd / opi5-controller    本地安全闭环（GPIO/PWM/MOS/传感器/执行器）
├── opi5-ai-qwen3vl / opi5-ai         Qwen3-VL 推理服务（control_allowed=false）
├── opi5-device-agent                 设备心跳/遥测/视频流/AI observation 编排
└── Flask + SQLite + React edge-console  本地可视化（PC 或 OPi5 部署）
```

### 模块职责

| 模块 | 进程/服务 | 核心职责 | 不承担的职责 |
|---|---|---|---|
| opi5-controller | `opi5_safetyd` | GPIO/PWM 传感器采样、执行器控制、`safety_fsm`、离线降级 | 不运行重型 AI |
| opi5-ai | `opi5-ai-qwen3vl.service` | Qwen3-VL 视觉推理、`risk_hint` 返回 | 不直接控制执行器；`control_allowed=false` |
| opi5-device-agent | `opi5-device-agent.service` | 心跳 5s、遥测采样 1Hz/批量 30s、AI observation 30s、视频流代理 | 不直接控制执行器 |
| Flask backend | `app.py` | 事件/遥测/AI/告警/通知存储、API 提供 | 不直接控制执行器 |
| React edge-console | Vite build → Flask static | Dashboard 多页面可视化 | 不直接控制执行器 |

### 历史架构（已归档）

| 阶段 | 架构 | 状态 |
|---|---|---|
| 第一版 | STM32 + ESP32-CAM → Flask | legacy，归档到 `legacy/2026-stm32-esp32/` |
| 第二版 | i.MX6ULL + OPi5 双板 | archived，归档到 `docs/archive/2026-imx6ull-stage/` |
| 当前 | OPi5 一板双进程 + Flask | **当前主线** |

## 2. 安全边界

- **安全闭环必须留在 OPi5 本地 safetyd/controller 进程**
- AI 服务、Flask 后端、React 前端只展示/解释/通知，不直接控制执行器
- `control_allowed` 始终为 `false`
- 无网络 / 无摄像头 / 无 AI 时，OPi5 safetyd 仍能依据本地传感器进入安全状态

## 3. 当前硬件引脚映射

> 详细接线图：`docs/reference/hardware/opi5_current_hardware_wiring.md`
> 详细引脚表：`docs/reference/hardware/opi5_pinmap_current.md`

| 逻辑信号 | OPi5 物理脚 | GPIO 号 | 方向 | 备注 |
|---|---|---|---|---|
| I2C5 SDA | pin 3 | 47 | I2C | OLED 0x3C + MPU6050 0x68 |
| I2C5 SCL | pin 5 | 46 | I2C | OLED 0x3C + MPU6050 0x68 |
| Pan servo | pin 7 | 54 (PWM15) | PWM | pwmchip4 |
| Water MOS | pin 11 | 138 | output | active HIGH |
| PIR | pin 13 | 139 | input | active HIGH |
| Flame | pin 15 | 28 | input | active LOW |
| MQ-2 | pin 19 | 49 | input | active LOW |
| RGB R | pin 21 | 48 | output | active HIGH |
| RGB G | pin 23 | 50 | output | active HIGH |
| RGB B | pin 24 | 52 | output | active HIGH |
| Buzzer | pin 26 | 35 | output | active LOW |

PCA9685 不可用（模块问题），所有执行器改用 GPIO/PWM 直控。

## 4. 后端 API 契约

权威来源：`docs/07_端边HTTP_JSON_Contract.md`，其他文件只引用，不另造字段。

### 当前 API 列表

```text
POST /api/devices/heartbeat          设备心跳
GET  /api/devices                    设备列表
GET  /api/devices/<device_id>        设备详情

POST /api/telemetry/batch            遥测批量上报
GET  /api/telemetry/series           遥测时序查询

POST /api/ai/observations            AI observation 上报
GET  /api/ai/observations/latest     最新 AI observation

GET/PUT /api/thresholds              阈值配置
GET     /api/alerts                  告警列表

GET /api/video/stream                视频流代理
GET /api/video/snapshot.jpg          快照代理

GET /api/stream/events               SSE 事件流

GET  /api/notification/settings      通知配置
PUT  /api/notification/settings      更新通知配置
POST /api/notification/test-email    测试邮件
GET  /api/notification/logs          通知日志

旧接口（保持兼容）：
POST/GET /api/events
GET /api/status/latest
POST /api/images
GET /uploads/<filename>
```

### 设备数据节奏

| 数据 | 频率 |
|---|---|
| heartbeat | 5s |
| telemetry sample | 1Hz |
| telemetry batch | 30s |
| AI observation | 30s |

### 当前已确认的契约细节

- heartbeat 已包含 `agent_url`、`agent_port`、`camera`（对象）、`camera_status`、`video_mode`、`video_available`
- telemetry canonical sample 使用 `device.*` 嵌套结构（cpu_temp_c、mem_used_mb 等不在顶层）
- `/api/telemetry/series` 支持 metric alias（cpu_temp_c → device.cpu_temp_c 等 8 个）
- notification_log 包含 `dedupe_key` 字段，cooldown 基于 dedupe_key + timestamp + status='sent'
- Flask 视频代理地址解析：OPI5_DEVICE_AGENT_URL → agent_url → ip+agent_port；unknown 时返回 503

## 5. 当前目录树

```text
SafetyMonitor/
├── README.md
├── CANONICAL_DECISIONS.md
├── CLAUDE.md / AGENTS.md
├── DEVPLAN.md
├── CLAUDE_CODE_TASK_01_repo_migration_legacy_archive.md
├── CLAUDE_CODE_TASK_05_opi5_rknn_inference_service.md
├── CLAUDE_CODE_TASK_06_backend_contract_extension.md
├── CLAUDE_CODE_TASK_12_yolo_fire_track_spray.md
├── CLAUDE_CODE_TASK_OPI5_CONTROLLER_PORT.md
├── CLAUDE_CODE_TASK_DOCS_OPI5_ALIGNMENT.md
├── edge/
│   ├── opi5-controller/          当前主线安全闭环
│   ├── opi5-ai/                  当前主线 AI 服务
│   ├── opi5-device-agent/        当前主线设备代理
│   └── imx6ull-controller/       历史归档
├── server/
│   ├── backend/                  Flask 后端
│   └── frontend/                 React edge-console
├── docs/
│   ├── 00–14                     设计文档
│   ├── archive/                  历史归档
│   │   ├── 2026-imx6ull-stage/
│   │   └── 2026-stm32-esp32/
│   ├── reference/
│   │   ├── hardware/             OPi5 硬件参考
│   │   └── api/                  API 参考
│   └── migration/                迁移决策记录
├── scripts/
├── tests/
│   ├── imx6ull/                  历史测试证据
│   ├── opi5/                     OPi5 测试证据
│   ├── integration/              集成测试证据
│   └── legacy/                   STM32/ESP32 测试
├── config/
│   ├── inventory.example.yaml
│   └── templates/                systemd/env 模板
└── legacy/
    └── 2026-stm32-esp32/         STM32/ESP32 归档代码
```

## 6. 网络拓扑

### 当前优先：全无线热点

```text
手机热点 (10.96.98.0/24)
├── PC / Windows (WiFi IP)  — Flask Dashboard
├── OPi5 (WiFi IP)          — AI Service + Safetyd + Device-agent
```

OPi5 同时运行安全闭环、AI 服务和设备代理，无需 i.MX6ULL。

### 回退链路

| 优先级 | 方案 | 说明 |
|---|---|---|
| 1（当前） | OPi5 本地全闭环 | 无需网络即可运行 |
| 2 | PC Flask 远程 | OPi5 → PC WiFi Flask |

## 7. 文件名与编号约定

- 保持 `docs/00`–`docs/14` 数字编号，不新增 `docs/15`
- 历史 i.MX6ULL 任务文件已归档到 `docs/archive/2026-imx6ull-stage/tasks/`，根目录保留 stub
- 设计文档服务报告、答辩与总体方案

## 8. 全局标记规范

```text
> **[CLAUDE_CODE_TODO | 类型]** 一句话说明
> - 为何 GPT 给不了：原因
> - 期望产物/操作：动作与产出
> - 回填位置：文件与小节
> - 验收：完成标准
```

类型：`VERIFY` `FILL` `INVESTIGATE` `MEASURE` `DECIDE`。默认假设用 `> **[ASSUMPTION]** ...`。

## 9. 不可违反的硬约束

1. 本地安全闭环优先；无网络、无摄像头、无 AI 时，OPi5 safetyd 仍要能依据本地传感器进入安全状态。
2. AI / Dashboard / 上层服务只允许返回 `risk_hint`、解释和展示信息，不直接控制蜂鸣器、RGB、水泵。
3. 不接 220V 强电；所有演示负载使用低压直流负载。
4. 舵机与负载独立供电并星形共地；执行器默认 OFF；MOS 栅极下拉。
5. 模型权重、数据集、SSH 私钥、局域网 IP、WiFi 密码、SQLite DB、抓拍图片不入库。
6. 未经实测不得写"已通过"；只能写"设计完成 / 待验证 / 预留"。

## 10. 历史归档索引

| 归档内容 | 位置 |
|---|---|
| i.MX6ULL 任务文件 (Task02/03/04/07/08/10/11) | `docs/archive/2026-imx6ull-stage/tasks/` |
| i.MX6ULL P0/P1 引脚分配 | `docs/archive/2026-imx6ull-stage/hardware/` |
| i.MX6ULL 设计文档 | `docs/archive/2026-imx6ull-stage/docs/` |
| STM32/ESP32 代码 | `legacy/2026-stm32-esp32/` |
| STM32/ESP32 文档 | `docs/archive/2026-stm32-esp32/` |
| 历史测试记录 | `tests/imx6ull/`, `tests/legacy/` |
| 垂直切片清单 | `docs/archive/2026-imx6ull-stage/VERTICAL_SLICE_INTEGRATION_CHECKLIST.md` |
