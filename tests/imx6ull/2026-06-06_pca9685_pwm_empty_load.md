# 2026-06-06 Task03-C PCA9685 空载 PWM 波形验证准备记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前基线提交：`89b0bdc`
- 当前阶段：Task03-C PCA9685 空载 PWM 波形验证准备

## 2. 接线说明

PCA9685 只接逻辑侧：

| PCA9685 | i.MX6ULL J5 |
|---|---|
| VCC | J5 pin 3 `VDD_3V3` |
| GND | J5 pin 2 `GND` |
| SDA | J5 pin 7 `I2C_SDA` |
| SCL | J5 pin 8 `I2C_SCL` |

逻辑分析仪：

| 逻辑分析仪 | PCA9685 |
|---|---|
| GND | GND |
| CH0 | PWM0 |

## 3. 禁止事项确认

- 未接 PCA9685 `V+`。
- 未接 MG90 舵机。
- 未接 MOS / 风扇 / 水泵。
- 本轮只让 PCA9685 channel 0 输出空载 PWM。
- 程序不访问 GPIO 输出，不控制 MOS，不控制舵机。

## 4. 构建结果

命令：

```bash
scripts/build_imx6ull.sh pca9685_pwm_test
file build/imx6ull/pca9685_pwm_test
```

输出摘要：

```text
[build] CC = /home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc
arm-buildroot-linux-gnueabihf-gcc.br_real (Buildroot 2020.02-gee85cab) 7.5.0
[build] 产物: build/imx6ull/pca9685_pwm_test
build/imx6ull/pca9685_pwm_test: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 4.9.0, not stripped
```

兼容性复测：

```bash
scripts/build_imx6ull.sh hello
scripts/build_imx6ull.sh gpio_test
```

输出摘要：

```text
[build] 产物: build/imx6ull/hello_imx6ull
[build] 产物: build/imx6ull/gpio_test
```

## 5. 部署结果

命令：

```bash
scripts/deploy_imx6ull.sh build/imx6ull/pca9685_pwm_test
ssh root@192.168.137.110 "ls -l /opt/edge-ai-safety-monitor/pca9685_pwm_test"
```

输出摘要：

```text
[deploy] 已推送 build/imx6ull/pca9685_pwm_test -> root@192.168.137.110:/opt/edge-ai-safety-monitor/
-rwx------ 1 root root 13432 Jan  1 01:04 /opt/edge-ai-safety-monitor/pca9685_pwm_test
```

部署路径：

```text
/opt/edge-ai-safety-monitor/pca9685_pwm_test
```

## 6. 板端运行

运行前安全提示：

```text
SAFETY CHECK: do not connect PCA9685 V+; do not connect MG90; connect only logic analyzer GND and CH0->PWM0.
```

命令：

```bash
ssh root@192.168.137.110 "/opt/edge-ai-safety-monitor/pca9685_pwm_test"
```

输出：

```text
空载 PWM 测试：不要接 MG90，不要接 PCA9685 V+。
Only PCA9685 channel 0 will output empty-load PWM.
Do not connect servo, MOS, fan, or pump.
Using /dev/i2c-0 addr=0x40
Configured PCA9685 for 50.0 Hz PWM, prescale=121
cycle=1
channel=0 pulse_us=1000 ticks=205
channel=0 pulse_us=1500 ticks=307
channel=0 pulse_us=2000 ticks=410
cycle=2
channel=0 pulse_us=1000 ticks=205
channel=0 pulse_us=1500 ticks=307
channel=0 pulse_us=2000 ticks=410
cycle=3
channel=0 pulse_us=1000 ticks=205
channel=0 pulse_us=1500 ticks=307
channel=0 pulse_us=2000 ticks=410
empty-load PWM test complete
```

## 7. 逻辑分析仪待用户补充

| 项目 | 结果 |
|---|---|
| PWM0 是否有波形 | TODO |
| 频率是否约 50Hz | TODO |
| 1.0ms 配置实测脉宽 | TODO |
| 1.5ms 配置实测脉宽 | TODO |
| 2.0ms 配置实测脉宽 | TODO |
| 截图路径 | TODO |

只有用户补充逻辑分析仪频率、脉宽和截图后，才能把 Task03-C 写成已通过。

## 8. 本轮边界

本轮只做空载 PWM；未接 PCA9685 `V+`；未接 MG90；未验证舵机转动。

当前状态：Task03-C 软件准备完成，空载 PWM 波形待用户用逻辑分析仪确认。
