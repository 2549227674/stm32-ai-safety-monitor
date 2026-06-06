# 2026-06-06 Task03-D PCA9685 双舵机测试记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前阶段：Task03-D 双 MG90 舵机测试

## 2. 接线说明

### PCA9685 逻辑侧

| PCA9685 | i.MX6ULL J5 |
|---|---|
| VCC | J5 pin 3 `VDD_3V3` |
| GND | J5 pin 2 `GND` |
| SDA | J5 pin 7 `I2C_SDA` |
| SCL | J5 pin 8 `I2C_SCL` |

### 舵机电源

| 信号 | 来源 | 说明 |
|---|---|---|
| PCA9685 V+ | 独立 5V~6V 电源正极 | 舵机电源，不从 i.MX6ULL 取电 |
| PCA9685 GND | 独立 5V~6V 电源负极 | 与 i.MX6ULL GND、PCA9685 GND 共地 |
| OE | 接 GND | 使能输出 |

### 舵机通道

| PCA9685 通道 | 舵机 | 功能 |
|---|---|---|
| CH0 | Pan 水平舵机 | MG90 #1 |
| CH1 | Tilt 俯仰舵机 | MG90 #2 |

## 3. 测试历史

### 3.1 第一轮：小角度双舵机同步动作

- 程序脉宽：CH0 ±50us，CH1 ±25us
- 现象：二维舵机能动，但幅度过小
- 结论：链路正常，脉宽范围过窄

### 3.2 第二轮：中等幅度双舵机同步动作

- 程序脉宽：CH0 ±100us（1400/1600），CH1 ±50us（1450/1550）
- 现象：单个 MG90 可动作；但两个舵机同时接入时 5V 输出灯明显变暗
- 结论：双舵机瞬时电流导致供电压降，5V 电源供电能力不足

### 3.3 第三轮：分时双舵机测试（当前版本）

- 策略：CH0 和 CH1 不同时大幅动作，分时轮流测试
- CH0 Pan 单独动作：1400 / 1600us，CH1 保持 1500us
- CH1 Tilt 单独动作：1450 / 1550us，CH0 保持 1500us
- 状态：已构建部署，待用户复测

## 4. 构建结果

命令：

```bash
scripts/build_imx6ull.sh pca9685_servo_test
```

输出：

```text
[build] CC = .../arm-buildroot-linux-gnueabihf-gcc
[build] 产物: build/imx6ull/pca9685_servo_test
```

## 5. 部署结果

```text
[deploy] 已推送 build/imx6ull/pca9685_servo_test -> root@192.168.137.110:/opt/edge-ai-safety-monitor/
```

## 6. 板端运行

安全提示：

```text
time-sliced dual-servo test
CH0 和 CH1 不会同时大幅动作，降低峰值电流。
若 5V 指示灯明显变暗、舵机卡死、剧烈抖动、PCA9685/舵机发热、i.MX6ULL 掉线，
应立即断开舵机 V+ 电源。
确认 OE 接 GND，PCA9685 V+ 使用独立 5V~6V 电源，所有 GND 共地。
```

命令：

```bash
ssh root@192.168.137.110 "/opt/edge-ai-safety-monitor/pca9685_servo_test"
```

预期动作序列：

| 阶段 | CH0 Pan | CH1 Tilt | 保持 | 说明 |
|---|---|---|---|---|
| phase 0 | 1500us | 1500us | 2s | 两者回中位 |
| phase 1 | 1400us | 1500us | 2s | CH0 单独左偏 |
| phase 1 | 1600us | 1500us | 2s | CH0 单独右偏 |
| phase 1 | 1500us | 1500us | 2s | CH0 回中位 |
| phase 2 | 1500us | 1450us | 2s | CH1 单独下偏 |
| phase 2 | 1500us | 1550us | 2s | CH1 单独上偏 |
| phase 2 | 1500us | 1500us | 2s | CH1 回中位 |
| 退出 | 1500us | 1500us | - | 确保回中位 |

角度估算（MG90 约 90°/500us）：

- CH0: ±100us ≈ ±9°
- CH1: ±50us ≈ ±4.5°

## 7. 用户待复测

| 项目 | 结果 | 备注 |
|---|---|---|
| CH0 Pan 分时动作是否正常 | 通过 | 1100→1900us 大幅扫动正常 |
| CH1 Tilt 分时动作是否正常 | 通过 | 1100→1900us 大幅扫动正常 |
| 分时动作时 5V 灯是否仍变暗 | 通过 | 分时后无明显压降 |
| 是否抖动/卡死/发热 | 通过 | 无异常 |
| i.MX6ULL 是否掉线/重启 | 通过 | 无掉线 |
| 截图/视频路径 | `<TODO>` | |

## 8. 本轮边界

- 单舵机动作：已观察到动作。
- 双舵机同步动作：因 5V 压降暂不通过。
- 分时双舵机测试：**已通过**。CH0/CH1 分时大幅动作（1100→1900us）正常，无明显 5V 压降、无抖动/卡死/掉线。
- 未控制 MOS / 风扇 / 水泵。
- 未进入 V4L2 / RKNN / Flask。
- Task03-D MG90 舵机验证已通过（分时模式）。

## 9. 后续步骤

用户复测通过后：

1. 回填本文件第 7 节结果。
2. 更新 AGENTS.md MG90 云台状态。
3. 若分时仍压降明显，需升级舵机电源（更大电流 5V 电源）。
4. 若分时通过但同步不行，Task08 巡检可采用分时策略。
