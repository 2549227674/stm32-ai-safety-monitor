# OPi5 GPIO 输入/输出测试

## 背景

OPi5 临时主控迁移，验证 GPIO 输入（传感器）和输出（执行器）可用。

## 引脚分配

| Pin | GPIO | 用途 | 方向 |
|---|---|---|---|
| 3 | 47 | I2C5 SDA (OLED 0x3C + MPU6050 0x68) | I2C |
| 5 | 46 | I2C5 SCL (OLED 0x3C + MPU6050 0x68) | I2C |
| 7 | 54 | Pan 舵机 (PWM15) | PWM |
| 11 | 138 | 水枪 MOS | output |
| 13 | 139 | PIR | input |
| 15 | 28 | 火焰 flame | input |
| 19 | 49 | MQ-2 | input |
| 21 | 48 | RGB R | output |
| 23 | 50 | RGB G | output |
| 24 | 52 | RGB B | output |
| 26 | 35 | 蜂鸣器 buzzer | output |

## 测试 1：火焰传感器 (GPIO28, pin 15)

**接线：** VCC→3.3V, GND→GND, DO→pin 15

**命令：**
```bash
echo 28 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio28/direction
cat /sys/class/gpio/gpio28/value
```

**结果：**
- 空闲：1 (HIGH)
- 打火机触发：0 (LOW)
- **active LOW**

**结论：已通过**

## 测试 2：PIR 传感器 (GPIO139, pin 13)

**接线：** VCC→3.3V, GND→GND, DO→pin 13

**命令：**
```bash
echo 139 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio139/direction
cat /sys/class/gpio/gpio139/value
```

**结果：**
- 空闲：0 (LOW)
- 有运动：1 (HIGH)
- **active HIGH**

**结论：已通过**

## 测试 3：MQ-2 烟雾传感器 (GPIO49, pin 19)

**接线：** VCC→3.3V (灵敏度调低), GND→GND, DO→pin 19

**命令：**
```bash
echo 49 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio49/direction
cat /sys/class/gpio/gpio49/value
```

**结果：**
- 空闲：1 (HIGH)
- 烟雾触发：0 (LOW)
- **active LOW**（3.3V 供电，灵敏度需手动调节）

**结论：已通过**

## 测试 4：蜂鸣器 (GPIO35, pin 26)

**接线：** VCC→外部5V, GND→GND, I/O→pin 26

**命令：**
```bash
echo 35 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio35/direction
echo 0 > /sys/class/gpio/gpio35/value  # 响
echo 1 > /sys/class/gpio/gpio35/value  # 停
```

**结果：**
- GPIO=0 → 响
- GPIO=1 → 停
- **active LOW**

**结论：已通过**

## 测试 5：RGB LED (GPIO48/50/52, pin 21/23/24)

**接线：** 共阴 RGB，R→220Ω→pin 21, G→220Ω→pin 23, B→220Ω→pin 24, COM→GND

**命令：**
```bash
for pin in 48 50 52; do
  echo $pin > /sys/class/gpio/export
  echo out > /sys/class/gpio/gpio$pin/direction
  echo 0 > /sys/class/gpio/gpio$pin/value
done
echo 1 > /sys/class/gpio/gpio48/value  # 红
echo 1 > /sys/class/gpio/gpio50/value  # 绿
echo 1 > /sys/class/gpio/gpio52/value  # 蓝
```

**结果：**
- 默认输入模式会亮白色（内部上拉），需设为输出 LOW 关闭
- GPIO=1 亮，GPIO=0 灭
- 红/绿/蓝单独可控

**结论：已通过**

## 测试 6：水枪 MOS (GPIO138, pin 11)

**接线：** MOS VIN+→外部5V, VIN-→GND, GND→OPi5 GND, I/O→pin 11, VOUT+→水枪红线, VOUT-→水枪黑线

**命令：**
```bash
echo 138 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio138/direction
echo 1 > /sys/class/gpio/gpio138/value  # 喷水
echo 0 > /sys/class/gpio/gpio138/value  # 停止
```

**结果：**
- GPIO=1 → MOS 导通 → 水枪喷水
- GPIO=0 → MOS 截止 → 水枪停止

**结论：已通过**

## 是否可写入 CANONICAL

可写入：所有 GPIO 输入/输出测试通过。
