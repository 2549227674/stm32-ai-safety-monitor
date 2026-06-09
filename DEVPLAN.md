# DEVPLAN.md — 1–2 周端边协同迁移执行看板

## 1. 总目标

在答辩前完成 OPi5 临时主控一板双进程的最小可演示闭环：`opi5_safetyd` 读取真实 GPIO/PWM/MOS 传感器与执行器，完成本地状态机和离线安全闭环；`opi5_ai_service` 提供本地 AI 推理/解释；Flask Dashboard 完成事件展示。

> 原计划采用 i.MX6ULL + OPi5 双板架构，因 i.MX6ULL 末期供电/启动异常，临时切换为 OPi5 一板双进程。i.MX6ULL 阶段的已完成验证保留为工程迭代证据。

## 2. 阶段看板

| 阶段 | 时间建议 | 任务 | 交付物 | 通过门槛 |
|---|---|---|---|---|
| P0 | Day 1 | Task01 仓库迁移与 legacy 归档 | 新目录、legacy README、迁移决策记录 | 已完成：旧代码归档，新主线目录存在 |
| P1 | Day 1–2 | Task02 WSL/i.MX SDK/SSH | hello、build/deploy 脚本 | 历史 i.MX 阶段已完成：板端运行 hello |
| P2 | Day 3–4 | Task03 GPIO/I2C/PWM/云台 | 单模块测试程序、测试记录 | 历史 i.MX 阶段已完成：GPIO 输入、I2C/PCA9685、MG90S 云台具备真实证据 |
| P3 | Day 5 | Task04 V4L2 抓拍 | `v4l2_capture`、图片、格式记录 | 历史 i.MX 阶段已通过：生成可打开 JPEG 图片（640x480, MJPG, v4l2-ctl） |
| P4 | Day 6–7 | Task05 OPi5 AI 服务 | mock/RKNN 服务 | Task05-A 已通过：OPi5 mock `/health` 与 `/api/infer/vision` 返回 AI JSON；Task05-B 已通过：Qwen3-VL 2B 真实 VLM 推理接入 |
| P5 | Day 8 | Task06 Flask 扩展 | DB 兼容、Dashboard AI 区域 | 已通过：旧事件兼容，新 AI 事件字段可读取和展示 |
| P6 | Day 9–10 | Task07 端边垂直切片 | 完整事件、图片、AI 结果 | 历史 i.MX 阶段已通过：C 版 imx_safetyd 支持 once/loop/flush、实时 GPIO、降级/spool、init.d 启动管理 |
| P7 | Day 10–11 | Task08 云台巡检 | pan/tilt 扫描日志、视频 | 历史 i.MX 阶段已通过：三点扫描 pan 60/90/120° + mock AI + Flask 上报 |
| P8 | Day 12–14 | 报告/PPT/录屏 | 报告、PPT、上报表、演示视频 | 可答辩、可回放 |
| P0-real | Day 12–14 | Task10 P0 传感器/执行器真实化 | 真实 GPIO 输入/输出、本地 ALARM 证据 | 历史 i.MX 阶段已通过：10-B 火焰、10-C MQ-2、10-D PIR、10-E 蜂鸣器/RGB |
| P1-ext | Day 15+ | Task11 P1 扩展 | OLED + relay + soc_temp + pump | 历史 i.MX 阶段已通过：11-A OLED、11-B relay、11-C soc_temp、11-D pump |
| **P-migrate** | Day 15+ | **Task13 OPi5 controller migration** | `opi5_safetyd` GPIO/PWM/MOS 验证 | **已完成：GPIO 输入（flame/PIR/MQ-2）、GPIO 输出（buzzer/RGB/MOS）、PWM 舵机、I2C OLED/MPU6050；PCA9685 确认为模块问题，改用 GPIO/PWM 直控** |
| **P-final** | Day 16+ | **Task09 最终演示脚本与证据包** | 演示脚本、Dashboard 截图、报告/PPT、录屏 | **当前阻塞项：最终演示链路、Dashboard 截图、报告/PPT、录屏、答辩问答** |

## 3. MVP 锁定范围

必须做：

- Task01–07 历史 i.MX 阶段已完成验证。
- Task13 OPi5 controller migration 已完成 GPIO/PWM/MOS 验证。
- OPi5 `opi5_ai_service` Qwen3-VL 真实 VLM 推理已通过。
- Flask 保留旧 `/api/events`，扩展显示 AI/vision/image。
- OPi5 `opi5_safetyd` 本地状态机具备 `NORMAL/WARN/VERIFY/ALARM/FAULT`。

可砍掉：

- 人脸识别精确身份库。
- 本地 LLM 解释。
- 高帧率视频流。
- PREEMPT_RT 内核重编译。
- MPU6050 设备健康监测。

## 4. 当前阻塞项

**当前阻塞项是最终演示链路、Dashboard 截图、报告/PPT、录屏、答辩问答。** 详见 Task09。

### 历史 i.MX 阶段已完成任务

Task01、Task02 已完成。Task02 已验证 i.MX6ULL SDK、inventory 读取、hello 交叉编译、WSL/PC SSH、scp 与板端运行。

Task03（历史 i.MX 阶段）：GPIO 输入通过、I2C/PCA9685 地址通过、MG90S 分时云台通过、PWM 波形实测跳过、MOS 跳过。证据见 `tests/imx6ull/2026-06-06_*.md`。

Task04（历史 i.MX 阶段）：V4L2 USB 摄像头抓拍已通过，生成可打开 JPEG 图片（640x480, MJPG, v4l2-ctl）。证据见 `tests/imx6ull/2026-06-06_v4l2_capture.md`。

Task07-C（历史 i.MX 阶段）：C 版 imx_safetyd 已通过，支持 once/loop/flush、真实 gpio117 空闲读取、NORMAL/VERIFY/FAULT 最小状态机、OPi5 offline fallback、Flask spool/flush。证据见 `tests/integration/2026-06-07_imx_safetyd_c.md`。

Task07-C2（历史 i.MX 阶段）：BusyBox/SysV `init.d` 启动管理已通过。证据见 `tests/integration/2026-06-07_imx_safetyd_initd.md`。

Task10 P0 传感器/执行器真实化（历史 i.MX 阶段）：10-B 火焰通过、10-C MQ-2 通过、10-D PIR 沿用历史、10-E 蜂鸣器+RGB 通过、10-E2 RGB PCA9685 PWM 通过。证据见 `tests/imx6ull/2026-06-07_p0_*.md`。

Task11 P1 扩展（历史 i.MX 阶段）：11-A OLED 通过、11-B relay 通过、11-C soc_temp 通过、11-D pump 通过。证据见 `tests/imx6ull/2026-06-07_p1_*.md`。

### 当前主线已完成任务

Task05-A OPi5 AI mock 服务已通过；Task05-B Qwen3-VL 2B 真实 VLM 推理已通过；Task05-C 持久化 worker 已通过；Task12-I systemd 自启动已通过。证据见 `tests/opi5/2026-06-07_*.md`、`tests/opi5/2026-06-08_*.md`。

Task06 Flask 契约扩展已通过。证据见 `tests/integration/2026-06-07_backend_contract_extension.md`。

Task12 网络优化全部子任务已通过（A–I）。证据见 `tests/opi5/2026-06-08_*.md`、`tests/integration/2026-06-08_*.md`。

Task13 OPi5 controller migration 已完成 GPIO/I2C/PWM/MOS 验证。证据见提交 `533f226`。

## Task07-D：C 版 imx_safetyd 原生化与模块化增强（历史 i.MX 阶段，可选，不阻塞 MVP）

> **[CLAUDE_CODE_TODO | ENHANCE]** Task07-D 后续升级路线记录
> - 为何 GPT 给不了：沙箱无法验证 i.MX6ULL sysroot 是否具备 libcurl 头文件/库，也无法实测原生 V4L2 mmap 流程。
> - 期望产物/操作：后续有时间时按 D1→D2→D3 顺序逐步升级。
> - 回填位置：DEVPLAN、docs/04、docs/06、docs/11、CLAUDE_CODE_TODO_INDEX
> - 验收：每项升级完成后，已有 Task07-C 行为不退化。

Task07-D 为工程化增强项，不作为当前 MVP 验收阻塞项。当前 MVP 仍以 Task07-C C 版 imx_safetyd + v4l2-ctl/curl 子进程方案为稳定基线。

- **Task07-D1：原生 libcurl HTTP client**
  - 目标：用 libcurl API 替换当前 C 程序中调用 curl 命令的方式。
  - 范围：封装 HTTP GET/POST、multipart 图片上传、JSON 上报、HTTP code、响应体、timeout 和错误码。
  - 收益：减少外部命令依赖，提高 daemon 长期运行稳定性和错误诊断能力。
  - 风险：需要确认 i.MX6ULL Buildroot sysroot 和板端运行环境具备 libcurl 头文件、库文件及其依赖。

- **Task07-D2：原生 V4L2 抓拍**
  - 目标：用 open/ioctl/mmap 或 read 方式直接访问 /dev/video1，替换当前 v4l2-ctl 命令。
  - 范围：查询能力、设置 MJPG 640x480、申请 buffer、取一帧、保存 JPEG。
  - 收益：减少 v4l2-ctl 依赖，获得更细粒度的摄像头错误信息和采集耗时。
  - 风险：V4L2 mmap 流程复杂，需处理格式兼容、buffer 生命周期和异常清理。

- **Task07-D3：更完整 C 模块化拆分**
  - 目标：将当前单文件 imx_safetyd.c 拆分为 gpio、camera、http、event、spool、fsm、main 等模块。
  - 收益：结构更清晰，便于报告展示和后续维护。
  - 风险：拆分可能引入集成问题，必须保持 Task07-C 已通过行为不退化。

Task11 P1 扩展（历史 i.MX 阶段）：11-A OLED 已通过，11-B relay 已通过，11-C soc_temp 已通过，11-D pump 已通过。P1 全部走 I2C 总线 + PCA9685 空闲通道 + SoC 内部温度。证据见 `tests/imx6ull/2026-06-07_p1_*.md`。

Task12 网络优化：已完成全部子任务。
- Task12-A: OPi5 USB WiFi rtl8188eu 驱动编译成功（0bda:0179 / RTL8188ETV）。
- Task12-B: Windows portproxy 转发验证通过。
- Task12-C: 拔掉 OPi5 网线后 portproxy 回归通过。
- Task12-D: i.MX 板载 RTL8723BU WiFi 诊断（枚举成功，驱动已加载）。
- Task12-E: i.MX 板载 WiFi 连接成功（nl80211，-45dBm，72.2MBit/s）。
- Task12-F: 全无线热点链路验证通过（三台设备同一热点，直连无需 portproxy）。
- Task12-G: 全无线自启动与代理清理，重启回归通过。
- Task12-H: 文档同步收口。
- Task12-I: OPi5 Qwen3-VL 真实 AI 自启动（systemd enabled，默认 qwen3vl 非 mock，重启回归通过）。
- 当前网络主线：全无线优先。回退：Windows portproxy / 全有线。
- 下一步：Task09 最终演示脚本与证据包整理。
- 证据：`tests/opi5/2026-06-08_opi5_usb_wifi_rtl8188eu.md`、`tests/imx6ull/2026-06-08_imx6ull_onboard_rtl8723bu_wifi_retry.md`、`tests/integration/2026-06-08_*wireless*.md`。

Task10 P0 传感器/执行器真实化（历史 i.MX 阶段已完成）：10-B 火焰通过、10-C MQ-2 通过、10-D PIR 沿用历史、10-E 蜂鸣器+RGB 通过、10-E2 RGB PCA9685 PWM 通过。引脚以 `CANONICAL_DECISIONS.md` 0.6 为准。证据见 `tests/imx6ull/2026-06-07_p0_*.md`。

Task06 Flask 契约扩展已通过：后端保持旧 `/api/events` 兼容，不修改 DB schema，通过 `raw_json` 透出 `contract_version/vision/ai_result/image_url/latency_ms`；Dashboard 新增 AI/视觉展示区，可展示 AI 摘要、`risk_hint`、objects、faces、image URL/图片预览、latency、pan/tilt 和 `control_allowed=false`。旧事件缺少 AI 字段时 API 返回 `null`，前端判空显示”暂无 AI/视觉结果”。

Task05-B Qwen3-VL 2B 真实 AI 部署已通过：OPi5 上 Qwen3-VL 2B 通过 rknn-llm C++ demo 部署，vision encoder (RKNN) + LLM (RKLLM W8A8) 真实推理通过；`/health` 和 `/api/infer/vision` 返回真实 VLM 结果（中文场景描述、risk_hint、vlm_result/explanation）；AI_BACKEND=mock 回归通过；`control_allowed=false`；当前通过子进程调用，端到端 ~13s。
> - 为何 GPT 给不了：沙箱看不到 PC 侧模型转换环境，OPi5 上候选 demo 虽已只读初筛但尚未运行验证。
> - 期望产物/操作：Claude Code 在 PC/OPi5 上列出可运行 demo、模型文件、依赖版本；选择第一个可接入的 demo，并真实运行至少一个最小 RKNN demo。
> - 回填位置：DEVPLAN 第 4 节；docs/09；Task05
> - 验收：形成 `edge/opi5-ai/models/README.md` 和 `tests/opi5/YYYY-MM-DD_rknn_inventory.md`。


> **[CLAUDE_CODE_TODO | DECIDE]** 确认最终 Dashboard 部署位置
> - 为何 GPT 给不了：最终演示是否脱离 PC 会影响 P8 报告与现场布线。
> - 期望产物/操作：用户拍板：最终使用 PC Flask，还是迁移 Flask 到 OPi5。
> - 回填位置：DEVPLAN P8；docs/13；README
> - 验收：最终演示脚本与部署命令只保留一种主方案。


## 5. 每日收尾规则

- 更新 `AGENTS.md`。
- 新增或更新 `tests/` 记录。
- 关闭已完成 TODO。
- 保持提交粒度小，避免一次提交混入多个 Task。
