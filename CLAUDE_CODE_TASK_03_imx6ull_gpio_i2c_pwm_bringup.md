# Task 03：i.MX6ULL GPIO / I2C / PWM / MOS Bring-up

## 目标 / 范围

验证 i.MX6ULL 的数字输入、I2C、PCA9685 PWM、MG90S 舵机和 MOS 低压负载。本任务只做单模块，不做端边 AI 联调。

## 前置条件

Task02 已通过。确认板卡供电稳定，负载先不接或接低风险负载。

## 操作步骤

1. 使用 `gpiodetect/gpioinfo` 盘点 GPIO。
2. 选一路输入做 `gpioget` 验证。
3. 选一路输出做 LED/空载验证。
4. 使用 `i2cdetect` 查找 PCA9685。
5. 写 PCA9685 PWM 测试。
6. 逻辑分析仪确认 PWM。
7. 单舵机测试，再二维云台。
8. MOS 先 LED/小风扇，再考虑水泵。

## 命令骨架

```bash
ssh <IMX_USER_TODO_FILL>@<IMX_IP_TODO_FILL>
gpiodetect
gpioinfo
gpioget <GPIOCHIP_TODO_FILL> <LINE_TODO_FILL>
i2cdetect -y <I2C_BUS_TODO_FILL>
# 编译并运行 pca9685_test / mos_output_test，具体命令由实现补充
```

## 产出物

- `edge/imx6ull-controller/src/gpio_test.c`
- `edge/imx6ull-controller/src/pca9685_test.c`
- `edge/imx6ull-controller/src/mos_output_test.c`
- `tests/imx6ull/YYYY-MM-DD_gpio_i2c_pwm_mos.md`

## 验收标准

- [x] GPIO 输入能读到变化。已完成 Task03-A：PIR / HC-SR501 DO -> J5 D0 / `CSI_DATA0` / `gpio117`，`gpio_test --sysfs 117 --watch` 可读到 0/1 变化。
- [x] I2C 能看到目标地址。已完成 Task03-B：PCA9685 逻辑侧接 J5 I2C，`/dev/i2c-0` 上 `0x40` ACK；未输出 PWM。
- [ ] PWM 波形可用逻辑分析仪观察。Task03-C 软件准备已完成，`pca9685_pwm_test` 已运行；用户决定跳过波形实测，因此不标记 PWM 波形已通过。
- [x] MG90S 能安全转动。Task03-D 分时双舵机测试已通过：CH0/CH1 各自 1100→1900us 大幅动作正常，无 5V 压降、无抖动/卡死/掉线。后续 Task08 云台巡检优先采用分时策略。
- [ ] MOS 负载默认 OFF 且可控。

## 禁止事项

- 不直接接大电流负载。
- 不从板卡给舵机供电。
- 不跳过空载测试。
- 不接 220V。

## 完成后回填

- 回填 docs/03、docs/11、AGENTS.md。
- 保存波形截图路径。
- 更新 DEVPLAN P2。
