# common/contracts

本目录用于放置端边 Contract 的开发辅助文件。权威说明在 `docs/07_端边HTTP_JSON_Contract.md`。

## 当前 Contract

- 版本：`1.0`
- 主事件字段：`type/device_id/seq/state/risk_score/need_snap/sensors/actuators`
- 新增视觉字段：`vision/ai_result/image_url/latency_ms/frame_id/pan_tilt`
- AI 安全字段：`control_allowed=false`

## 后续可加入

- JSON Schema。
- curl payload 样例。
- Python/C 结构体定义。
- Contract 兼容性测试脚本。

> **[CLAUDE_CODE_TODO | VERIFY]** 从 docs/07 生成或同步 JSON Schema
> - 为何 GPT 给不了：当前只提供文档级 Contract，还未生成机器可校验 schema。
> - 期望产物/操作：Task06/07 后根据实际 API 生成 `event_v1.schema.json` 和 `infer_v1.schema.json`。
> - 回填位置：common/contracts/README.md
> - 验收：schema 能验证测试 payload。
