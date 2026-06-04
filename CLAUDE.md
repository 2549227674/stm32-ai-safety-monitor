# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

本项目是一套基于 STM32 的端云协同 AI 多模态安全巡检与设备健康监测系统。当前处于 **P0 本地硬件 bring-up 阶段**，STM32 CubeMX + Keil MDK-ARM + HAL 工程已建立，BSP 模块已拆分，部分传感器和执行器已验证。

**核心概念**：STM32F103C8T6 作为实时安全主控，ESP32-CAM 作为边缘视觉协处理器，云端 AI 负责异常解释。系统融合烟雾、火焰、PIR、门磁、温湿度、震动六类传感器数据，通过多模态风险评分状态机驱动本地执行器（蜂鸣器、RGB LED、继电器、风扇、水泵）。

**STM32 开发方式**：STM32 端采用 STM32CubeMX + Keil MDK-ARM + HAL 库开发。代码统一使用 HAL API，不使用也不混用 STM32F10x 标准外设库。CubeMX 生成文件中的自定义代码必须写在 `USER CODE BEGIN` / `USER CODE END` 区域内，避免重新生成工程时丢失。

## 四层系统架构

```
第四层 ─ 云端 AI 智能解释层    （图像复核、异常解释、JSON 返回）
第三层 ─ 服务器 / Web / 手机端  （事件存储、Dashboard、REST API、可选 MQTT）
第二层 ─ ESP32-CAM 边缘视觉层  （抓拍、本地缓存、Web 预览、UART 从机）
第一层 ─ STM32 本地实时控制层  （传感器融合、风险评分、状态机、执行器）
```

**核心约束**：STM32 是唯一主控制器，ESP32-CAM 不控制执行器。本地安全闭环必须能在无网络、无摄像头、无 AI 的情况下独立运行。

## 仓库结构

```
stm32-ai-safety-monitor/
├── README.md                    # 项目说明
├── CLAUDE.md                    # Claude Code 指南
├── AGENTS.md                    # Agent 工作指南
├── DEVPLAN.md                   # 垂直切片执行计划
├── docs/                        # 设计文档（14 个文档，docs/00-14）
├── firmware/                    # 固件代码
│   ├── stm32/                   # STM32 固件（已有 CubeMX + Keil 工程）
│   └── esp32cam/                # ESP32-CAM 固件（后续阶段）
├── server/                      # 服务器端（后续阶段）
├── hardware/                    # 硬件资料
├── tests/                       # 测试记录
└── project/                     # 项目文档（报告、PPT、提交材料）
```

## 文档结构

所有设计文档位于 `docs/` 目录：

| 文件 | 内容 |
|---|---|
| `docs/00_README.md` | 项目总览、架构摘要、推荐阅读顺序 |
| `docs/01_项目立项与总体设计.md` | 项目定位、应用场景、创新点 |
| `docs/02_需求分析与功能优先级.md` | 功能需求、非功能需求、四层功能划分 |
| `docs/03_硬件系统设计与供电安全.md` | 引脚连接、供电拓扑、共地、安全注意事项 |
| `docs/04_软件架构与模块划分.md` | 裸机调度、模块分层、目录结构、关键结构体与伪代码 |
| `docs/05_开发计划与MVP验收标准.md` | MVP 验收标准、第一天/第三天里程碑 |
| `docs/06_状态机与联动机制.md` | 状态机、风险评分、传感器联动、执行器策略 |
| `docs/07_UART协议与JSON_Contract.md` | STM32↔ESP32-CAM UART 帧协议、ACK/NACK、心跳、错误码 |
| `docs/08_ESP32CAM抓拍_Web_服务器方案.md` | ESP32-CAM 抓拍、Web Dashboard、服务器 API、MQTT |
| `docs/09_云端AI多模态异常解释方案.md` | AI 输入输出 JSON、图像复核、降级策略 |
| `docs/10_MPU6050设备健康监测方案.md` | GY-521 震动检测算法（RMS、峰峰值） |
| `docs/11_测试计划与风险矩阵.md` | 单模块/集成/系统测试计划、风险矩阵 |
| `docs/12_材料清单与获取计划.md` | 物料清单、采购计划 |
| `docs/13_实训报告与答辩演示方案.md` | 报告结构、答辩演示脚本（3-5 分钟） |
| `docs/14_上报表内容与答辩摘要.md` | 上报表内容、答辩摘要 |

## 当前 P0 状态

| 编号 | 测试项 | 状态 |
|---|---|---|
| P0-00 | STM32 最小工程 + PC13 LED | 已完成 |
| P0-02 | AD 四位按键 (PA0) | 已完成 |
| P0-03 | 蜂鸣器 (PB5) / RGB (PB0/PB1/PB10) | 已完成 |
| P0-04A | OLED I2C (PB6/PB7) | 已完成 |
| P0-04B | PIR (PB15) | 已完成 |
| P0-04B | MQ-2 DO (PB14) | 软件链路通过，实物暂缓 |
| P0-04B | 门磁 (PA4) | 已验证，常开型 |
| P0-05A | 继电器 (PA6) | 未到货，暂缓 |
| P0-05B | 风扇 (PA7) | 已通过，L9110S 驱动 |
| P0-05C | 水泵 (PB8) | 未开始 |
| P0-06 | risk_score 风险评分 | 未开始 |
| P0-07 | safety_fsm 状态机 | 未开始 |

详细测试记录见 `tests/p0_bringup_record.md` 和 `firmware/stm32/docs/bringup_log.md`。

## STM32 当前代码结构

已拆分的 BSP/App 模块（Task 01 已完成）：

```
firmware/stm32/SafetyMonitor/Core/Inc/
├── bsp_key.h          # AD 按键 BSP
├── bsp_inputs.h       # 传感器输入 BSP
├── bsp_outputs.h      # 执行器输出 BSP
├── bsp_oled.h         # OLED I2C 驱动
├── bsp_dht11.h        # DHT11（保留但暂缓，当前不使用）
├── app_display.h      # OLED 调试页面
└── main.h             # CubeMX 生成

firmware/stm32/SafetyMonitor/Core/Src/
├── bsp_key.c
├── bsp_inputs.c
├── bsp_outputs.c
├── bsp_oled.c
├── bsp_dht11.c        # 保留但暂缓，main.c 不 include/init/read
├── app_display.c
├── main.c             # CubeMX 生成 + USER CODE 区域
├── adc.c, gpio.c, i2c.c, tim.c  # CubeMX 生成
└── stm32f1xx_it.c     # 中断处理
```

**DHT11 当前状态**：`bsp_dht11.h/c` 保留，但 main.c 不 include、不 init、不 read。`AppDisplay_Update` 最后三个参数使用 `0U, 0U, 0U`。不参与 risk_score / safety_fsm。

## 关键技术细节

### STM32 软件栈
- 裸机前后台 + SysTick 定时调度（不使用 RTOS）
- 任务周期：按键 10ms、传感器 50-100ms、风险评分 100-200ms、状态机 100ms、OLED 200-500ms、MPU6050 20ms、ESP 心跳 2s
- 关键枚举：`SystemState_t`、`RiskLevel_t`、`BuzzerMode_t`、`RgbMode_t`

### risk_score 风险评分（0-10 分制）

| 分值 | 风险等级 |
|---:|---|
| 0-2 | NORMAL |
| 3-5 | PRE_ALARM |
| 6-8 | ALARM |
| >=9 | DANGER |

### UART 协议（STM32↔ESP32-CAM）
- 帧格式：`0xAA 0x55 | version | msg_type | seq | cmd | len | payload | checksum`
- 命令码：PING/PONG、SNAP_REQ/OK/FAIL、UPLOAD_REQ/OK/FAIL、AI_REQ/RESULT/FAIL、ACK/NACK
- payload 0-64 字节，checksum 为 version 到 payload 的异或

### 硬件安全规则
- 所有 GND 必须共地（推荐星形接地）
- MQ-2 AO 可能超过 3.3V，必须加分压电阻（10k/20k）
- 水泵必须经 MOS 管/驱动板控制，禁止 GPIO 直接驱动
- 水泵软件限时：喷淋 1-3 秒，冷却 10 秒以上
- ESP32-CAM 需独立 5V 支路（Wi-Fi 和拍照峰值电流大）
- 风扇/继电器/水泵不得由 ST-Link 或 STM32 GPIO 直接供电，必须独立供电并使用驱动模块

### CubeMX / Keil 开发规则
- 引脚和外设配置优先通过 STM32CubeMX 图形界面修改并 Generate Code
- 不要手写 main.h / gpio.c / adc.c / i2c.c / tim.c
- 用户业务逻辑写在 USER CODE 区域或独立 Core/Inc、Core/Src 模块
- 不混用 STM32F10x 标准外设库
- 不引入 FreeRTOS

## 最小成功标准

系统可提交的基础条件：
1. 任一传感器（烟雾/火焰/PIR/门磁）触发
2. STM32 计算风险评分并驱动状态机转换
3. OLED 显示当前状态和风险等级
4. 蜂鸣器/RGB LED 提供反馈
5. 至少一个执行器（风扇/继电器/水泵）动作
6. 按键可布防、撤防、静音、复位

## A 类冲刺目标（超越 MVP）

- ESP32-CAM 异常抓拍成功（SNAP_OK 可回显）
- Web Dashboard 可在手机浏览器查看当前状态和抓拍图片
- 服务器接收并存储至少一条事件和图片
- 云端 AI 返回异常解释 JSON 并展示在 Web 页面
- 断网/AI 失败时本地系统继续工作，Web 显示"AI 分析不可用"
