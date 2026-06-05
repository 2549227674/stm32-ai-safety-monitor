# Task 06：Flask Backend Contract 扩展

## 目标 / 范围

在现有 `server/backend/` 上增量扩展 vision/AI/image 字段和 Dashboard 展示，保持旧 `/api/events` 兼容。本任务不重写 Flask 框架。

## 前置条件

Task01 已完成。建议 Task05 mock 服务已可返回标准 JSON。

## 操作步骤

1. 阅读现有 `app.py/database.py/dashboard.js/dashboard.html`。
2. 决定最小 schema 方案：优先 raw_json 透出，必要时新增列。
3. 扩展 `normalize_event` 支持 `vision/ai_result/image_url/latency_ms`。
4. 扩展 API 返回。
5. 扩展 Dashboard 区域。
6. 用旧事件和新事件各测一次。

## 命令骨架

```bash
cd server/backend
python app.py
curl http://127.0.0.1:5000/health
curl -X POST http://127.0.0.1:5000/api/events -H 'Content-Type: application/json' -d @../../tests/payloads/event_with_ai.json
curl http://127.0.0.1:5000/api/status/latest
```

## 产出物

- 修改 `server/backend/app.py`、`database.py`、`static/dashboard.js`、`templates/dashboard.html`（按需要）
- `tests/payloads/event_with_ai.json`
- `tests/integration/YYYY-MM-DD_backend_contract_extension.md`

## 验收标准

- [ ] 旧 STM32_TEST 风格事件仍可 POST 并显示。
- [ ] 新 AI 事件可 POST 并显示 vision/summary/image_url。
- [ ] 数据库迁移不破坏旧 DB，或明确要求删除测试 DB 重建。

## 禁止事项

- 不推倒重写 Flask。
- 不提交 `safety_monitor.db`。
- 不破坏 `/api/events` 旧字段。

## 完成后回填

- 回填 docs/04 Flask 扩展状态。
- 回填 docs/07 实际字段。
- 更新 AGENTS.md 和 DEVPLAN P5。
