# 00 文档包说明（OPi5 主线版）

## 1. 项目定位

项目名称：基于 Orange Pi 5 的本地 AI 多模态安全巡检系统。

OPi5 一板主控，同时运行本地安全闭环（`opi5_safetyd`）和 AI 推理（`opi5_ai_service`），Flask + SQLite + React edge-console 提供本地可视化。项目经历 STM32+ESP32-CAM → i.MX6ULL+OPi5 → OPi5 一板三次迭代。

## 2. 当前架构

| 模块 | 进程 | 核心职责 |
|---|---|---|
| opi5-controller | `opi5_safetyd` | GPIO/PWM 传感器采样、执行器控制、状态机 |
| opi5-ai | `opi5-ai-qwen3vl.service` | Qwen3-VL 视觉推理，`control_allowed=false` |
| opi5-device-agent | `opi5-device-agent.service` | 心跳/遥测/视频流/AI observation |
| Flask backend | `app.py` | 事件/遥测/AI/告警/通知 API |
| React edge-console | Vite build | Dashboard 多页面可视化 |

## 3. 核心约束

1. 安全闭环留在 OPi5 本地 safetyd/controller 进程
2. AI / Flask / 前端不直接控制执行器；`control_allowed=false`
3. 不接 220V；低压直流演示
4. 未实测不写"已通过"
5. 历史阶段归档在 `docs/archive/` 和 `legacy/`

## 4. 文档地图

| 顺序 | 文档 | 目的 |
|---|---|---|
| 1 | `CANONICAL_DECISIONS.md` | 统一事实源 |
| 2 | `docs/01_项目立项与总体设计_端边协同版.md` | 项目定位 |
| 3 | `docs/02_需求分析与功能优先级_端边协同版.md` | 功能优先级 |
| 4 | `docs/03_硬件系统设计与供电安全_OPi5版.md` | 硬件设计 |
| 5 | `docs/04_软件架构与模块划分_Linux边缘控制版.md` | 软件架构 |
| 6 | `docs/06_状态机与联动机制_Linux实现版.md` | 状态机 |
| 7 | `docs/07_端边HTTP_JSON_Contract.md` | API 契约权威 |
| 8 | `docs/08_OPi5视觉采集与视频流方案.md` | 视觉采集与视频流 |
| 9 | `docs/09_OrangePi5_RKNN本地AI推理与解释方案.md` | AI 实现 |
| 10 | `docs/11_测试计划与风险矩阵_端边协同版.md` | 测试证据 |
| 11 | `docs/12_材料清单与获取计划_新硬件版.md` | 材料清单 |
| 12 | `docs/13_实训报告与答辩演示方案_迁移版.md` | 报告/PPT |
| 13 | `docs/14_最终演示脚本与证据包.md` | 演示脚本 |

## 5. 硬件参考

- OPi5 接线图：`docs/reference/hardware/opi5_current_hardware_wiring.md`
- OPi5 引脚表：`docs/reference/hardware/opi5_pinmap_current.md`

## 6. 历史归档

| 阶段 | 归档位置 |
|---|---|
| i.MX6ULL 阶段 | `docs/archive/2026-imx6ull-stage/` |
| STM32/ESP32 阶段 | `docs/archive/2026-stm32-esp32/`, `legacy/2026-stm32-esp32/` |
