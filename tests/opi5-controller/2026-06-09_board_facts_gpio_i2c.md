# OPi5 板端事实采集

## 背景

OPi5 临时主控迁移前的硬件盘点。

## 系统信息

```
Linux orangepi5 6.1.99-rockchip-rk3588 #1.2.2 SMP Thu Jun 26 14:42:53 CST 2025 aarch64
PRETTY_NAME="Orange Pi 1.2.2 Jammy"
Ubuntu 22.04.5 LTS (Jammy Jellyfish)
```

## 排针

26-pin 排针，`gpio readall` 输出：

```
 +------+-----+----------+--------+---+   OPI5   +---+--------+----------+-----+------+
 | GPIO | wPi |   Name   |  Mode  | V | Physical | V |  Mode  | Name     | wPi | GPIO |
 +------+-----+----------+--------+---+----++----+---+--------+----------+-----+------+
 |      |     |     3.3V |        |   |  1 || 2  |   |        | 5V       |     |      |
 |   47 |   0 |    SDA.5 |   ALT9 | 1 |  3 || 4  |   |        | 5V       |     |      |
 |   46 |   1 |    SCL.5 |   ALT9 | 1 |  5 || 6  |   |        | GND      |     |      |
 |   54 |   2 |    PWM15 |     IN | 1 |  7 || 8  | 0 | IN     | RXD.0    | 3   | 131  |
 |      |     |      GND |        |   |  9 || 10 | 0 | IN     | TXD.0    | 4   | 132  |
 |  138 |   5 |  CAN1_RX |     IN | 1 | 11 || 12 | 1 | IN     | CAN2_TX  | 6   | 29   |
 |  139 |   7 |  CAN1_TX |     IN | 1 | 13 || 14 |   |        | GND      |     |      |
 |   28 |   8 |  CAN2_RX |   ALT9 | 1 | 15 || 16 | 1 | IN     | SDA.1    | 9   | 59   |
 |      |     |     3.3V |        |   | 17 || 18 | 1 | IN     | SCL.1    | 10  | 58   |
 |   49 |  11 | SPI4_TXD |     IN | 1 | 19 || 20 |   |        | GND      |     |      |
 |   48 |  12 | SPI4_RXD |     IN | 1 | 21 || 22 | 1 | IN     | GPIO2_D4 | 13  | 92   |
 |   50 |  14 | SPI4_CLK |     IN | 1 | 23 || 24 | 1 | IN     | SPI4_CS1 | 15  | 52   |
 |      |     |      GND |        |   | 25 || 26 | 1 | IN     | PWM1     | 16  | 35   |
 +------+-----+----------+--------+---+----++----+---+--------+----------+-----+------+
```

## GPIO 映射（实测）

| 物理脚 | GPIO 号 | 名称 | Mode | 用途 |
|---|---|---|---|---|
| pin 3 | 47 | SDA.5 | ALT9 | I2C5 SDA |
| pin 5 | 46 | SCL.5 | ALT9 | I2C5 SCL |
| pin 7 | 54 | PWM15 | IN | MQ-2 DO [待验证] |
| pin 11 | 138 | CAN1_RX | IN | door [待验证] |
| pin 13 | 139 | CAN1_TX | IN | PIR [待验证] |
| pin 15 | 28 | CAN2_RX | IN | flame DO [待验证] |
| pin 16 | 59 | SDA.1 | IN | I2C1 SDA（备选） |
| pin 18 | 58 | SCL.1 | IN | I2C1 SCL（备选） |
| pin 26 | 35 | PWM1 | IN | buzzer [待验证] |

**注意**：GPIO 号与 RK3588S 公式 `x*32+y*8+z` 不完全一致（pin 7 公式=46 实测=54，pin 26 公式=11 实测=35）。必须以 `gpio readall` 实测为准。

## I2C 总线

```
/dev/i2c-0   fd880000  (内核设备: 0x42, 0x43)
/dev/i2c-1   feab0000  (I2C5 overlay 启用后出现，但引脚未配置为 ALT)
/dev/i2c-2   feaa0000  (内核设备: 0x42)
/dev/i2c-5   fead0000  (I2C5 overlay: i2c5-m3, status=okay)
/dev/i2c-6   fec80000  (I2C0: es8388@10, fusb302@22, hym8563@51)
/dev/i2c-7   fec90000  (I2C1)
/dev/i2c-9   fde80000  (HDMI DDC)
/dev/i2c-10  fde50000  (DP)
```

## I2C overlay 配置

`/boot/orangepiEnv.txt`:
```
overlays=i2c5-m3 i2c1-m2
```

**注意**：overlays 值不需要 `rk3588-` 前缀（boot.cmd 会自动加上）。

## PCA9685 检测结果

**未通过** — 3 个不同 PCA9685 模块均未在 i2c-5 或 i2c-1 上响应。
但 OLED SSD1306 (0x3C) 在 I2C1 (pin 16/18) 上正常响应，证明 OPi5 I2C 硬件正常。
详见 `tests/opi5-controller/2026-06-09_i2c_pca9685_detect.md`。

## 依赖

```
build-essential, i2c-tools, gpiod, libgpiod-dev, v4l-utils, curl, git
wiringOP (next 分支) 已安装
```

## 最终引脚分配（2026-06-09 实测）

| Pin | GPIO | 用途 | 状态 |
|---|---|---|---|
| 3 | 47 | I2C5 SDA (OLED 0x3C + MPU6050 0x68) | 已验证 |
| 5 | 46 | I2C5 SCL (OLED 0x3C + MPU6050 0x68) | 已验证 |
| 7 | 54 | Pan 舵机 (PWM15, pwmchip4) | 已验证 |
| 11 | 138 | 水枪 MOS | 已验证 |
| 13 | 139 | PIR (active HIGH) | 已验证 |
| 15 | 28 | 火焰 flame (active LOW) | 已验证 |
| 19 | 49 | MQ-2 (active LOW, 3.3V) | 已验证 |
| 21 | 48 | RGB R | 已验证 |
| 23 | 50 | RGB G | 已验证 |
| 24 | 52 | RGB B | 已验证 |
| 26 | 35 | 蜂鸣器 buzzer (active LOW) | 已验证 |

## I2C overlay 配置

`/boot/orangepiEnv.txt`:
```
overlays=i2c5-m3 i2c1-m4 pwm15-m2 pwm1-m1
```

## PCA9685 检测结果

**未通过** — 4 个不同 PCA9685 模块均未响应。
OLED SSD1306 同线正常，确认为模块问题。
详见 `tests/opi5-controller/2026-06-09_i2c_pca9685_detect.md`。
