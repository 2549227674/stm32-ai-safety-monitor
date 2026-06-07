# 04 软件架构与模块划分（Linux 边缘控制版）

## 4.1 总体软件架构

```text
edge/imx6ull-controller/        edge/opi5-ai/               server/backend/
  imx_safetyd                      opi5_ai_service             Flask app.py
  gpio_input                       /health                     SQLite database.py
  pca9685_servo                    /api/infer/vision           Dashboard JS/HTML
  mos_output                       RKNN model wrapper          /api/events
  v4l2_capture                     optional llm_explain        /api/images
  safety_fsm
  event_client
```

核心原则：i.MX6ULL 本地状态机是唯一执行器决策点；OPi5 与 Flask 是服务端，不直接控制硬件。

## 4.2 仓库目录结构

见 `CANONICAL_DECISIONS.md` 的统一目录树。新代码放在 `edge/`、`common/`、`scripts/`、`tests/`，旧代码迁移到 `legacy/`。

## 4.3 i.MX6ULL 控制进程 `imx_safetyd`

建议先用 C/C++ 实现底层硬件控制和 V4L2 抓拍，必要时配合 Python 辅助脚本。MVP 可采用单进程多模块架构。

| 模块 | 职责 | 周期/触发 |
|---|---|---|
| `gpio_input` | 读取门磁/PIR/火焰/按键 | 20–100ms |
| `pca9685_servo` | I2C 控制舵机 PWM | 事件触发或巡检周期 |
| `mos_output` | 控制低压负载 | 状态机输出 |
| `v4l2_capture` | 抓 JPEG 静帧 | `need_snap` 或巡检角度。Task04 已验证：v4l2-ctl 抓取 /dev/video1 MJPG 640x480 成功（UVC 1.00, 0bdc:8088） |
| `safety_fsm` | 计算 `state/risk_score` | 50–100ms |
| `ai_client` | POST 图片到 OPi5 | 事件触发，超时降级 |
| `event_client` | POST 事件到 Flask | 事件触发或定时 |
| `local_spool` | 断网缓存 | 网络失败时 |

> **[ASSUMPTION]** 传感器与安全状态机属于人机尺度场景，Linux 用户态 50–100ms 周期可满足课程 MVP。 —— 若不成立，对应 [CLAUDE_CODE_TODO] 处理。


> **[CLAUDE_CODE_TODO | MEASURE]** 实测 i.MX6ULL 控制循环抖动和端到端延迟
> - 为何 GPT 给不了：沙箱无法在目标板运行控制进程。
> - 期望产物/操作：在 Task07 中记录采样、抓拍、AI 请求、上报的时间戳，计算 min/avg/max。
> - 回填位置：docs/04 第 4.3；docs/11；tests/integration
> - 验收：测试记录包含至少 20 次循环/事件延迟统计。


## 4.4 Orange Pi 5 AI 服务 `opi5_ai_service`

建议先实现 FastAPI 或 Flask 风格服务；为减少依赖，MVP 可直接使用 Flask。服务接口：

- `GET /health`
- `POST /api/infer/vision`

处理流程：接收 multipart JPEG 和 metadata，进行预处理，调用 mock/RKNN 推理，返回 `objects/faces/risk_hint/summary/action_hint/control_allowed=false`。

## 4.5 Flask Dashboard

真实仓库已存在 Flask 后端，并具备：

- `GET /`
- `GET /health`
- `POST /api/events`
- `GET /api/events`
- `GET /api/status/latest`
- `POST /api/images`
- `GET /uploads/<filename>`

不重写后端框架，只做兼容扩展：新增或兼容 `vision_json`、`ai_result_json`、`image_url`、`latency_ms`。若不改 schema，也可优先把新增字段保存进现有 `raw_json` 并在 API 返回中透出。

## 4.6 `common/contracts`

`common/contracts/README.md` 只作为开发入口，权威字段表在 `docs/07_端边HTTP_JSON_Contract.md`。

## 4.7 WSL 交叉编译与部署

命令骨架：

```bash
source <WSL_SDK_ENV_TODO_FILL>
$CC --version
mkdir -p build/imx6ull
$CC edge/imx6ull-controller/src/hello.c -o build/imx6ull/hello_imx6ull
scp build/imx6ull/hello_imx6ull <IMX_USER_TODO_FILL>@<IMX_IP_TODO_FILL>:/opt/edge-ai-safety-monitor/
ssh <IMX_USER_TODO_FILL>@<IMX_IP_TODO_FILL> /opt/edge-ai-safety-monitor/hello_imx6ull
```

Task02 已验证 WSL 交叉编译与 i.MX6ULL 板端运行：本 SDK 无 `environment-setup-*`，通过
`config/inventory.yaml` 的 `imx6ull.cc` 指向 `arm-buildroot-linux-gnueabihf-gcc`；`hello_imx6ull`
已在板端输出 `hello from imx6ull target`。证据见 `tests/imx6ull/2026-06-06_toolchain_ssh.md`。


## 4.8 systemd 服务

最终 i.MX6ULL 控制进程以 systemd 管理：

```ini
[Unit]
Description=Edge AI Safety Monitor i.MX6ULL Controller
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/opt/edge-ai-safety-monitor/imx_safetyd --config /etc/edge-ai-safety-monitor/imx.yaml
Restart=always
RestartSec=2
WatchdogSec=10

[Install]
WantedBy=multi-user.target
```

## 4.9 日志与配置

| 类型 | 路径建议 | 入库 |
|---|---|---|
| 示例配置 | `edge/imx6ull-controller/config/*.example.yaml` | 是 |
| 本地真实配置 | `/etc/edge-ai-safety-monitor/*.yaml` 或 `config/local*.yaml` | 否 |
| systemd unit | `edge/imx6ull-controller/systemd/*.service` | 是 |
| 日志 | `logs/` 或 journald | 否 |
| 缓存图片 | `spool/`、`captures/` | 否 |

## 4.10 不入库内容

模型权重、数据集、真实配置、SSH 私钥、数据库、图片、视频、日志不入库。见根目录 `.gitignore`。
