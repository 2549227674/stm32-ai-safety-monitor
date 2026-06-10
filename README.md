# Edge AI Safety Monitor — 基于 Orange Pi 5 的本地 AI 多模态安全巡检系统

> **当前演示主线**：Orange Pi 5 (RK3588S) 一板主控，运行 `opi5_safetyd`（本地安全闭环）+ `opi5_ai_service`（Qwen3-VL AI 推理）+ `opi5-device-agent`（设备管理），Flask + SQLite + React edge-console 提供本地可视化。
>
> 项目经历 STM32+ESP32-CAM → i.MX6ULL+OPi5 双板 → OPi5 一板三次迭代。历史阶段代码和文档归档在 `legacy/` 和 `docs/archive/`。

## 1. 项目定位

面向《数字系统综合训练》课程的端边协同本地 AI 多模态安全巡检系统。OPi5 同时承担本地安全闭环和 AI 推理，Flask 后端提供事件/遥测/AI/告警/通知存储与 API，React 前端提供多页面 Dashboard。

## 2. 当前架构

| 层级 | 模块 | 核心职责 |
|---|---|---|
| 安全闭环 | `opi5-controller` / `opi5_safetyd` | GPIO/PWM 传感器采样、执行器控制、状态机、离线降级 |
| AI 推理 | `opi5-ai` / `opi5-ai-qwen3vl.service` | Qwen3-VL 视觉推理，`control_allowed=false` |
| 设备管理 | `opi5-device-agent` | 心跳、遥测、视频流、AI observation 编排 |
| 后端 | `server/backend/` (Flask) | 事件/遥测/AI/告警/通知 API，SQLite 存储 |
| 前端 | `server/frontend/` (React) | Dashboard 多页面可视化 |

## 3. 快速开始

### 先读哪几个文件

| 文件 | 给谁看 | 用途 |
|---|---|---|
| `CANONICAL_DECISIONS.md` | 人 + Claude Code | 全局唯一事实源 |
| `CLAUDE.md` | Claude Code | 本地开发规则 |
| `docs/00_README.md` | 人 + 老师 | 设计文档包总览 |
| `docs/07_端边HTTP_JSON_Contract.md` | 人 + Claude Code | API 契约权威来源 |
| `DEVPLAN.md` | 人 + Claude Code | 任务看板 |

### 代码结构

```text
edge/opi5-controller/     安全闭环 C 程序
edge/opi5-ai/             AI 推理 Python/C++ 服务
edge/opi5-device-agent/   设备代理 Python 服务
server/backend/           Flask 后端
server/frontend/          React 前端
config/templates/         systemd/env 模板
scripts/                  构建/部署脚本
```

### 后端 API

详见 `docs/07_端边HTTP_JSON_Contract.md`。主要接口：

- `POST /api/devices/heartbeat` — 设备心跳
- `POST /api/telemetry/batch` — 遥测批量上报
- `POST /api/ai/observations` — AI observation
- `GET /api/video/stream` — 视频流代理
- `GET /api/stream/events` — SSE 事件流
- `POST/GET /api/events` — 旧接口兼容

### 设备数据节奏

| 数据 | 频率 |
|---|---|
| heartbeat | 5s |
| telemetry sample | 1Hz |
| telemetry batch | 30s |
| AI observation | 30s |

## 4. 当前硬件

OPi5 26pin GPIO 直连所有外设（PCA9685 不可用，改用 GPIO/PWM 直控）。

详细接线：`docs/reference/hardware/opi5_current_hardware_wiring.md`

| 信号 | Pin | GPIO | 说明 |
|---|---|---|---|
| I2C5 SDA | 3 | 47 | OLED + MPU6050 |
| I2C5 SCL | 5 | 46 | OLED + MPU6050 |
| Pan servo | 7 | 54 | PWM15 |
| Water MOS | 11 | 138 | active HIGH |
| PIR | 13 | 139 | active HIGH |
| Flame | 15 | 28 | active LOW |
| MQ-2 | 19 | 49 | active LOW |
| RGB R/G/B | 21/23/24 | 48/50/52 | active HIGH |
| Buzzer | 26 | 35 | active LOW |

## 5. 验证

无真实设备时，`opi5-device-agent` 提供 mock sensors 和 mock video。有 online device 时自动切换为 real mode。

```bash
# 语法检查
python -m py_compile server/backend/*.py edge/opi5-device-agent/*.py

# 前端构建
cd server/frontend && npm run build
```

## 6. 历史迭代

| 阶段 | 架构 | 归档位置 |
|---|---|---|
| 第一版 | STM32 + ESP32-CAM | `legacy/2026-stm32-esp32/` |
| 第二版 | i.MX6ULL + OPi5 双板 | `docs/archive/2026-imx6ull-stage/` |
| 当前 | OPi5 一板主控 | 本仓库主线 |

## 7. 入库安全

见 `.gitignore`。禁止提交模型权重、抓拍图片、数据库、SSH 私钥、真实 IP、WiFi 密码。
