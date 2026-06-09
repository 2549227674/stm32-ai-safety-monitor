# OPi5 I2C 设备测试

## 背景

验证 I2C 总线上的外设可用性。

## I2C 总线配置

| 总线 | Pin | Overlay | 设备 |
|---|---|---|---|
| I2C5 | pin 3/5 | i2c5-m3 | OLED SSD1306 (0x3C), MPU6050 (0x68) |

## 测试 1：OLED SSD1306 (0x3C)

**接线：** VCC→3.3V, GND→GND, SDA→pin 3, SCL→pin 5

**命令：**
```bash
sudo i2cdetect -y 5
```

**结果：**
```
30: -- -- -- -- -- -- -- -- -- -- -- -- 3c -- -- --
```

**结论：已通过**

## 测试 2：MPU6050/6500 (0x68)

**接线：** VCC→3.3V, GND→GND, SDA→pin 3, SCL→pin 5（与 OLED 共用 I2C5）

**命令：**
```bash
sudo i2cdetect -y 5
sudo i2cget -y 5 0x68 0x75  # WHO_AM_I
```

**结果：**
- 地址：0x68
- WHO_AM_I：0x70（MPU6500/MPU9250 兼容）
- 加速度计数据可读

**结论：已通过**

## 测试 3：PCA9685 (0x40)

**接线：** VCC→3.3V, GND→GND, SDA→pin 3/16, SCL→pin 5/18

**结果：**
- 4 个不同模块均未 ACK
- OLED 同线正常，排除 OPi5 I2C 硬件问题
- 确认为 PCA9685 模块批次问题

**结论：未通过（模块不可用）**

## 是否可写入 CANONICAL

- OLED 0x3C：可写入已验证
- MPU6050 0x68：可写入已验证
- PCA9685 0x40：不可写入（模块不可用，需更换）
