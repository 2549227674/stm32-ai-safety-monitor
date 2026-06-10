# i.MX6ULL Pro 引脚分配（历史归档）

> 本文件为历史 i.MX6ULL 阶段的引脚分配记录。当前主线使用 OPi5 引脚，见 `docs/03_硬件系统设计与供电安全_OPi5版.md`。

## J5 GPIO 引脚

| 信号 | GPIO line | J5 引脚 | 有效电平 | 说明 |
|---|---|---|---|---|
| PIR | gpio117 | D0 | active HIGH | 人体红外 |
| door | gpio118 | D1 | 高有效 | 门磁 |
| flame | gpio119 | D2 | active HIGH | 火焰 DO |
| mq2 | gpio120 | D3 | active HIGH | MQ-2 DO |
| RGB-R | gpio121 | D4 | active HIGH | 红灯 |
| buzzer | gpio122 | D5 | active LOW | 蜂鸣器 |
| RGB-G | gpio123 | D6 | active HIGH | 绿灯 |
| RGB-B | gpio124 | D7 | active HIGH | 蓝灯 |

## PCA9685 I2C PWM

| 通道 | 用途 | 说明 |
|---|---|---|
| CH0 | Pan 舵机 | 水平扫描 |
| CH1 | Tilt 舵机 | 垂直扫描 |
| CH2 | RGB-R | PWM 视觉增强 |
| CH3 | RGB-G | PWM 视觉增强 |
| CH4 | RGB-B | PWM 视觉增强 |
| CH5 | 继电器 | P1 扩展 |
| CH6 | 水泵 | P1 扩展 |

## 供电

- i.MX6ULL Pro: 5V/2A 独立供电
- PCA9685 + 舵机: 5V~6V/3A 独立供电
- 负载: 独立 5V
- GND: 星形共地

## 验证状态

所有引脚已通过 Task10/Task11 验证。详见 `tests/imx6ull/` 目录下的测试记录。
