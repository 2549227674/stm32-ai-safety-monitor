# Claude Code 执行指南（OPi5 主线版）

> 本文件是 Claude Code 消费文档包的总入口。执行前先读 `CANONICAL_DECISIONS.md`。

## 1. 文档包地图

| 文件 | 给谁看 | 用途 |
|---|---|---|
| `CANONICAL_DECISIONS.md` | 人 + Claude Code | 全局唯一事实源 |
| `README.md` | 人 + Claude Code | 仓库入口 |
| `CLAUDE.md` / `AGENTS.md` | Claude Code | 开发规则、安全红线 |
| `DEVPLAN.md` | 人 + Claude Code | 任务看板 |
| `docs/00_README.md` | 人 | 设计文档地图 |
| `docs/01`–`docs/14` | 人 + 报告 | 设计文档正文 |
| `docs/07_端边HTTP_JSON_Contract.md` | 人 + Claude Code | API 契约权威 |
| `CLAUDE_CODE_TASK_*.md` | Claude Code | 可执行任务 |

## 2. 标记约定

```text
> **[CLAUDE_CODE_TODO | 类型]** 一句话说明
> - 为何 GPT 给不了：原因
> - 期望产物/操作：动作和产出
> - 回填位置：文件与小节
> - 验收：完成标准
```

类型：`VERIFY`、`FILL`、`INVESTIGATE`、`MEASURE`、`DECIDE`。

检索：`grep -RIn "\[CLAUDE_CODE_TODO" .`

## 3. 执行顺序

| 顺序 | 任务 | 目标 | 状态 |
|---|---|---|---|
| 1 | 文档对齐与归档 | 统一 OPi5 主线口径，归档 i.MX/STM32/ESP32 | 当前任务 |
| 2 | OPi5 device-agent / backend / frontend 字段对齐 | 确保 API 字段一致 | 待执行 |
| 3 | 前端真实数据联调修复 | mock/real mode 切换正常 | 待执行 |
| 4 | mock/seed 数据链路验证 | 无设备时可演示 | 待执行 |
| 5 | 真实设备回归 | 摄像头可用时验证 | 待执行 |
| 6 | 报告/PPT/硬件图纸收口 | 答辩材料 | 待执行 |

历史 i.MX6ULL 阶段任务（Task02/03/04/07/08/10/11）已归档到 `docs/archive/2026-imx6ull-stage/tasks/`。

## 4. 当前架构

| 节点 | 职责 | 部署入口 |
|---|---|---|
| OPi5 | 安全闭环 + AI 推理 + 设备代理 | `/opt/edge-ai-safety-monitor/` |
| PC / WSL | Flask 后端 + React 前端 + 开发 | `server/backend/`, `server/frontend/` |

## 5. 安全红线

1. 安全闭环必须留在 OPi5 本地 safetyd/controller 进程
2. AI / Flask / 前端不直接控制执行器；`control_allowed=false`
3. 不接 220V；低压直流演示
4. 先空载再接负载；先 mock 再接真实
5. 不提交模型、数据库、密钥、真实 IP

## 6. 最小验证

```bash
git status --short
python -m py_compile server/backend/*.py edge/opi5-device-agent/*.py
cd server/frontend && npm run build  # 如果有 Node 环境
```

## 7. 回报模板

```text
本轮完成：
- ...

主要改动文件：
- ...

一致性检查：
- 旧主控污染 grep：仅 archive/legacy/tests 命中
- OPi5 新能力 grep：README/CANONICAL/docs/07 已命中

最小验证：
- py_compile: PASS/FAIL
- npm build: PASS/SKIPPED/FAIL

遗留 TODO：
- ...
```
