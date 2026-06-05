# 文档包审核报告与补丁说明（2026-06-05）

对 `edge_ai_safety_monitor_doc_package.zip` 的审核结论与补丁。本补丁包按相同相对路径
组织，**覆盖/补充**在原文档包之上即可。

---

## 0. 总体结论

**通过，地基真实可靠，不需要重做。** 用原始项目 zip 逐项核对了文档包 CANONICAL 中声称的
“已确认仓库事实”，**全部属实，无幻觉**：

| GPT 的声称 | 核对结果 |
|---|---|
| Flask 路由 `/`、`/health`、`/api/events`(GET/POST)、`/api/status/latest`、`/api/images`、`/uploads/<filename>` | ✅ 真实（`@app.get/@app.post` 风格） |
| `events` 表含 `timestamp/device_id/seq/type/state/risk_score/need_snap/sensors_json/actuators_json/raw_json/source` | ✅ 真实 |
| `images` 表含 `timestamp/device_id/event_id/filename/url/note` | ✅ 真实 |
| STM32 `main.c` 用 `MX_USART2_UART_Init()` + `AppComm_Init(&huart2)` | ✅ 真实 |
| ESP32-CAM 默认 `Serial`/UART0、`RX_PIN=3`/`TX_PIN=1` | ✅ 真实 |
| “zip 内代码与最新会话脱节” | ✅ 属实，且对答辩诚实未美化 |

文档内部一致性（项目名 / 四层架构 / docs/07 契约字段 / 目录树）一致；安全红线（AI 不控执行器、
低压、星形共地、默认 OFF、未实测不写已通过）贯彻到位；37 条 TODO 分类清晰。

---

## 1. 发现的问题（按严重度）

| 级别 | 问题 | 处理 |
|---|---|---|
| 高（会坑第一步） | 把包 overlay 进分支后，旧 `docs/01–14`（`#U` 乱码名）与新 `_端边协同版` 文件**同号并存**；旧根目录 `CLAUDE_CODE_TASK_01/02/03_*_improved.md` 也会残留。原 Task01 未清理。 | 提供**修订版 Task01**（含 `git rm` 旧同号 docs、旧 Task 文档移入 legacy、overlay 步骤、核对无重复编号）。 |
| 中（缺关键产物） | `edge/opi5-ai/service/app.py`、`scripts/*.sh`、`legacy/.../README_legacy.md` 被到处引用却只有 `.gitkeep` / 未提供。 | 提供**可运行 mock AI 服务** + **build/deploy/inventory 脚本** + **legacy README**。 |
| 中（省返工） | Task06 未点明：真实后端 `raw_json` 已存全量 payload（存储天然兼容），缺口只在 `event_row_to_dict` 未透出。 | 提供 `server/backend/CONTRACT_EXTENSION_NOTES.md`，给出最小改动与自测。 |
| 低（串码） | `docs/13` 第 13.5 节表格混入希伯来文 `bלבד`（应为“仅”）。 | 见下方一行修法。 |

### docs/13 串码修正（一行）

把 `docs/13_实训报告与答辩演示方案_迁移版.md` 中：

```
| mock בלבד | “已完成端边 AI 服务接口和结构化返回，模型接入预留” |
```

改为：

```
| 仅 mock | “已完成端边 AI 服务接口和结构化返回，模型接入预留” |
```

命令（在分支根目录）：

```bash
sed -i 's/mock בלבד/仅 mock/' docs/13_实训报告与答辩演示方案_迁移版.md
```

---

## 2. 本补丁包含的文件

| 路径 | 类型 | 说明 |
|---|---|---|
| `CLAUDE_CODE_TASK_01_repo_migration_legacy_archive.md` | 覆盖 | 修订版，修复 overlay 后旧文件并存问题 |
| `edge/opi5-ai/service/app.py` | 新增 | 可运行 mock AI 服务，**已实测**符合 docs/07（/health、/api/infer/vision、`control_allowed=false`、缺图 400、hazard 抬升 risk_hint） |
| `scripts/lib_inventory.sh` | 新增 | 读取 `config/inventory.yaml`，**已实测**取值正确、占位符被拒 |
| `scripts/build_imx6ull.sh` | 新增 | source SDK 交叉编译（Task02） |
| `scripts/deploy_imx6ull.sh` | 新增 | scp 产物到 i.MX6ULL 并可选运行 |
| `scripts/deploy_opi5.sh` | 新增 | 同步 AI 服务到 OPi5（不传模型/图片） |
| `scripts/run_mock_ai.sh` | 新增 | 本地一键启动 mock AI |
| `legacy/2026-stm32-esp32/README_legacy.md` | 新增 | 诚实记录第一版状态与差异 |
| `server/backend/CONTRACT_EXTENSION_NOTES.md` | 新增 | Task06 精确落点（基于真实代码） |

所有脚本默认值安全、无真实 IP/路径；环境相关项均走 `config/inventory.yaml`（不入库）或 `[CLAUDE_CODE_TODO]`。

---

## 3. 如何应用

```bash
# 1) 先按修订版 Task01 建分支并 overlay 原文档包
# 2) 再把本补丁包按相同相对路径覆盖到分支根目录
rsync -av edge_ai_doc_package_supplement/ ./
# 3) 修 docs/13 串码
sed -i 's/mock בלבד/仅 mock/' docs/13_实训报告与答辩演示方案_迁移版.md
# 4) 给脚本执行权限
chmod +x scripts/*.sh
# 5) 重新生成 TODO 索引（本补丁新增/改动了少量 TODO 标记）
grep -RIn "\[CLAUDE_CODE_TODO" . | sort > /tmp/todo_scan.txt && echo "TODO 行数: $(wc -l < /tmp/todo_scan.txt)"
```

> 提醒：本补丁的 `server/backend/CONTRACT_EXTENSION_NOTES.md` 只是说明文件；
> **真实的 `app.py` / `database.py` 仍由 Claude Code 在分支里按该说明增量修改**，不要被任何包覆盖。

---

## 4. 几点非阻塞建议（可选，不改也能跑）

- 建议把 mock AI 落地后，作为 Task05 的“第一可交付”：在**任何硬件就绪前**就能用
  `scripts/run_mock_ai.sh` + `tests/payloads/event_with_ai.json` 跑通
  i.MX(mock 发包) → OPi5(mock) → Flask → Dashboard 的纯软件垂直切片，极大降低答辩风险。
- `common/contracts/` 后续可由 docs/07 生成 `event_v1.schema.json`，把契约变成可机器校验（包内已有该 TODO）。
- 三机网络拓扑（PC/WSL ↔ i.MX6ULL ↔ OPi5）建议尽早画进 docs/03 或 docs/13，答辩时直观。
