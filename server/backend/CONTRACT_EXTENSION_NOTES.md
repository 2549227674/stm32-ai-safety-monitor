# Flask 端边契约扩展落点说明（Task06 精确版）

> 本说明基于对真实 `server/backend/app.py`（241 行）与 `database.py`（143 行）的核对，
> 给出最小改动落点，避免 Claude Code 在不必要的 DB 迁移上浪费时间。

## 1. 关键发现：存储已天然兼容，无需改库

真实 `app.py` 的 `normalize_event()`（约第 146 行）已经把**整包 payload 存入 `raw_json`**：

```python
"raw_json": payload,
```

`database.py` 的 `insert_event()` 把 `raw_json` 完整 `json.dumps` 进 `events.raw_json` 列。
因此新增字段 `vision / ai_result / image_url / latency_ms / contract_version` 在 POST 时
**已经被持久化**，MVP 阶段**不需要为它们新增数据库列、也不需要迁移旧库**。

## 2. 唯一缺口：读取时把新字段“透出来”

`database.py` 的 `event_row_to_dict()`（约第 117 行）只返回归一化子集，**丢弃了 `raw_json`**：

```python
return {
    "event_id": ..., "timestamp": ..., "device_id": ..., "seq": ...,
    "type": ..., "state": ..., "risk_score": ..., "need_snap": ...,
    "sensors": json.loads(row["sensors_json"]),
    "actuators": json.loads(row["actuators_json"]),
    "source": row["source"],
}   # ← 没有 raw_json，于是 vision/ai_result 永远到不了前端
```

`/api/events` 与 `/api/status/latest` 都用它，所以 Dashboard 看不到 AI/视觉字段。

## 3. 最小改动（推荐 MVP 做法）

在 `event_row_to_dict()` 末尾补一行，把原始包透出（前端按需取 vision/ai_result）：

```python
    out = {
        # ...原有字段保持不变...
        "sensors": json.loads(row["sensors_json"]),
        "actuators": json.loads(row["actuators_json"]),
        "source": row["source"],
    }
    raw = json.loads(row["raw_json"]) if row["raw_json"] else {}
    # 只透出展示需要的扩展字段，保持响应精简、且旧事件天然为 None
    out["vision"]     = raw.get("vision")
    out["ai_result"]  = raw.get("ai_result")
    out["image_url"]  = raw.get("image_url") or (raw.get("vision") or {}).get("image_url")
    out["latency_ms"] = raw.get("latency_ms")
    return out
```

- 旧事件没有这些字段 → 取到 `None`，前端渲染时加判空即可，**完全向后兼容**。
- 不改表结构 → 旧 `safety_monitor.db` 不受影响。

## 4. 前端展示（dashboard.js / dashboard.html）

新增一个 AI/视觉区块，按 docs/08 §8.6 的字段渲染：objects / faces / risk_hint / summary /
latency_ms / 最新图片（`image_url` 经 `/uploads/<filename>` 显示）。所有字段判空，缺失则隐藏区块。

## 5. 自测（无需硬件）

```bash
# 1) 本机起 mock AI（可选，用于产出真实 ai_result 形状）
scripts/run_mock_ai.sh &

# 2) 起 Flask，灌入新旧两种事件，确认都能显示
cd server/backend && python app.py &
# 旧风格事件（无 AI 字段）—— 仍应正常显示
curl -X POST http://127.0.0.1:5000/api/events -H 'Content-Type: application/json' \
     -d '{"type":"event","device_id":"labbox_001","seq":1,"state":"NORMAL","risk_score":0,"need_snap":false,"sensors":{},"actuators":{}}'
# 新风格事件（带 vision/ai_result）
curl -X POST http://127.0.0.1:5000/api/events -H 'Content-Type: application/json' \
     -d @tests/payloads/event_with_ai.json
curl http://127.0.0.1:5000/api/status/latest   # 应能看到 vision/ai_result
```

## 6. 验收

- [ ] 旧 STM32_TEST 风格事件仍可 POST 且 Dashboard 正常显示（无报错）。
- [ ] 新事件的 `vision.summary`、`ai_result.risk_hint`、`image_url` 在 `/api/status/latest` 可见。
- [ ] 未改 `events` 表结构（或若改，旧库仍可读 / 已写明需重建测试库）。
- [ ] 旧库不被破坏。
