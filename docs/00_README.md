# 00 项目最终执行版文档包说明

## 1. 项目定位

项目名称：基于 i.MX6ULL 与 Orange Pi 5 的端边协同本地 AI 多模态安全巡检系统。

本项目不是单纯 AI 应用，也不是继续维护旧 STM32/ESP32-CAM 方案，而是把课程项目升级为“嵌入式 Linux 数字系统 + 本地边缘 AI”的端边协同安全巡检系统。i.MX6ULL Pro 负责底层硬件与安全闭环，Orange Pi 5 负责本地 RKNN 推理，Flask + SQLite + Dashboard 负责数据留痕和可视化。

## 2. 新旧迁移摘要

| 旧方案 | 新方案 | 判断 |
|---|---|---|
| STM32 本地主控 | i.MX6ULL Linux 本地安全控制 | 替换，状态机概念保留、实现重写 |
| ESP32-CAM 抓拍/联网 | i.MX6ULL USB 摄像头 + V4L2 + 云台 | 替换，不再继续 ESP32-CAM 主线 |
| Flask + SQLite + Dashboard | 保留并扩展 | 当前最可复用资产 |
| 云端 AI API | Orange Pi 5 RKNN + 可选本地 LLM | 替换，改为全本地边缘 AI |

## 3. 核心约束

1. 本地安全闭环优先。
2. AI 不直接控制危险执行器。
3. 不依赖公网云作为 MVP 必需条件。
4. 不接 220V 强电。
5. 未实测不写“已通过”。
6. 旧 STM32/ESP32-CAM 阶段归档为工程迭代历史。

## 4. 四层逻辑架构

| 层级 | 名称 | 物理节点 | 核心职责 | 不承担的职责 |
|---|---|---|---|---|
| 第一层 | Linux 本地安全控制层 | i.MX6ULL Pro | GPIO/I2C/PWM/MOS、传感器采样、执行器控制、`risk_score`、`safety_fsm`、离线降级 | 不运行重型 AI；不承担 Dashboard 主机 |
| 第二层 | Linux 视觉采集与巡检云台层 | i.MX6ULL Pro | USB 摄像头 V4L2 抓帧、PCA9685 控制 MG90 二维云台、图片缓存与上传 | 不做模型推理；不直接覆盖安全状态机 |
| 第三层 | 本地服务器与可视化层 | PC 初期 / Orange Pi 5 最终 | Flask + SQLite + Dashboard、事件存储、图片展示、AI 结果展示 | 不直接控制蜂鸣器、继电器、风扇、水泵 |
| 第四层 | Orange Pi 5 本地 AI 推理与解释层 | Orange Pi 5 16GB + 128G NVMe | RKNN 视觉推理、人脸/目标/异常识别、可选 rknn-llm 解释 | 不直接控制执行器；只返回 `risk_hint` 与解释 |


## 5. 文档地图与阅读顺序

| 顺序 | 文档 | 目的 |
|---|---|---|
| 1 | `CANONICAL_DECISIONS.md` | 统一事实源 |
| 2 | `docs/01_项目立项与总体设计_端边协同版.md` | 项目定位与创新点 |
| 3 | `docs/02_需求分析与功能优先级_端边协同版.md` | 功能与优先级 |
| 4 | `docs/03_硬件系统设计与供电安全_iMX6ULL_OPi5.md` | 接线、供电、安全 |
| 5 | `docs/04_软件架构与模块划分_Linux边缘控制版.md` | 软件模块与部署 |
| 6 | `docs/06_状态机与联动机制_Linux实现版.md` | 状态机与安全联动 |
| 7 | `docs/07_端边HTTP_JSON_Contract.md` | 契约权威来源 |
| 8 | `docs/09_OrangePi5_RKNN本地AI推理与解释方案.md` | AI 层实现 |
| 9 | `docs/11_测试计划与风险矩阵_端边协同版.md` | 测试与答辩证据 |
| 10 | `docs/13_实训报告与答辩演示方案_迁移版.md` | 报告/PPT/演示 |

## 6. 最小成功标准

- i.MX6ULL 能运行控制程序并产生一条事件。
- OPi5 提供 `/api/infer/vision`，至少 mock 返回结构化 JSON。
- Flask `/api/events` 能接收新事件，并保持旧事件兼容。
- Dashboard 能显示 `state/risk_score/sensors/actuators/vision/ai_result/image_url` 中的核心字段。
- 至少有一项 GPIO 或 I2C/PWM 硬件实测记录。

## 7. A 类冲刺标准

- PCA9685 + MG90 云台可扫描。
- V4L2 抓图真实可用。
- OPi5 跑通真实 RKNN demo。
- 逻辑分析仪记录至少一组 PWM/I2C/GPIO 波形。
- 报告中展示迁移决策、端边链路、离线降级与安全红线。

## 8. Legacy 说明

STM32/ESP32-CAM 不是“删除失败代码”，而是作为第一版工程探索归档。已知历史包括：ESP32-CAM 网络侧 POST 可用、STM32 PA9 UART 输出可用、PA9→GPIO13 物理桥接未最终验收。新主线不再继续修复该链路。

> **[CLAUDE_CODE_TODO | VERIFY]** 迁移分支落地后核对 docs/00 链接
> - 为何 GPT 给不了：当前包是 overlay 文档，尚未实际复制到迁移分支；真实路径需本地确认。
> - 期望产物/操作：解压本包到迁移分支后，运行 `find docs -maxdepth 2 -type f` 并核对文件名。
> - 回填位置：docs/00_README.md 第 5 节
> - 验收：所有链接路径存在，无 `docs/15`。
