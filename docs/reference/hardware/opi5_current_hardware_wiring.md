# OPi5 当前硬件接线参考

> 最后更新：2026-06-10
> 权威来源：CANONICAL_DECISIONS.md 0.8.1 + 板端实测

## 概述

当前演示主线使用 Orange Pi 5 (RK3588S) 26pin GPIO 接口连接所有外设。
不使用 PCA9685（4 个模块均未 ACK），所有执行器改用 GPIO/PWM 直控。

## OPi5 26pin 接线表

| 逻辑信号 | OPi5 物理脚 | GPIO 号 | 方向 | 电平/协议 | 备注 |
|---|---|---|---|---|---|
| I2C1 SDA | pin 16 | 59 | I2C | 3.3V | OLED 0x3C + MPU6050 0x68 |
| I2C1 SCL | pin 18 | 58 | I2C | 3.3V | OLED 0x3C + MPU6050 0x68 |
| Pan servo | pin 7 | 54 (PWM15) | PWM | 50Hz, 80-90° | pwmchip4 |
| Water MOS | pin 11 | 138 | output | active HIGH | 水泵/水枪 |
| PIR | pin 13 | 139 | input | active HIGH | HC-SR501 |
| Flame | pin 15 | 28 | input | active LOW | 火焰传感器 |
| MQ-2 | pin 19 | 49 | input | active LOW | 烟雾/气体传感器 |
| RGB R | pin 21 | 48 | output | active HIGH | |
| RGB G | pin 23 | 50 | output | active HIGH | |
| RGB B | pin 24 | 52 | output | active HIGH | |
| Buzzer | pin 26 | 35 | output | active LOW | NPN 三极管驱动 |

## I2C 设备

| 设备 | I2C 地址 | 总线 | 状态 |
|---|---|---|---|
| OLED SSD1306 | 0x3C | I2C1 | 已验证 |
| MPU6050/GY521 | 0x68 | I2C1 | 已验证 |

## 供电

- OPi5 通过 USB-C 供电
- 舵机独立 5V 供电
- 负载独立 5V 供电
- 所有 GND 星形共地

## 不再使用的外设

以下外设属于 i.MX6ULL 阶段，不进入当前 OPi5 接线：

- PCA9685（模块问题，改用 GPIO/PWM 直控）
- Tilt 舵机（单轴 Pan only）
- 继电器 KY-019
- 风扇
- P1 水泵（PCA9685 CH6 驱动）

## USB 设备

- USB 摄像头：OPi5 USB 口
