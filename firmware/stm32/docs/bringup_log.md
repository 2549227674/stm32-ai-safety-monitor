# STM32 Bring-up Log

## P0 Bring-up 进度表

| 编号 | 阶段 | 状态 | 引脚 / 配置 | 现象 | 结论 | 备注 |
|---|---|---|---|---|---|---|
| P0-00 | STM32 最小工程 + PC13 LED 闪烁 | 已完成 | PC13 GPIO_Output；HSE 8MHz + PLL x9，SYSCLK 72MHz；PA13/PA14 SWD | ST-Link 可识别、可下载；Keil 工程可编译烧录；PC13 板载 LED 闪烁 | 主控开发链路跑通 | PC13 板载 LED 低电平点亮；PA13/PA14 必须保留 |
| P0-01 | USART1 串口日志输出 | 暂缓 | PA9 USART1_TX，PA10 USART1_RX，115200 8N1 | USB-TTL 未到货，未复测 | 暂不作为当前验收项 | 当前调试临时依赖 PC13 LED 和 Keil Debug Watch |
| P0-02 | AD 四位按键输入验证 | 已完成 | PA0/ADC1_IN0；软件触发；单通道；轮询；Sampling Time 55.5 Cycles；ADC Prescaler /6 | 不按键约 0x0005-0x0007；K1 约 0x0648-0x064D；K2 约 0x0972-0x0976；K3 约 0x0CA8-0x0CAB；K4 约 0x0FFF | K1-K4 可稳定区分 | 该模块是 AD 模拟按键，不是 4 路独立 GPIO |
| P0-03 | 蜂鸣器 / RGB LED 输出验证 | 已完成 | PB5 Buzzer；PB0 RGB_R；PB1 RGB_G；PB10 RGB_B | 不按键：蜂鸣器不响、RGB 不亮、PC13 心跳；K1 蜂鸣器响；K2 红；K3 绿；K4 蓝 | 声光输出 GPIO 验证通过 | 蜂鸣器低电平触发，PB5 初始电平建议 High |
| P0-04 | 传感器输入验证 | 未开始 | 待分配 | 未测试 | 下一步 | 建议优先火焰 DO、PIR、门磁、MQ-2 AO |

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
| OLED 显示 | 未完成 |  |
| K1 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| K2 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| K3 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| K4 按键 | 已完成 | AD 按键 PA0/ADC1_IN0 阈值识别 |
| RGB LED | 已完成 | PB0/PB1/PB10 分别控制 R/G/B |
| 蜂鸣器 | 已完成 | PB5，低电平触发 |

## 传感器检查表

| 项目 | 状态 | 备注 |
|---|---|---|
| MQ-2 ADC 读取 | 未完成 |  |
| 火焰传感器 GPIO 读取 | 未完成 |  |
| PIR 传感器 GPIO 读取 | 未完成 |  |
| 门磁 GPIO 读取 | 未完成 |  |
| DHT 温湿度读取 | 未完成 |  |
| MPU6050 I2C 通信 | 未完成 |  |
| MPU6050 震动数据读取 | 未完成 |  |
