# DEVPLAN.md — 1–2 周端边协同迁移执行看板

## 1. 总目标

在两周内完成 `基于 i.MX6ULL 与 Orange Pi 5 的端边协同本地 AI 多模态安全巡检系统` 的最小可演示闭环：i.MX6ULL 本地传感器/状态机 + V4L2 抓拍 + OPi5 AI 服务 + Flask Dashboard 展示。

## 2. 阶段看板

| 阶段 | 时间建议 | 任务 | 交付物 | 通过门槛 |
|---|---|---|---|---|
| P0 | Day 1 | Task01 仓库迁移与 legacy 归档 | 新目录、legacy README、迁移决策记录 | 已完成：旧代码归档，新主线目录存在 |
| P1 | Day 1–2 | Task02 WSL/i.MX SDK/SSH | hello、build/deploy 脚本 | 已完成：板端运行 hello |
| P2 | Day 3–4 | Task03 GPIO/I2C/PWM/云台 | 单模块测试程序、测试记录 | GPIO 输入、I2C/PCA9685、MG90S 云台至少具备真实证据；MOS 作为扩展项保留 |
| P3 | Day 5 | Task04 V4L2 抓拍 | `v4l2_capture`、图片、格式记录 | 已通过：i.MX 生成可打开 JPEG 图片（640x480, MJPG, v4l2-ctl） |
| P4 | Day 6–7 | Task05 OPi5 AI 服务 | mock/RKNN 服务 | Task05-A 已通过：OPi5 mock `/health` 与 `/api/infer/vision` 返回 AI JSON；Task05-B 继续 RKNN demo 盘点与接入 |
| P5 | Day 8 | Task06 Flask 扩展 | DB 兼容、Dashboard AI 区域 | 已通过：旧事件兼容，新 AI 事件字段可读取和展示 |
| P6 | Day 9–10 | Task07 端边垂直切片 | 完整事件、图片、AI 结果 | Task07-A/B/C/C2 已通过：C 版 imx_safetyd 支持 once/loop/flush、实时 GPIO、降级/spool、init.d 启动管理 |
| P7 | Day 10–11 | Task08 云台巡检 | pan/tilt 扫描日志、视频 | Task08-A 已通过：三点扫描 pan 60/90/120° + mock AI + Flask 上报 |
| P8 | Day 12–14 | 报告/PPT/录屏 | 报告、PPT、上报表、演示视频 | 可答辩、可回放 |

## 3. MVP 锁定范围

必须做：

- Task01–07。
- OPi5 推理服务至少 mock；优先接一个现有可跑 RKNN demo。
- Flask 保留旧 `/api/events`，扩展显示 AI/vision/image。
- i.MX6ULL 本地状态机具备 `NORMAL/WARN/VERIFY/ALARM/FAULT`。

可砍掉：

- 人脸识别精确身份库。
- 本地 LLM 解释。
- 高帧率视频流。
- PREEMPT_RT 内核重编译。
- MPU6050 设备健康监测。

## 4. 当前阻塞项

Task01、Task02 已完成。Task02 已验证 i.MX6ULL SDK、inventory 读取、hello 交叉编译、WSL/PC SSH、scp 与板端运行。

Task03-A GPIO 输入验证已完成：板端 libgpiod 工具缺失，已采用 `/sys/class/gpio` fallback，`gpio_test` 已交叉编译并部署到 i.MX6ULL。100ask i.MX6ULL V1.1 原理图引脚参考已整理到 `docs/reference/hardware/100ask_imx6ull_pinmap.md`。PIR / HC-SR501 已接入 J5 D0 / `CSI_DATA0` / `gpio117`，能稳定读到 0/1 变化；证据见 `tests/imx6ull/2026-06-06_gpio_input_probe.md`。裸门磁/裸按键因无 10k 上拉暂未测，但 PIR DO 已完成 GPIO 输入验证。

Task03-B I2C/PCA9685 地址验证已完成：PCA9685 逻辑侧接 J5 I2C，`/dev/i2c-0` 上 `0x40` ACK；证据见 `tests/imx6ull/2026-06-06_i2c_pca9685_probe.md`。本轮未接 PCA9685 `V+`、未接舵机、未输出 PWM。

Task03-C 空载 PWM 软件准备已完成：`pca9685_pwm_test` 已构建、部署并在板端运行一次，按 50Hz / prescale 121 / channel0 1.0ms、1.5ms、2.0ms 三组配置输出空载 PWM；证据见 `tests/imx6ull/2026-06-06_pca9685_pwm_empty_load.md`。用户决定跳过逻辑分析仪波形实测，不再等待截图；Task03-C 仅标记为软件准备完成，不写 PWM 波形已通过。

Task03-D MG90S 舵机测试已通过：单 MG90S 链路正常；双 MG90S 同步动作有 5V 压降，改为分时模式后 CH0/CH1 各自 1100→1900us 大幅动作通过，无压降/抖动/卡死/掉线。后续 Task08 云台巡检优先采用分时策略。证据见 `tests/imx6ull/2026-06-06_pca9685_servo_small_angle.md`。

Task03-E MOS 低压负载本轮跳过：XY-MOS 模块控制端 GND/TRIG-PWM 为圆孔焊盘，需额外焊接排针或飞线；当前 MVP 不依赖低压负载演示，保留为后续扩展项。证据见 `tests/imx6ull/2026-06-06_mos_skip_decision.md`。

Task03 总结：GPIO 输入通过、I2C/PCA9685 地址通过、MG90S 分时云台通过、PWM 波形实测跳过、MOS 跳过。下一步进入 Task04 V4L2 USB 摄像头抓拍。

Task05-A OPi5 AI mock 服务已通过：WSL 可 SSH 到 OPi5，Python 3.10.12、pip 22.0.2、Flask 2.2.5 可用；`scripts/deploy_opi5.sh` 已能把 `edge/opi5-ai/service/app.py` 部署到 `/opt/edge-ai-safety-monitor/opi5-ai/`；`/health` 与 `/api/infer/vision` 可从 WSL curl 访问，返回符合 `docs/07` 的 mock AI JSON，且 `control_allowed=false`。本轮只读盘点到 OPi5 上存在 `rknn-toolkit2-master`、`rknn-llm`、`phase10/models/yolov8n_phase10`、`models/efficientad_models`、`models/fastsam_models`、`models/qwen3vl_models` 等候选目录/模型文件名，但未验证任何真实 RKNN demo 可用。

Task07-C C 版 imx_safetyd 已通过：新增 i.MX6ULL C 主控程序，支持 once/loop/flush，读取真实 gpio117，执行 NORMAL/VERIFY/FAULT 最小状态机；VERIFY 时调用 v4l2-ctl 抓拍并通过 curl 请求 OPi5 mock AI；OPi5 不可达时生成干净 fallback ai_result；Flask 不可达时写 pending spool，恢复后可 flush 到 sent。C 程序仍调用 v4l2-ctl/curl 子进程，非原生 V4L2/HTTP 库实现。当前板端无 systemctl，systemd 模板已入库但未在板端启用。本轮 PIR 仅测到空闲 raw=0，VERIFY 使用 FORCE_VERIFY=1。

Task07-C2 init.d 启动管理已通过：BusyBox/SysV `init.d` 脚本 `S99imx-safetyd` 已部署，支持 `start/stop/restart/status`、PID 文件、日志追加与重复启动防护；板端 `rcS` 会执行 `S??*`，具备开机自启条件。`/etc/edge-ai-safety-monitor/imx-safetyd.env` 仅在板端手动创建，不入库。

后续阻塞项仍包括 Task05-B 真实 RKNN demo 运行验证、PC 侧 RKNN 资产盘点与最终 Dashboard 部署决策。

Task06 Flask 契约扩展已通过：后端保持旧 `/api/events` 兼容，不修改 DB schema，通过 `raw_json` 透出 `contract_version/vision/ai_result/image_url/latency_ms`；Dashboard 新增 AI/视觉展示区，可展示 AI 摘要、`risk_hint`、objects、faces、image URL/图片预览、latency、pan/tilt 和 `control_allowed=false`。旧事件缺少 AI 字段时 API 返回 `null`，前端判空显示“暂无 AI/视觉结果”。


> **[CLAUDE_CODE_TODO | INVESTIGATE]** Task05-B 盘点并验证 OPi5 与 PC 上已有 RKNN 仓库
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
