# OPi5 引脚映射（当前主线）

> 权威来源：CANONICAL_DECISIONS.md 0.8.1 + 板端 `gpio readall` 实测

## GPIO 引脚映射

```
OPi5 26pin Header
+-----+-----+----------+------+---+----+-----+-----+
| BCM | wPi |   Name   | Mode | V | Phy | V | Mode |   Name   | wPi | BCM |
+-----+-----+----------+------+---+-----+---+------+----------+-----+-----+
|     |     |    3.3V  |      |   |   1 | 2 |      |    5V    |     |     |
|  47 |     | I2C5_SDA |      |   |   3 | 4 |      |    5V    |     |     |
|  46 |     | I2C5_SCL |      |   |   5 | 6 |      |   GND    |     |     |
|  54 |   7 | PWM15    | ALT4 | 0 |   7 | 8 |      |          |     |     |
|     |     |      GND |      |   |   9 | 10|      |          |     |     |
| 138 |   0 | GPIO138  |  OUT | 0 |  11 | 12|      |          |     |     |
| 139 |   2 | GPIO139  |   IN | 0 |  13 | 14|      |   GND    |     |     |
|  28 |   3 | GPIO28   |   IN | 0 |  15 | 16| ALT9 | I2C1_SDA |   9 |  59 |
|     |     |    3.3V  |      |   |  17 | 18| ALT9 | I2C1_SCL |  10 |  58 |
|  49 |  12 | GPIO49   |   IN | 0 |  19 | 20|      |   GND    |     |     |
|  48 |  13 | GPIO48   |  OUT | 0 |  21 | 22|      |          |     |     |
|  50 |  14 | GPIO50   |  OUT | 0 |  23 | 24|      |          |     |     |
|     |     |      GND |      |   |  25 | 26|  OUT | GPIO35   |  11 |  35 |
+-----+-----+----------+------+---+-----+---+------+----------+-----+-----+
```

## 信号说明

| Pin | GPIO | 信号 | 说明 |
|---|---|---|---|
| 3 | 47 | I2C5 SDA | 空闲（I2C5 总线不可用） |
| 5 | 46 | I2C5 SCL | 空闲（I2C5 总线不可用） |
| 7 | 54 | PWM15 | Pan servo, pwmchip4 |
| 11 | 138 | Water MOS | active HIGH, 水泵/水枪 |
| 13 | 139 | PIR | active HIGH |
| 15 | 28 | Flame | active LOW |
| 16 | 59 | I2C1 SDA | OLED + MPU6050 |
| 18 | 58 | I2C1 SCL | OLED + MPU6050 |
| 19 | 49 | MQ-2 | active LOW |
| 21 | 48 | RGB R | active HIGH |
| 23 | 50 | RGB G | active HIGH |
| 24 | 52 | RGB B | active HIGH |
| 26 | 35 | Buzzer | active LOW |
