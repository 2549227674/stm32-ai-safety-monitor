# OPi5 临时主控 — 引脚映射（2026-06-09 实测确认）

> PCA9685 不可用，改用 OPi5 GPIO/PWM 直控所有执行器。

## 最终引脚分配

| 逻辑信号 | OPi5 物理脚 | GPIO 号 | 方向 | 备注 |
|---|---|---|---|---|
| I2C5 SDA | pin 3 | 47 | I2C | OLED 0x3C + MPU6050 0x68 |
| I2C5 SCL | pin 5 | 46 | I2C | OLED 0x3C + MPU6050 0x68 |
| Pan 舵机 | pin 7 | 54 (PWM15) | PWM | pwmchip4, 80-90 度有效范围 |
| PIR | pin 13 | 139 | input | active HIGH，已验证 |
| 火焰 flame | pin 15 | 28 | input | active LOW，已验证 |
| MQ-2 | pin 19 | 49 | input | active LOW，3.3V+调灵敏度，已验证 |
| RGB R | pin 21 | 48 | output | active HIGH，已验证 |
| RGB G | pin 23 | 50 | output | active HIGH，已验证 |
| RGB B | pin 24 | 52 | output | active HIGH，已验证 |
| 蜂鸣器 buzzer | pin 26 | 35 | output | active LOW，已验证 |
| 水枪 MOS | pin 11 | 138 | output | active HIGH，MOS 模块，已验证 |

## I2C overlay 配置

`/boot/orangepiEnv.txt`:
```
overlays=i2c5-m3 i2c1-m4 pwm15-m2 pwm1-m1
```

## 空闲引脚

| Pin | GPIO | 备注 |
|---|---|---|
| 8 | 131 | RXD.0，可复用为 GPIO |
| 10 | 132 | TXD.0，可复用为 GPIO |
| 16 | 59 | I2C1 SDA（已启用但未使用） |
| 18 | 58 | I2C1 SCL（已启用但未使用） |

## 执行器控制方式

| 执行器 | 控制方式 | 引脚 | 说明 |
|---|---|---|---|
| 舵机 | OPi5 硬件 PWM | pin 7 | pwmchip4, 50Hz |
| RGB | GPIO 直控 | pin 21/23/24 | 共阴 LED，220Ω 限流 |
| 蜂鸣器 | GPIO 直控 | pin 26 | active LOW，NPN 驱动 |
| 水枪 | GPIO + MOS | pin 11 | MOS 模块隔离 |

## 安全要求

- 舵机/水枪必须外部供电，不从 OPi5 排针取电
- 外部电源 GND 必须与 OPi5 GND 共地
- 程序退出时必须 all_off（GPIO 输出 LOW，PWM disable）
