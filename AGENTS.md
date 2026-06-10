# AGENTS.md — Agent / Codex 工作入口（OPi5 主线版）

> Codex 自动加载的仓库级指南。与 `CLAUDE.md` 同步；冲突以 `CANONICAL_DECISIONS.md` 为准。

## 0. 开始任何任务前必须先读（按顺序）

1. `CANONICAL_DECISIONS.md` — 全局唯一事实源
2. `CLAUDE_CODE_EXECUTION_GUIDE.md` — 执行总入口
3. 当前要执行的 `CLAUDE_CODE_TASK_*.md`
4. 与任务相关的 `docs/0x_*.md`
5. 契约权威来源：`docs/07_端边HTTP_JSON_Contract.md`

## 1. 当前阶段

| 项目 | 状态 |
|---|---|
| 工作目录 | `/home/qbz415/SafetyMonitor` |
| 当前分支 | `migration/imx6ull-opi5-edge-ai` |
| 当前主线 | OPi5 一板主控（opi5_safetyd + opi5_ai_service + opi5-device-agent）+ Flask + React |
| 历史归档 | i.MX6ULL → `docs/archive/2026-imx6ull-stage/`；STM32/ESP32 → `legacy/` |
| 当前重点 | 文档对齐、前后端/device-agent 字段对齐、最终演示脚本 |

## 2. 当前活跃代码目录

| 目录 | 说明 |
|---|---|
| `edge/opi5-controller/` | OPi5 本地安全闭环 |
| `edge/opi5-ai/` | OPi5 AI 推理服务 |
| `edge/opi5-device-agent/` | OPi5 设备代理 |
| `server/backend/` | Flask 后端 |
| `server/frontend/` | React edge-console |
| `config/templates/` | systemd/env 模板 |

## 3. 安全红线

1. 安全闭环必须留在 OPi5 本地 safetyd/controller 进程
2. AI / Flask / 前端只展示/解释/通知，不直接控制执行器
3. `control_allowed` 始终为 `false`
4. 不接 220V；演示负载用低压直流
5. 先空载再接负载；先 mock 再接真实

## 4. 禁止做的事

| 禁止 | 原因 |
|---|---|
| 把 i.MX6ULL 写成当前主控 | 已归档为历史 |
| 把未实测硬件写成"已通过" | 报告须与真实状态一致 |
| 让 AI/Flask/前端直接控制执行器 | 安全边界 |
| 提交 config/inventory.yaml、.env、模型权重、数据库 | 入库安全 |
| 删除旧代码历史 | legacy 是答辩证据 |

## 5. 命令与配置

- 真实环境值从不入库的 `config/inventory.yaml` 读取
- 仓库内只放 `config/inventory.example.yaml`
- 部署脚本：`scripts/deploy_opi5.sh`

## 6. 标记规范

```text
> **[CLAUDE_CODE_TODO | 类型]** 一句话说明
> - 为何 GPT 给不了：原因
> - 期望产物/操作：动作与产出
> - 回填位置：文件与小节
> - 验收：完成标准
```

检索待办：`grep -RIn "\[CLAUDE_CODE_TODO" .`

## 7. 完成任务后的回填

1. 更新 `CANONICAL_DECISIONS.md` 状态
2. 更新 `DEVPLAN.md` 看板
3. 在 `tests/<scope>/YYYY-MM-DD_*.md` 记录证据
4. 提交信息：`taskXX: 简短说明`

## 8. 历史任务记录

历史 i.MX6ULL 阶段任务（Task02/03/04/07/08/10/11）已归档到 `docs/archive/2026-imx6ull-stage/tasks/`。
历史测试证据保留在 `tests/imx6ull/` 和 `tests/integration/`。

当前主线已完成：
- Task01: 仓库迁移与 legacy 归档
- Task05: OPi5 AI 服务（mock + Qwen3-VL 真实 + 持久化 worker + systemd 自启动）
- Task06: Flask 契约扩展
- Task12: 网络优化（全无线 + 自启动）
- Task13: OPi5 controller migration

下一步：文档对齐 → 前后端/device-agent 字段对齐 → 最终演示脚本。
