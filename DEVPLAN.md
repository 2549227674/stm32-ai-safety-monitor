# DEVPLAN.md — 1–2 周端边协同迁移执行看板

## 1. 总目标

在两周内完成 `基于 i.MX6ULL 与 Orange Pi 5 的端边协同本地 AI 多模态安全巡检系统` 的最小可演示闭环：i.MX6ULL 本地传感器/状态机 + V4L2 抓拍 + OPi5 AI 服务 + Flask Dashboard 展示。

## 2. 阶段看板

| 阶段 | 时间建议 | 任务 | 交付物 | 通过门槛 |
|---|---|---|---|---|
| P0 | Day 1 | Task01 仓库迁移与 legacy 归档 | 新目录、legacy README、迁移决策记录 | 已完成：旧代码归档，新主线目录存在 |
| P1 | Day 1–2 | Task02 WSL/i.MX SDK/SSH | hello、build/deploy 脚本 | 已完成：板端运行 hello |
| P2 | Day 3–4 | Task03 GPIO/I2C/PWM/MOS | 单模块测试程序、测试记录 | GPIO/I2C/PWM/MOS 至少各有一项证据 |
| P3 | Day 5 | Task04 V4L2 抓拍 | `v4l2_capture`、图片、格式记录 | i.MX 生成可打开图片 |
| P4 | Day 6–7 | Task05 OPi5 AI 服务 | mock/RKNN 服务 | curl 上传图像返回 AI JSON |
| P5 | Day 8 | Task06 Flask 扩展 | DB 兼容、Dashboard AI 区域 | 新旧事件均可显示 |
| P6 | Day 9–10 | Task07 端边垂直切片 | 完整事件、图片、AI 结果 | Dashboard 可演示 |
| P7 | Day 10–11 | Task08 云台巡检 | pan/tilt 扫描日志、视频 | 云台角度与识别结果联动 |
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

Task03-C 空载 PWM 软件准备已完成：`pca9685_pwm_test` 已构建、部署并在板端运行一次，按 1.0/1.5/2.0ms 三组配置输出 channel 0 空载 PWM；证据见 `tests/imx6ull/2026-06-06_pca9685_pwm_empty_load.md`。当前等待用户补充逻辑分析仪频率、脉宽和截图；在此之前 PWM 不写已通过。MG90/MOS 仍未完成。

后续阻塞项仍包括 OPi5 盘点与最终 Dashboard 部署决策。


> **[CLAUDE_CODE_TODO | INVESTIGATE]** 盘点 OPi5 与 PC 上已有 RKNN 仓库
> - 为何 GPT 给不了：沙箱看不到本机 `rknn-toolkit2-master`、`rknn-llm-main`、OPi5 上的 demo 文件。
> - 期望产物/操作：Claude Code 在 PC/OPi5 上列出可运行 demo、模型文件、依赖版本；选择第一个可接入的 demo。
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
