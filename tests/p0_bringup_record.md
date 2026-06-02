# P0 Bring-up Test Record

本文档作为 P0 硬件 bring-up 阶段的验收依据。当前记录只覆盖已实际验证内容，不把预留功能写成已完成。

## 进度记录

| 编号 | 测试项 | 状态 | 测试结果 |
|---|---|---|---|
| P0-00 | STM32 最小工程 + PC13 LED 闪烁 | 已完成 | STM32CubeMX + Keil MDK-ARM + HAL 工程可编译烧录；ST-Link 可识别、可下载；PC13 板载 LED 可闪烁；HSE 8MHz + PLL x9，SYSCLK 72MHz；PA13/PA14 保留 SWD |
| P0-01 | USART1 串口日志输出 | 暂缓 | USB-TTL 未到货；计划使用 PA9/PA10；当前调试依赖 PC13 LED 和 Keil Debug Watch |
| P0-02 | AD 四位按键输入验证 | 已完成 | AD 按键模块 VCC 接 3.3V，GND 接 GND，AD 接 PA0/ADC1_IN0；K1/K2/K3/K4 ADC 值可区分 |
| P0-03 | 蜂鸣器 / RGB LED 输出验证 | 已完成 | PB5 蜂鸣器低电平触发；PB0/PB1/PB10 可控制 RGB R/G/B；按 K1 蜂鸣器响，K2 红灯，K3 绿灯，K4 蓝灯 |
| P0-04 | 传感器输入验证 | 未开始 | 下一步建议优先火焰 DO、PIR、门磁、MQ-2 AO |

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
| ST-Link 供电 | 当前可用于 STM32 和小模块 P0 bring-up |
| 大负载供电 | 后续风扇、水泵、继电器不得由 ST-Link 或 STM32 GPIO 直接供电，必须独立供电并使用驱动模块 |
