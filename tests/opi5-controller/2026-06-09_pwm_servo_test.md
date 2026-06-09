# OPi5 PWM 舵机测试

## 背景

PCA9685 不可用，改用 OPi5 硬件 PWM 直控舵机。

## 引脚配置

| 功能 | Pin | PWM 控制器 | Overlay |
|---|---|---|---|
| Pan 舵机 | pin 7 | pwmchip4 (febf0030.pwm) | pwm15-m2 |

## 接线

```
舵机信号线 → OPi5 pin 7 (PWM15)
舵机红线   → 外部 5V 正极
舵机黑/棕线 → 外部 5V 负极
外部电源 GND → OPi5 pin 6 (GND)
```

## 命令

```bash
echo 0 > /sys/class/pwm/pwmchip4/export
echo 20000000 > /sys/class/pwm/pwmchip4/pwm0/period  # 20ms, 50Hz
echo 1500000 > /sys/class/pwm/pwmchip4/pwm0/duty_cycle  # 1.5ms center
echo 1 > /sys/class/pwm/pwmchip4/pwm0/enable
```

## 测试结果

| Duty (us) | 观察 |
|---|---|
| 500 | 一端 |
| 1000 | 中间偏左 |
| 1500 | 中间 |
| 2000 | 中间偏右 |
| 2500 | 另一端 |

- 总扫描幅度：约 80-90 度
- 舵机型号：270 度舵机（实际有效范围 80-90 度）
- 电源：5V（电流需 ≥1A）
- 无抖动、无发热

## 结论

**已通过**（80-90 度有效范围，满足巡检演示需求）

## 注意事项

- 舵机必须外部供电，不能从 OPi5 排针取电
- 外部电源 GND 必须与 OPi5 GND 共地
- 脉宽范围 500us-2500us，超出可能卡死
- 程序退出时需 disable PWM 或回中位

## 是否可写入 CANONICAL

可写入：OPi5 PWM15 (pin 7) 直控舵机已验证。
