# DEVPLAN.md — OPi5 主线任务看板

## 1. 总目标

OPi5 一板主控完成本地安全闭环 + AI 推理 + 设备管理 + Flask/React 可视化的完整演示。

## 2. 阶段看板

### 已完成

| 任务 | 状态 | 说明 |
|---|---|---|
| Task01 仓库迁移与 legacy 归档 | 已完成 | STM32/ESP32 归档到 `legacy/` |
| Task05 OPi5 AI 服务 | 已完成 | mock + Qwen3-VL 真实 + 持久化 worker + systemd 自启动 |
| Task06 Flask 契约扩展 | 已完成 | 旧事件兼容，新 AI 字段可展示 |
| Task12 网络优化 | 已完成 | 全无线 + 自启动 |
| Task13 OPi5 controller migration | 已完成 | GPIO/PWM/MOS 验证 |
| 文档对齐与归档 | 已完成 | i.MX/STM32/ESP32 归档，OPi5 主线口径统一 |
| 前后端/device-agent 字段对齐 | 已完成 | heartbeat/telemetry/通知字段三方一致（`126e2ce` + `7289b1d`） |
| 前端真实数据联调 | 已完成 | mock/real mode 正常，camera_status/video_mode 对齐 |
| mock/seed 数据链路验证 | 已完成 | 无设备时可演示 telemetry/AI/通知 |

### 下一步

| 任务 | 目标 |
|---|---|
| OPi5 真实设备回归 | 有真实硬件时端到端验证 |
| USB camera real stream/snapshot | 真实摄像头 → device-agent → Flask → 前端 |
| SMTP 真实邮件测试 | 配置真实 SMTP 服务器后发送测试邮件 |
| 报告/PPT/硬件图纸收口 | 答辩材料最终提交 |

## 3. MVP 锁定范围

必须做：
- OPi5 safetyd 本地安全闭环
- OPi5 AI Qwen3-VL 推理
- OPi5 device-agent 心跳/遥测/视频
- Flask 后端 API
- React 前端 Dashboard
- 邮件通知

可砍掉：
- 人脸识别精确身份库
- 高帧率视频流
- PREEMPT_RT 内核重编译

## 4. 历史归档

i.MX6ULL 阶段的已完成验证保留在 `tests/imx6ull/` 和 `tests/integration/`，作为工程迭代证据。
详见 `docs/archive/2026-imx6ull-stage/README.md`。

## 5. 每日收尾规则

- 更新 `CANONICAL_DECISIONS.md`
- 新增或更新 `tests/` 记录
- 保持提交粒度小
