# CLAUDE_CODE_TODO_INDEX.md — OPi5 当前待办

> 旧 i.MX 阶段 TODO 索引已归档至 `docs/archive/2026-imx6ull-stage/CLAUDE_CODE_TODO_INDEX_legacy.md`。

## OPEN

| 待办 | 说明 | 验收 |
|---|---|---|
| real camera 回归 | 真实 USB 摄像头接入时的端到端测试 | MJPEG 流可查看，快照正常 |
| report/PPT 收口 | 实训报告和答辩 PPT 最终提交 | 报告内容与 tests 证据一致 |
| docs 最终报告/PPT 同步 | 最终演示脚本与证据包文档更新 | docs/13、docs/14 与实际一致 |
| OPi5 真实设备回归测试 | 有真实 OPi5 硬件时端到端验证 | heartbeat/telemetry/AI/视频全链路 |
| real USB camera 端到端验证 | 真实摄像头 → device-agent → Flask → 前端 | 视频流可查看，camera_status=online |
| SMTP 真实邮箱端到端验证 | 配置真实 SMTP 服务器后发送测试邮件 | 收件箱收到告警邮件 |

## COMPLETED（近期）

| 待办 | 结果 | 证据 |
|---|---|---|
| front/back/device-agent 字段对齐 | heartbeat/telemetry/通知字段三方一致 | commit `126e2ce` |
| heartbeat 新增 agent_url/camera/video_mode | heartbeat 包含 agent_url、camera 对象、camera_status、video_mode、video_available | commit `126e2ce` |
| telemetry metric alias | cpu_temp_c→device.cpu_temp_c 等 8 个 alias 生效 | commit `126e2ce` |
| notification dedupe | dedupe_key 字段 + cooldown 判断 | commit `126e2ce` |
| notifier scope / agent_url unknown / minimal JPEG | 热修 4 项 | commit `7289b1d` |
| docs Phase 2 对齐 | 18 个 docs 文件重写为 OPi5 主线 | commit `b1499c4` |
| Qwen3-VL 真实 AI 部署 | 端到端推理通过 | `tests/opi5/2026-06-07_qwen3vl_real_ai.md` |
| email 通知 | cooldown + logging | commit `41143e6` |
| device-agent 心跳/遥测/AI observation | 30s 周期上报正常 | commit `b45785e` |
| Flask backend device API | heartbeat/telemetry/AI observation 接收 | commit `b45785e` |
