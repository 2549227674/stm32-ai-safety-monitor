# P0 Bring-up Test Record

本文档作为 P0 硬件 bring-up 阶段的验收依据。当前记录只覆盖已实际验证内容，不把预留功能写成已完成。

## 进度记录

| 编号 | 测试项 | 状态 | 测试结果 |
|---|---|---|---|
| P0-00 | STM32 最小工程 + PC13 LED 闪烁 | 已完成 | STM32CubeMX + Keil MDK-ARM + HAL 工程可编译烧录；ST-Link 可识别、可下载；PC13 板载 LED 可闪烁；HSE 8MHz + PLL x9，SYSCLK 72MHz；PA13/PA14 保留 SWD |
| P0-01 | USART1 串口日志输出 | 暂缓 | USB-TTL 未到货；计划使用 PA9/PA10；当前调试依赖 PC13 LED 和 Keil Debug Watch |
| P0-02 | AD 四位按键输入验证 | 已完成 | AD 按键模块 VCC 接 3.3V，GND 接 GND，AD 接 PA0/ADC1_IN0；K1/K2/K3/K4 ADC 值可区分 |
| P0-03 | 蜂鸣器 / RGB LED 输出验证 | 已完成 | PB5 蜂鸣器低电平触发；PB0/PB1/PB10 可控制 RGB R/G/B；按 K1 蜂鸣器响，K2 红灯，K3 绿灯，K4 蓝灯 |
| P0-04A-1 | OLED 固定文本显示 | 已完成 | I2C1 PB6/PB7，SSD1306 OLED，地址 0x3C；已显示 Safety Monitor / P0 OLED OK 等固定文本 |
| P0-04A-2 | OLED 实时调试页面 | 已完成 | 显示 KEY ADC、KEY ID、FLAME、PIR、MQ2、STATE，输入变化可在页面反映 |
| P0-04B-0 | 火焰传感器 DO 输入 | 部分完成 | 3 针 VCC/GND/DO 模块接 PB12 / FLAME_DO；无火焰状态显示正常；真实触发待安全补测 |
| P0-04B-1 | PIR HC-SR501 输入 | 已完成 | HC-SR501 OUT 最终接 PB15 / PIR_DO，实物验证通过；PB13 原方案输入链路不可靠，暂不使用 |
| P0-04B-2 | MQ-2 DO 输入 | 软件链路通过，实物暂缓 | PB14 / MQ2_DO 模拟高低电平通过；MQ-2 实物 DO/AO 因缺少 10k/20k 分压电阻暂缓 |
| P0-04B-3 | MQ-2 AO 输入 | 未开始 | 后续规划 PA1 / ADC1_IN1；5V 供电时必须分压后接入 STM32 ADC |
| P0-05 | 继电器 / 风扇 / 水泵低压驱动 | 未开始 | N-MOSFET 模块留到执行器驱动；不用于 MQ-2 分压 |
| P0-06 | risk_score 风险评分 | 未开始 | 待输入输出链路稳定后实现 |
| P0-07 | safety_fsm 状态机 | 未开始 | 待 risk_score 和执行器输出验证后接入 |
| P0-08 | 按键功能映射 | 未开始 | P0-02 只完成 AD 按键识别，布防/撤防/静音/复位业务逻辑未接入 |
| P0-09 | P0 本地闭环整体验收 | 未开始 | 不接 ESP32-CAM/Web/服务器/AI 也必须独立完成本地报警和执行器联动 |

## AD 按键实测值

| 输入 | 实测 ADC |
|---|---|
| 不按键 | 0x0005-0x0007 |
| K1 | 0x0648-0x064D |
| K2 | 0x0972-0x0976 |
| K3 | 0x0CA8-0x0CAB |
| K4 | 0x0FFF |

## 当前供电记录

| 项目 | 结论 |
|---|---|
| 面包板电源轨 | STM32 3.3V 接红色电源轨，STM32 GND 接蓝色电源轨 |
| 小模块供电 | AD 按键、蜂鸣器、RGB 等小模块从面包板电源轨取电 |
| ST-Link 供电 | 只适合 STM32 和少量小功耗模块临时 bring-up；MQ-2、继电器、风扇、水泵、ESP32-CAM 不得从 ST-Link 3.3V 取电 |
| Type-C 供电 | 手机 Type-C 充电器可给 STM32 核心板供电；此时 ST-Link 只接 SWDIO/SWCLK/GND，不接 ST-Link 3.3V |
| 3.3V/5V 分轨 | 黑色面包板电源模块可通过跳帽分别输出 3.3V/5V；两侧正极不得直接相连，所有模块 GND 必须与 STM32 GND 共地 |
| MQ-2 电平保护 | 5V 供电时 DO/AO 接 STM32 前必须分压；AO 后续 PA1/ADC1_IN1 |
| 12V 禁止直连 | 12V 适配器/电池不得直接接 STM32、OLED、PIR、MQ-2 |
| 大负载供电 | 后续风扇、水泵、继电器不得由 ST-Link 或 STM32 GPIO 直接供电，必须独立供电并使用驱动模块 |
