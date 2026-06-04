# AGENTS.md

本文件为 AI agent（Claude Code、Codex 等）提供仓库工作指南。与 `CLAUDE.md` 互补，侧重于当前状态、操作规则和禁止事项。

## 当前项目状态

**阶段**：P0 本地硬件 bring-up 与本地闭环前置验证。

**已完成**：
- STM32CubeMX + Keil MDK-ARM + HAL 工程已建立
- Task 01 完成：main.c 已拆分为 BSP 模块（bsp_key、bsp_inputs、bsp_outputs、bsp_oled、app_display）
- P0-00 PC13 LED、P0-02 ADKEY、P0-03 蜂鸣器/RGB、P0-04A OLED、P0-04B PIR PB15、PA4 门磁：已通过
- P0-05B 风扇 PA7：已通过（L9110S 驱动模块）
- 阶段 3：Flask + SQLite + Dashboard 骨架已完成
- 阶段 4：ESP32-CAM WiFi + HTTP TEST 事件上报已通过（手机热点，未接 STM32/UART）

**暂缓**：
- DHT11：bsp_dht11.h/c 保留，main.c 不 include/init/read，不参与 risk_score / safety_fsm
- P0-05A 继电器 PA6：未到货
- MQ-2 实物：缺分压电阻
- USART1 日志：USB-TTL 未到货

**未开始**：
- P0-05C 水泵 PB8
- P0-06 risk_score 风险评分
- P0-07 safety_fsm 状态机
- STM32 ↔ ESP32-CAM UART 桥接（阶段 5）
- ESP32-CAM 抓拍
- 垂直切片（阶段 6+）
- 云端 AI

**重要限制**：阶段 4 只验证了 ESP32-CAM 定时 POST TEST 事件到 Flask，不代表 UART 桥接完成，不代表抓拍完成，不代表 STM32 已接入 ESP32-CAM。

## 硬件引脚验证状态

| 引脚 | 功能 | 状态 |
|---|---|---|
| PC13 | 板载 LED | 已验证 |
| PA0 | ADC1_IN0 / AD 按键 | 已验证 |
| PB5 | 蜂鸣器 | 已验证 |
| PB0/PB1/PB10 | RGB R/G/B | 已验证 |
| PB6/PB7 | I2C1 OLED | 已验证 |
| PB15 | PIR DO | 已验证 |
| PB12 | 火焰 DO | 部分验证 |
| PB14 | MQ-2 DO | 软件链路通过 |
| PA4 | 门磁 DO | 已验证 |
| PA6 | RELAY1_CTRL | 预留，未验证 |
| PA7 | FAN_CTRL | 已验证（L9110S） |
| PB8 | PUMP_CTRL | 预留，未验证 |
| PA9/PA10 | USART1 | 预留，未验证 |
| PA2/PA3 | USART2 | 预留，未验证 |
| PA1 | ADC1_IN1 / MQ-2 AO | 预留，未验证 |

详细引脚信息见 `firmware/stm32/docs/pinmap.md`。

## agent 工作规则

### 基本原则

1. **每次只做一个任务**。不要在一个 commit 中混合不相关的修改。
2. **修改前先说明**会改哪些文件，等用户确认后再动手。
3. **不要擅自重构 `docs/00-14`**。这些是已审核的设计文档，修改需用户明确指示。
4. **不要把未验证硬件写成已完成**。未实测的引脚/模块必须标注为"预留"、"待验证"或"暂缓"。
5. **不要推荐**机械臂、小车、PCB 打样、220V 强电、公网云平台强依赖。
6. **硬件验证记录必须写清**：接线、供电、共地、安全限制和验收现象。

### CubeMX / Keil 规则

1. 引脚和外设配置优先通过 STM32CubeMX 图形界面修改并 Generate Code。
2. 不要手写 `main.h` / `gpio.c` / `adc.c` / `i2c.c` / `tim.c`。
3. 用户业务逻辑写在 `USER CODE BEGIN` / `USER CODE END` 区域或独立 `Core/Inc`、`Core/Src` 模块。
4. 不混用 STM32F10x 标准外设库，统一使用 HAL API。
5. 不引入 FreeRTOS。

### DHT11 规则

1. `bsp_dht11.h/c` 可以保留，但当前暂缓。
2. main.c 不 include、不 init、不 read DHT11。
3. `AppDisplay_Update` 最后三个参数使用 `0U, 0U, 0U`。
4. DHT11 不参与 risk_score / safety_fsm。
5. 不要把 DHT11 写成已通过。

### 文档修改边界

| 文件 | agent 可修改 | 说明 |
|---|---|---|
| `CLAUDE.md` | 是 | 本指南 |
| `AGENTS.md` | 是 | 本指南 |
| `DEVPLAN.md` | 是 | 执行计划看板 |
| `firmware/stm32/docs/*` | 是 | bring-up 日志、引脚表等 |
| `tests/*` | 是 | 测试记录 |
| `docs/00-14` | 否 | 除非用户明确指示 |
| `*.ioc` | 否 | 除非用户明确指示 |
| `*.uvprojx` / `*.uvoptx` | 否 | 除非用户明确指示 |
| 源代码 `*.c` / `*.h` | 视任务 | 需用户确认范围 |

## 禁止事项

1. **不要**把未验证的引脚写成"已验证"。
2. **不要**把 PA6 / RELAY1_CTRL 写成已验证（继电器未到货）。
3. **不要**把 PB8 / PUMP_CTRL 写成已验证（水泵未开始）。
4. **不要**把 DHT11 写成已通过。
5. **不要**修改 `docs/06` 的 risk_score 规则，除非用户明确指示。
6. **不要**在 STM32 代码中引入 FreeRTOS。
7. **不要**混用 STM32F10x 标准外设库和 HAL 库。
8. **不要**让 ESP32-CAM 控制本地执行器。
9. **不要**接 220V 强电。
10. **不要**用 ST-Link 3.3V 给风扇、水泵、继电器、ESP32-CAM 供电。

## 最小成功标准

系统可提交的基础条件：
1. 任一传感器（烟雾/火焰/PIR/门磁）触发
2. STM32 计算风险评分并驱动状态机转换
3. OLED 显示当前状态和风险等级
4. 蜂鸣器/RGB LED 提供反馈
5. 至少一个执行器（风扇/继电器/水泵）动作
6. 按键可布防、撤防、静音、复位
