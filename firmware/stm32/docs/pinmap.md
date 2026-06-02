# STM32 Pin Map

本文档记录当前已验证和预留引脚。未实际验证的引脚必须标注为“预留”或“待验证”，避免后续误认为已经完成。

## 已验证引脚

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

## 预留 / 待验证引脚

| 引脚 | 计划功能 | 状态 | 说明 |
|---|---|---|---|
| PA9 | USART1_TX | 预留 / 待验证 | USB-TTL 未到货，P0-01 暂缓 |
| PA10 | USART1_RX | 预留 / 待验证 | USB-TTL 未到货，P0-01 暂缓 |
| PA2 | USART2_TX | 预留 | 后续 STM32 发给 ESP32-CAM |
| PA3 | USART2_RX | 预留 | 后续 ESP32-CAM 发给 STM32 |
| PB6 | I2C1_SCL | 预留 / 待验证 | 后续 OLED / MPU6050 |
| PB7 | I2C1_SDA | 预留 / 待验证 | 后续 OLED / MPU6050 |
| PB12 | PIR 或其他 GPIO 输入 | 预留 / 待验证 | P0-04 传感器输入候选 |
| PB13 | 门磁或其他 GPIO 输入 | 预留 / 待验证 | P0-04 传感器输入候选 |
| PB8 | 风扇驱动输入 | 预留 / 待验证 | 必须经 MOS/驱动模块，不得 GPIO 直驱负载 |
| PB9 | 水泵驱动输入 | 预留 / 待验证 | 必须独立供电并经 MOS/驱动模块 |

## 电平与安全备注

| 项目 | 备注 |
|---|---|
| SWD | PA13/PA14 不得改作普通 GPIO |
| PC13 LED | 低电平点亮，驱动能力弱，只用于状态指示 |
| AD 按键 | 这是 AD 模拟按键模块，不是 PB12-PB15 四路独立按键 |
| 蜂鸣器 | 低电平触发，建议 `BUZZER_ON_LEVEL = GPIO_PIN_RESET`，`BUZZER_OFF_LEVEL = GPIO_PIN_SET`，PB5 初始电平 High |
| 面包板供电 | STM32 3.3V 接红色电源轨，GND 接蓝色电源轨，小模块从电源轨取电 |
| ST-Link 供电 | 只适合 STM32 + 小功耗模块 bring-up，不得给 ESP32-CAM、风扇、继电器、水泵供电 |
