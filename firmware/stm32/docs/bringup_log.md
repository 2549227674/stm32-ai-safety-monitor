# STM32 Bring-up Log

## P0 Bring-up 进度表

| 编号 | 阶段 | 状态 | 引脚 / 配置 | 现象 | 结论 | 备注 |
|---|---|---|---|---|---|---|
| P0-00 | STM32 最小工程 + PC13 LED 闪烁 | 已完成 | PC13 GPIO_Output；HSE 8MHz + PLL x9，SYSCLK 72MHz；PA13/PA14 SWD | ST-Link 可识别、可下载；Keil 工程可编译烧录；PC13 板载 LED 闪烁 | 主控开发链路跑通 | PC13 板载 LED 低电平点亮；PA13/PA14 必须保留 |
| P0-01 | USART1 串口日志输出 | 暂缓 | PA9 USART1_TX，PA10 USART1_RX，115200 8N1 | USB-TTL 未到货，未复测 | 暂不作为当前验收项 | 当前调试临时依赖 PC13 LED 和 Keil Debug Watch |
| P0-02 | AD 四位按键输入验证 | 已完成 | PA0/ADC1_IN0；软件触发；单通道；轮询；Sampling Time 55.5 Cycles；ADC Prescaler /6 | 不按键约 0x0005-0x0007；K1 约 0x0648-0x064D；K2 约 0x0972-0x0976；K3 约 0x0CA8-0x0CAB；K4 约 0x0FFF | K1-K4 可稳定区分 | 该模块是 AD 模拟按键，不是 4 路独立 GPIO |
| P0-03 | 蜂鸣器 / RGB LED 输出验证 | 已完成 | PB5 Buzzer；PB0 RGB_R；PB1 RGB_G；PB10 RGB_B | 不按键：蜂鸣器不响、RGB 不亮、PC13 心跳；K1 蜂鸣器响；K2 红；K3 绿；K4 蓝 | 声光输出 GPIO 验证通过 | 蜂鸣器低电平触发，PB5 初始电平建议 High |
| P0-04A-1 | OLED 固定文本显示 | 已完成 | I2C1 PB6/PB7，SSD1306，地址 0x3C | 显示 Safety Monitor / P0 OLED OK / I2C1 PB6/PB7 / ADDR:0x3C | OLED I2C 固定文本通过 | 已新增 bsp_oled.h / bsp_oled.c |
| P0-04A-2 | OLED 实时调试页面 | 已完成 | app_display，周期刷新 | KEY ADC、KEY ID、FLAME、PIR、MQ2、STATE 可显示并随输入变化 | P0 调试页面通过 | 已新增 app_display.h / app_display.c |
| P0-04B-0 | 火焰传感器 DO 输入 | 部分完成 | PB12 / FLAME_DO | 无火焰状态显示正常 | 输入读取链路部分通过 | 真实火焰/安全触发待补测 |
| P0-04B-1 | PIR HC-SR501 输入 | 已完成 | PB15 / PIR_DO | HC-SR501 OUT 接 PB15，OLED PIR 显示变化正常 | PB15 方案通过 | PB13 原方案输入链路不可靠，不再作为 PIR 默认引脚 |
| P0-04B-2 | MQ-2 DO 输入 | 软件链路通过，实物暂缓 | PB14 / MQ2_DO | 模拟高低电平时 OLED MQ2/STATE 可变化 | 软件链路通过 | MQ-2 实物 DO/AO 待 10k/20k 分压电阻 |
| P0-04B-3 | MQ-2 AO 输入 | 未开始 | 后续 PA1 / ADC1_IN1 | 未测试 | 待分压后接入 | 5V 供电时必须分压 |
| P0-05 | 继电器 / 风扇 / 水泵低压驱动 | 未开始 | 待分配，需驱动模块 | 未测试 | 待 P0-05 | N-MOSFET 模块留作执行器驱动，不用于 MQ-2 分压 |
| P0-06 | risk_score 风险评分 | 未开始 | 软件模块 | 未测试 | 待输入稳定后实现 | 本地安全判断由 STM32 完成 |
| P0-07 | safety_fsm 状态机 | 未开始 | 软件模块 | 未测试 | 待 risk_score | 不依赖 ESP32-CAM/Web/AI |
| P0-08 | 按键功能映射 | 未开始 | PA0 AD 按键业务逻辑 | 未测试 | 待状态机接入 | P0-02 仅完成 ADC 识别 |
| P0-09 | P0 本地闭环整体验收 | 未开始 | STM32 本地闭环 | 未测试 | 待 P0-05~P0-08 | 不接 ESP32-CAM 也必须独立演示 |

## AD 按键阈值记录

| Key | ADC 阈值建议 |
|---|---|
| KEY_NONE | adc < 500 |
| KEY_1 | 1200 <= adc <= 1900 |
| KEY_2 | 2100 <= adc <= 2700 |
| KEY_3 | 3000 <= adc <= 3500 |
| KEY_4 | adc >= 3800 |
| KEY_UNKNOWN | 其他区间 |

## 最小工程验证检查表

| 项目 | 状态 | 备注 |
|---|---|---|
| CubeMX 工程创建 | 已完成 | 工程名 SafetyMonitor |
| 芯片选择 STM32F103C8T6 | 已完成 | CubeMX 芯片为 STM32F103C8Tx |
| SYS Serial Wire 配置 | 已完成 | PA13 SWDIO、PA14 SWCLK |
| 时钟配置 | 已完成 | HSE 8MHz + PLL x9，SYSCLK 72MHz |
| Keil MDK-ARM 工程生成 | 已完成 | Keil 可打开工程 |
| Keil 编译通过 | 已完成 | 可编译烧录 |
| ST-Link 下载成功 | 已完成 | ST-Link/V2 SWD 可识别 |
| LED 闪烁 | 已完成 | PC13 板载 LED 可闪烁 |
| USER CODE 重新生成不丢失 | 待复核 | 后续 CubeMX 重新生成后检查 |

## 基础外设检查表

| 项目 | 状态 | 备注 |
|---|---|---|
| USART1 串口日志 | 暂缓 | USB-TTL 未到货，计划 PA9/PA10 |
| OLED 固定文本显示 | 已完成 | I2C1 PB6/PB7，地址 0x3C |
| OLED 实时调试页面 | 已完成 | 显示 KEY ADC、KEY ID、FLAME、PIR、MQ2、STATE |
| K1 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| K2 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| K3 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| K4 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| RGB LED | 已完成 | PB0/PB1/PB10 分别控制 R/G/B |
| 蜂鸣器 | 已完成 | PB5，低电平触发 |

## 传感器检查表

| 项目 | 状态 | 备注 |
|---|---|---|
| 火焰传感器 GPIO 读取 | 部分完成 | PB12 / FLAME_DO，无火焰状态正常；真实触发待安全补测 |
| PIR 传感器 GPIO 读取 | 已完成 | PB15 / PIR_DO，HC-SR501 实物验证通过；PB13 原方案弃用 |
| MQ-2 DO 读取 | 软件链路通过，实物暂缓 | PB14 / MQ2_DO，模拟高低电平通过；实物待 10k/20k 分压电阻 |
| MQ-2 ADC 读取 | 未完成 | 后续 PA1 / ADC1_IN1，5V 供电时必须分压 |
| 门磁 GPIO 读取 | 未完成 |  |
| DHT 温湿度读取 | 未完成 |  |
| MPU6050 I2C 通信 | 未完成 |  |
| MPU6050 震动数据读取 | 未完成 |  |

## OLED P0 调试页面

当前 OLED 使用 `I2C1`，`PB6=SCL`，`PB7=SDA`，地址 `0x3C`。已新增：

```text
Core/Inc/bsp_oled.h
Core/Src/bsp_oled.c
Core/Inc/app_display.h
Core/Src/app_display.c
```

当前实时调试页面大致为：

```text
Safety Monitor
P0 Debug Page

KEY ADC:xxxx
KEY ID :NONE/K1/K2/K3/K4
FLAME  :0/1
PIR:0 MQ2:0
STATE  :IDLE/PIR/SMOKE/ALARM
```

`app_display` 只负责显示 P0 调试数据，不直接读取传感器，不直接控制执行器。
