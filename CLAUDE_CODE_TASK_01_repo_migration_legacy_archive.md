# Task 01：仓库迁移与 Legacy 归档（修订版）

> 本文件为审核修订版，相比初版新增：①把文档包 overlay 落地到迁移分支的步骤；
> ②清理被替换的旧同号文档/旧 Task 文档，避免新旧文件并存造成重复编号。

## 目标 / 范围

建立迁移分支；把文档包落地到分支；将旧 STM32/ESP32-CAM 代码、旧测试记录、旧根目录
Task 文档与旧同号 docs 归档/清理；创建新主线目录。
本任务不写 i.MX6ULL 业务代码，不修改 Flask 业务逻辑。

## 前置条件

需要能访问当前 git 仓库，并已拿到本文档包（解压目录，下称 `<PKG>`）。

> **[CLAUDE_CODE_TODO | VERIFY]** 确认当前仓库工作区是否干净
> - 为何 GPT 给不了：沙箱不能知道用户本地 git 状态。
> - 期望产物/操作：运行 `git status` 并决定是否先提交/暂存/备份。
> - 回填位置：Task01 前置条件
> - 验收：工作区无意外未提交修改，或已明确保留。

## 操作步骤

1. 切换 main 并拉取最新。
2. 新建 `migration/imx6ull-opi5-edge-ai` 分支。
3. 创建新主线目录。
4. `git mv` 旧固件到 `legacy/2026-stm32-esp32/`。
5. **把旧根目录 Task 文档移入 legacy**（它们是第一版执行历史，保留可追溯）。
6. **删除被替换的旧同号设计文档**（内容已被新 `_端边协同版` 等文件取代；保留只会造成同号重复）。
7. **把文档包 overlay 复制进分支**（新文档与代码骨架）。
8. 写 `legacy/2026-stm32-esp32/README_legacy.md`（本包已提供模板）。
9. 复制 `config/inventory.example.yaml` 为本地 `config/inventory.yaml` 并填值（不入库）。
10. 核对无重复同号 docs、无 `docs/15`、无残留 `#U` 乱码文件名。

## 命令骨架

```bash
git status
git checkout main && git pull
git checkout -b migration/imx6ull-opi5-edge-ai

# 3) 新目录
mkdir -p edge/imx6ull-controller/{src,include,config,systemd}
mkdir -p edge/opi5-ai/{service,models,scripts}
mkdir -p common/contracts scripts tests/{imx6ull,opi5,integration,legacy}
mkdir -p legacy/2026-stm32-esp32

# 4) 旧固件归档（文件不存在则跳过，不要伪造历史）
git mv firmware/stm32     legacy/2026-stm32-esp32/stm32
git mv firmware/esp32cam  legacy/2026-stm32-esp32/esp32cam

# 4b) 旧测试记录归档
git mv tests/* tests/legacy/ 2>/dev/null || true   # 仅迁移旧记录；新目录已建

# 5) 旧根目录 Task 文档进 legacy（按本地实际文件名）
mkdir -p legacy/2026-stm32-esp32/old-claude-tasks
git mv CLAUDE_CODE_TASK_01_refactor_improved.md       legacy/2026-stm32-esp32/old-claude-tasks/ 2>/dev/null || true
git mv CLAUDE_CODE_TASK_02_flask_server_improved.md   legacy/2026-stm32-esp32/old-claude-tasks/ 2>/dev/null || true
git mv CLAUDE_CODE_TASK_03_esp32cam_improved.md       legacy/2026-stm32-esp32/old-claude-tasks/ 2>/dev/null || true

# 6) 删除被替换的旧同号设计文档（01–14 旧名，含 #U 乱码名；00 保留同名将被覆盖）
#    新文件名见 CANONICAL_DECISIONS.md 0.5 映射表。git 历史仍可追溯旧内容。
git rm -f "docs/01_"*.md "docs/02_"*.md "docs/03_"*.md "docs/04_"*.md "docs/05_"*.md \
          "docs/06_"*.md "docs/07_"*.md "docs/08_"*.md "docs/09_"*.md "docs/10_"*.md \
          "docs/11_"*.md "docs/12_"*.md "docs/13_"*.md "docs/14_"*.md
# 注意：上一行会同时删除新名文件吗？不会——此刻新文件尚未 overlay 进来（见第 7 步）。

# 7) overlay 文档包（用解压目录 <PKG> 覆盖/新增；inventory.yaml 等本地文件不在包内）
rsync -av --exclude '.git' <PKG>/ ./
# 或：cp -r <PKG>/* ./  （注意保留已存在的 server/backend 实际代码，勿覆盖真实 app.py）

# 9) 本地配置
cp config/inventory.example.yaml config/inventory.yaml   # 填真实值，禁止提交

git add -A
git status
```

> 注意第 7 步：文档包内 `server/backend/` 只含说明类文件（如 CONTRACT_EXTENSION_NOTES.md），
> **不要用包覆盖真实的 `server/backend/app.py` / `database.py`**。如包内不含这两个真实文件则无风险；
> 若用 `cp`，请显式排除已存在的后端实现。

## 产出物

- 迁移分支 `migration/imx6ull-opi5-edge-ai`
- `legacy/2026-stm32-esp32/`（stm32、esp32cam、old-claude-tasks、旧 tests）+ `README_legacy.md`
- 新主线目录（edge/、common/、scripts/、tests/ 新子目录）
- 文档包落地（CANONICAL、执行指南、新 docs/00–14、Task01–08 等）
- `docs/migration/2026-06-04_平台迁移决策记录.md`

## 验收标准

- [ ] `git status` 显示移动清晰，旧代码未删除、仅进入 legacy。
- [ ] `ls docs` 中每个编号只剩一个文件，无 `#U` 乱码名，无 `docs/15`。
- [ ] `find . -maxdepth 1 -name 'CLAUDE_CODE_TASK_0*' ` 只剩新的 01–08。
- [ ] `legacy/2026-stm32-esp32/README_legacy.md` 写明“网络侧已验证、PA9 UART 输出已验证、GPIO13 桥接未最终验收”。
- [ ] `config/inventory.yaml` 已创建且未被 `git add`。

## 禁止事项

- 不删除旧代码（只 `git mv` 进 legacy）。
- 不把旧 Task04 写成最终通过。
- 不在本任务修改硬件业务代码或真实 Flask 实现。
- 不提交 `config/inventory.yaml`。

## 完成后回填

- 更新 `AGENTS.md` 阶段为“Task01 已完成”。
- 在 `tests/legacy/` 写入迁移记录。
- 关闭相关 VERIFY TODO。
