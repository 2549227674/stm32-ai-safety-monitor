# 04 软件架构与模块划分（OPi5 主线版）

## 4.1 总体软件架构

当前演示主线为 OPi5 一板三进程架构：

```text
edge/opi5-controller/     edge/opi5-ai/            edge/opi5-device-agent/
  opi5_safetyd              opi5_ai_service           opi5-device-agent
  gpio_input                /health                   /health
  pwm_servo                 /api/infer/vision         /api/status
  gpio_rgb_buzzer_mos       Qwen3-VL / mock           /api/video/stream
  oled_ssd1306              control_allowed=false      /api/video/snapshot.jpg
  safety_fsm                                           heartbeat (5s)
  event_client                                         telemetry (30s)
  spool                                                AI observation (30s)

server/backend/            server/frontend/
  app.py                     Vite build
  SQLite                     React edge-console
  Flask API                  Dashboard 多页面
```

核心原则：`opi5_safetyd` 是唯一执行器决策点；`opi5_ai_service` 只返回 `risk_hint`，`control_allowed=false`；`opi5-device-agent` 负责心跳/遥测/视频/AI observation。

## 4.2 仓库目录结构

见 `CANONICAL_DECISIONS.md` 的统一目录树。当前活跃代码：

| 目录 | 说明 |
|---|---|
| `edge/opi5-controller/` | OPi5 本地安全闭环 |
| `edge/opi5-ai/` | OPi5 AI 推理服务 |
| `edge/opi5-device-agent/` | OPi5 设备代理 |
| `server/backend/` | Flask 后端 |
| `server/frontend/` | React edge-console |
| `config/templates/` | systemd/env 模板 |
| `scripts/` | 构建/部署脚本 |

## 4.3 历史 i.MX6ULL 控制进程（已完成验证，不再部署）

> 本节为历史 i.MX 阶段的软件架构记录，保留为工程迭代证据。当前主线已迁移到 `opi5_safetyd`。

原 i.MX6ULL 使用 C 版 `imx_safetyd`，通过 V4L2 抓拍 + curl 子进程完成 HTTP 通信。Task07-C 已验证 NORMAL/VERIFY/FAULT 状态机、AI offline fallback、Flask spool/flush。

历史代码位于 `edge/imx6ull-controller/`，不再部署。详见 `docs/archive/2026-imx6ull-stage/`。

## 4.4 OPi5 AI 服务 `opi5_ai_service`

OPi5 AI 服务运行在端口 8080，当前主线为 Qwen3-VL 2B 模型。

- `GET /health` — 服务健康检查
- `POST /api/infer/vision` — 视觉推理，返回 `risk_hint`/`summary`/`objects`/`vlm_result`

处理流程：接收 JPEG + metadata → 预处理 → Qwen3-VL 推理 → 返回结构化 JSON。

详见 `docs/09_OrangePi5_RKNN本地AI推理与解释方案.md`。

## 4.5 OPi5 设备代理 `opi5-device-agent`

设备代理负责设备与后端之间的数据桥接：

| 功能 | 周期 | 说明 |
|---|---|---|
| 心跳 | 5s | `POST /api/devices/heartbeat` |
| 遥测批量上报 | 30s | `POST /api/telemetry/batch` |
| AI observation | 30s | 调用 AI 服务 → `POST /api/ai/observations` |
| 视频流 | 持续 | `/api/video/stream` MJPEG |
| 快照 | 按需 | `/api/video/snapshot.jpg` |

摄像头不可用时自动回退 mock 模式。

## 4.6 Flask 后端

Flask 后端提供完整 API：

| API 组 | 端点 | 说明 |
|---|---|---|
| 设备管理 | `/api/devices/*` | 心跳、设备列表 |
| 遥测 | `/api/telemetry/*` | 批量上报、时序查询 |
| AI 观察 | `/api/ai/observations/*` | observation 上报与查询 |
| 事件 | `/api/events` | 兼容旧事件接口 |
| 阈值 | `/api/thresholds` | 阈值配置 |
| 告警 | `/api/alerts` | 告警列表 |
| 视频代理 | `/api/video/*` | 转发 device-agent 视频 |
| 通知 | `/api/notification/*` | 配置、测试、日志 |
| SSE | `/api/stream/events` | 实时事件流 |

## 4.7 React edge-console

React 前端提供 Dashboard 多页面可视化，展示设备状态、遥测时序、AI observation、视频流、通知设置等。

## 4.8 编译与部署

### OPi5 原生编译

```bash
scripts/build_opi5_controller.sh
scripts/deploy_opi5.sh
```

## 4.9 systemd 服务

OPi5 上三个独立 systemd service：

- `opi5-safetyd.service` — 本地安全闭环
- `opi5-ai-qwen3vl.service` — AI 推理服务
- `opi5-device-agent.service` — 设备代理

三个 service 互不依赖；任一停止不影响其他服务运行。

## 4.10 日志与配置

| 类型 | 路径建议 | 入库 |
|---|---|---|
| 示例配置 | `config/templates/*.env.example` | 是 |
| 本地真实配置 | `/etc/edge-ai-safety-monitor/*.env` | 否 |
| systemd unit | `config/templates/*.service` | 是 |
| 日志 | journald | 否 |

## 4.11 不入库内容

模型权重、数据集、真实配置、SSH 私钥、数据库、图片、视频、日志不入库。见根目录 `.gitignore`。
