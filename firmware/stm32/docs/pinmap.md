# STM32 Pin Map

本文档记录当前已验证和预留引脚。未实际验证的引脚必须标注为“预留”或“待验证”，避免后续误认为已经完成。

## 已验证 / 部分验证引脚

| 引脚 | 功能 | 状态 | 说明 |
|---|---|---|---|
| PA13 | SWDIO | 已验证 / 保留 | ST-Link 下载调试，必须保留 |
| PA14 | SWCLK | 已验证 / 保留 | ST-Link 下载调试，必须保留 |
| PC13 | 板载 LED | 已验证 | 低电平点亮，用于 P0 心跳闪烁 |
| PA0 | ADC1_IN0 | 已验证 | AD 四位按键模块，VCC 接 3.3V，GND 接 GND，AD 接 PA0 |
| PB5 | Buzzer | 已验证 | 有源蜂鸣器低电平触发：RESET 响，SET 不响 |
| PB0 | RGB_R | 已验证 | KY-016 RGB 红色通道 |
| PB1 | RGB_G | 已验证 | KY-016 RGB 绿色通道 |
| PB10 | RGB_B | 已验证 | KY-016 RGB 蓝色通道 |
| PB6 | I2C1_SCL / OLED | 已验证 | SSD1306 OLED，地址 0x3C |
| PB7 | I2C1_SDA / OLED | 已验证 | SSD1306 OLED，地址 0x3C |
| PB12 | FLAME_DO | 部分验证 | 3 针火焰传感器 DO；无火焰状态显示正常，真实触发待安全补测 |
| PB14 | MQ2_DO | 软件链路通过 | 模拟高低电平通过；MQ-2 实物 DO/AO 待 10k/20k 分压电阻 |
| PB15 | PIR_DO | 已验证 | HC-SR501 OUT；PB15 最终方案通过 |

## 预留 / 待验证引脚

| 引脚 | 计划功能 | 状态 | 说明 |
|---|---|---|---|
| PA9 | USART1_TX | 预留 / 待验证 | USB-TTL 未到货，P0-01 暂缓 |
| PA10 | USART1_RX | 预留 / 待验证 | USB-TTL 未到货，P0-01 暂缓 |
| PA2 | USART2_TX | 预留 | 后续 STM32 发给 ESP32-CAM |
| PA3 | USART2_RX | 预留 | 后续 ESP32-CAM 发给 STM32 |
| PA1 | ADC1_IN1 | 预留 / 待验证 | 后续 MQ-2 AO 可选输入；5V 供电时必须先分压 |
| PB13 | 暂不使用 | 原方案异常 | 原 PIR 输入链路不可靠，疑似排针/杜邦线/接触问题；不再作为 PIR 默认引脚 |
| PB8 | 风扇驱动输入 | 预留 / 待验证 | 必须经 MOS/驱动模块，不得 GPIO 直驱负载 |
| PB9 | 水泵驱动输入 | 预留 / 待验证 | 必须独立供电并经 MOS/驱动模块 |

## 电平与安全备注

| 项目 | 备注 |
|---|---|
| SWD | PA13/PA14 不得改作普通 GPIO |
| PC13 LED | 低电平点亮，驱动能力弱，只用于状态指示 |
| AD 按键 | 这是 AD 模拟按键模块，不是 PB12-PB15 四路独立按键 |
| 蜂鸣器 | 低电平触发，建议 `BUZZER_ON_LEVEL = GPIO_PIN_RESET`，`BUZZER_OFF_LEVEL = GPIO_PIN_SET`，PB5 初始电平 High |
| OLED | I2C1：PB6=SCL、PB7=SDA，地址 0x3C；已完成固定文本和 P0 调试页面 |
| MQ-2 | DO 使用 PB14，软件链路模拟通过；AO 后续 PA1/ADC1_IN1；5V 供电时 DO/AO 接 STM32 前必须分压 |
| PIR | HC-SR501 最终使用 PB15；PB13 原方案不可靠，暂不使用 |
| 面包板供电 | 黑色面包板电源模块可通过跳帽输出 3.3V/5V 分轨；两侧正极不得直接相连，所有 GND 必须与 STM32 GND 共地 |
| ST-Link 供电 | 只适合 STM32 + 少量小功耗模块 bring-up；若 STM32 已由 Type-C 供电，ST-Link 只接 SWDIO/SWCLK/GND，不接 ST-Link 3.3V |
| 12V | 12V 适配器/电池不得直接接 STM32、OLED、PIR、MQ-2 |
