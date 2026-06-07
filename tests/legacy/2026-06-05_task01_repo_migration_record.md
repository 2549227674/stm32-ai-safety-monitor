# Task01 仓库迁移与 legacy 归档记录

日期：2026-06-05

## 操作范围

- 新建迁移分支：`migration/imx6ull-opi5-edge-ai`
- 将 `firmware/stm32/` 归档到 `legacy/2026-stm32-esp32/stm32/`
- 将 `firmware/esp32cam/` 归档到 `legacy/2026-stm32-esp32/esp32cam/`
- 将旧根目录 Task01/02/03 文档归档到 `legacy/2026-stm32-esp32/old-claude-tasks/`
- 删除旧 `docs/01`-`docs/14` 同号设计文档，并 overlay 新文档体系
- 将旧测试记录归档到 `tests/legacy/`
- 保留真实 Flask 后端 `server/backend/app.py` 与 `server/backend/database.py`

## 迁移说明

本记录只说明仓库迁移动作完成，不代表 legacy STM32/ESP32-CAM 链路新增验收通过。

已知 legacy 事实以 `AGENTS.md` 和 `legacy/2026-stm32-esp32/README_legacy.md` 为准：

- ESP32-CAM 网络 POST TEST 到 Flask：历史已验证
- STM32 PA9 UART 输出：历史已验证
- STM32 到 ESP32-CAM GPIO13 物理桥接：未最终验收

## 验收

- 新主线目录 `edge/`、`common/`、`scripts/`、`tests/` 已落地
- `docs/00`-`docs/14` 使用迁移后的新文件名
- `config/inventory.yaml` 已从 example 创建，本地使用，不提交
- mock AI 服务与脚本由审核补丁提供，后续 Task05/Task06 验证
