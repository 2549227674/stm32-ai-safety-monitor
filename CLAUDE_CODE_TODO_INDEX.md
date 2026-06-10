# CLAUDE_CODE_TODO_INDEX.md — OPi5 当前待办

> 旧 i.MX 阶段 TODO 索引已归档至 `docs/archive/2026-imx6ull-stage/CLAUDE_CODE_TODO_INDEX_legacy.md`。

## OPEN

| 待办 | 说明 | 验收 |
|---|---|---|
| front/back/device-agent 字段对齐 | 确保 React 前端、Flask 后端、device-agent 三方字段名和类型一致 | 前端能正确展示 device-agent 上报的数据 |
| heartbeat 新增 agent_url/camera/video_mode | device-agent 心跳上报 agent_url、摄像头状态、视频模式 | Flask 设备详情可见这些字段 |
| telemetry metric alias | 遥测 metric 名称对齐（如 cpu_temp_c vs soc_temp） | Dashboard 时序图正确显示 |
| notification dedupe | 通知去重，避免重复发送相同告警邮件 | 相同告警在冷却期内只发一次 |
| real camera 回归 | 真实 USB 摄像头接入时的端到端测试 | MJPEG 流可查看，快照正常 |
| report/PPT 收口 | 实训报告和答辩 PPT 最终提交 | 报告内容与 tests 证据一致 |

## COMPLETED（近期）

| 待办 | 结果 | 证据 |
|---|---|---|
| docs Phase 2 对齐 | 18 个 docs 文件重写为 OPi5 主线 | commit `b1499c4` |
| Qwen3-VL 真实 AI 部署 | 端到端推理通过 | `tests/opi5/2026-06-07_qwen3vl_real_ai.md` |
| email 通知 | cooldown + logging | commit `41143e6` |
| device-agent 心跳/遥测/AI observation | 30s 周期上报正常 | commit `b45785e` |
| Flask backend device API | heartbeat/telemetry/AI observation 接收 | commit `b45785e` |
