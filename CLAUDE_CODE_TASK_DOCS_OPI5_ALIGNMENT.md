# CLAUDE_CODE_TASK_DOCS_OPI5_ALIGNMENT — OPi5 主线文档对齐与归档

## 目标

将仓库文档统一到当前事实：Orange Pi 5 是当前演示主控。i.MX6ULL / STM32 / ESP32-CAM 仅作为历史迭代证据归档。

## 归档清单

| 文件 | 归档位置 |
|---|---|
| `CLAUDE_CODE_TASK_02_wsl_imx6ull_toolchain_ssh.md` | `docs/archive/2026-imx6ull-stage/tasks/`（根目录保留 stub） |
| `CLAUDE_CODE_TASK_03_imx6ull_gpio_i2c_pwm_bringup.md` | 同上 |
| `CLAUDE_CODE_TASK_04_imx6ull_v4l2_capture.md` | 同上 |
| `CLAUDE_CODE_TASK_07_end_edge_vertical_slice.md` | 同上 |
| `CLAUDE_CODE_TASK_08_pan_tilt巡检演示.md` | 同上 |
| `CLAUDE_CODE_TASK_10_imx6ull_sensor_actuator_p0.md` | 同上 |
| `CLAUDE_CODE_TASK_11_imx6ull_p1_oled_relay_soctemp_pump.md` | 同上 |
| `VERTICAL_SLICE_INTEGRATION_CHECKLIST.md` | `docs/archive/2026-imx6ull-stage/` |

## 修改清单

| 文件 | 修改内容 |
|---|---|
| `CANONICAL_DECISIONS.md` | 全面重写为 OPi5 主线，移除 i.MX6ULL P0/P1 大段，更新架构/API/引脚 |
| `README.md` | 重写为 OPi5 主线，补充 device-agent/frontend/notification |
| `CLAUDE.md` | 精简为 OPi5 主线，移除 i.MX 三机协作 |
| `AGENTS.md` | 同步 CLAUDE.md |
| `CLAUDE_CODE_EXECUTION_GUIDE.md` | 重写任务顺序，移除旧 Task02-11 |
| `DEVPLAN.md` | 改为当前阶段看板 |
| `docs/00_README.md` | 更新为 OPi5 主线文档地图 |

## 新增文件

| 文件 | 说明 |
|---|---|
| `docs/archive/2026-imx6ull-stage/README.md` | i.MX6ULL 阶段归档说明 |
| `docs/archive/2026-stm32-esp32/README.md` | STM32/ESP32 归档说明 |
| `docs/archive/historical-tests-note.md` | 历史测试说明 |
| `docs/reference/hardware/opi5_current_hardware_wiring.md` | OPi5 接线参考 |
| `docs/reference/hardware/opi5_pinmap_current.md` | OPi5 引脚映射 |
| `CLAUDE_CODE_TASK_DOCS_OPI5_ALIGNMENT.md` | 本文件 |

## grep 检查命令

```bash
# 旧主控污染检查（应仅在 archive/legacy/tests 命中）
grep -RInE "安全闭环必须留在 i\.MX|i.MX.*唯一安全|当前主线.*i.MX|PCA9685.*当前主线|ESP32-CAM.*当前主线|STM32.*当前主线" \
  README.md CLAUDE.md AGENTS.md DEVPLAN.md CANONICAL_DECISIONS.md CLAUDE_CODE_EXECUTION_GUIDE.md docs/0*.md || true

# OPi5 新能力检查（应命中）
grep -RInE "opi5-device-agent|/api/devices|/api/telemetry|/api/ai/observations|/api/video|/api/notification|edge-console|notification_config" \
  README.md CLAUDE.md AGENTS.md DEVPLAN.md CANONICAL_DECISIONS.md CLAUDE_CODE_EXECUTION_GUIDE.md docs/07*.md || true
```

## 最小验证

```bash
git status --short
python -m py_compile server/backend/*.py edge/opi5-device-agent/*.py
```
