# 2026-06-06 Task03-A GPIO 输入验证记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前基线提交：`be1cdc5`
- 当前阶段：Task03-A GPIO 输入验证

## 2. 结论

- Task03-A GPIO 输入验证已通过：`gpio117` 能稳定读到 PIR / HC-SR501 DO 的 0/1 变化。
- 默认设备树已恢复为 `100ask_imx6ull-14x14.dtb`；原 `imx6ull-100ask-custom.dtb` 仅保留备用。
- 板端未安装 `gpiodetect`、`gpioinfo`、`gpioget`，本轮使用 `/sys/class/gpio` fallback。
- 已新增并交叉编译 `gpio_test`，程序只做输入读取，不做输出驱动。
- 已部署到 i.MX6ULL：`/opt/edge-ai-safety-monitor/gpio_test`
- 裸门磁/裸按键因无 10k 上拉暂未测；10k 上拉仅作为裸门磁/裸按键方案的备用需求。
- 未验证 I2C/PCA9685/PWM/MG90/MOS。

## 3. 板端 GPIO 工具检查

命令：

```bash
ssh root@192.168.137.110 "which gpiodetect || true; which gpioinfo || true; which gpioget || true; ls -l /sys/class/gpio || true"
```

结果：

| 项目 | 状态 |
|---|---|
| `gpiodetect` | 缺失 |
| `gpioinfo` | 缺失 |
| `gpioget` | 缺失 |
| `/sys/class/gpio` | 存在 |

输出摘要：

```text
which: no gpiodetect in (/bin:/sbin:/usr/bin:/usr/sbin)
which: no gpioinfo in (/bin:/sbin:/usr/bin:/usr/sbin)
which: no gpioget in (/bin:/sbin:/usr/bin:/usr/sbin)
total 0
--w------- 1 root root 4096 Jan  1 00:26 export
lrwxrwxrwx 1 root root    0 Jan  1 00:26 gpio133 -> ../../devices/soc0/soc/2000000.aips-bus/20ac000.gpio/gpiochip4/gpio/gpio133
lrwxrwxrwx 1 root root    0 Jan  1 00:26 gpiochip0 -> ../../devices/soc0/soc/2000000.aips-bus/209c000.gpio/gpio/gpiochip0
lrwxrwxrwx 1 root root    0 Jan  1 00:26 gpiochip128 -> ../../devices/soc0/soc/2000000.aips-bus/20ac000.gpio/gpio/gpiochip128
lrwxrwxrwx 1 root root    0 Jan  1 00:26 gpiochip32 -> ../../devices/soc0/soc/2000000.aips-bus/20a0000.gpio/gpio/gpiochip32
lrwxrwxrwx 1 root root    0 Jan  1 00:26 gpiochip504 -> ../../devices/soc0/spi4/spi_master/spi32766/spi32766.0/gpio/gpiochip504
lrwxrwxrwx 1 root root    0 Jan  1 00:26 gpiochip64 -> ../../devices/soc0/soc/2000000.aips-bus/20a4000.gpio/gpio/gpiochip64
lrwxrwxrwx 1 root root    0 Jan  1 00:26 gpiochip96 -> ../../devices/soc0/soc/2000000.aips-bus/20a8000.gpio/gpio/gpiochip96
--w------- 1 root root 4096 Jan  1 00:26 unexport
```

`gpiodetect` 输出：未运行，工具缺失。

`gpioinfo` 输出：未运行，工具缺失。

## 4. gpio_test.c 构建结果

命令：

```bash
scripts/build_imx6ull.sh gpio_test
```

输出摘要：

```text
[build] CC = /home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc
arm-buildroot-linux-gnueabihf-gcc.br_real (Buildroot 2020.02-gee85cab) 7.5.0
[build] 产物: build/imx6ull/gpio_test
```

兼容性复测：

```bash
scripts/build_imx6ull.sh hello
```

输出摘要：

```text
[build] 产物: build/imx6ull/hello_imx6ull
```

## 5. file 输出

命令：

```bash
file build/imx6ull/gpio_test
```

输出：

```text
build/imx6ull/gpio_test: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 4.9.0, not stripped
```

## 6. 部署结果

命令：

```bash
scripts/deploy_imx6ull.sh build/imx6ull/gpio_test
ssh root@192.168.137.110 "ls -l /opt/edge-ai-safety-monitor/gpio_test"
```

输出摘要：

```text
[deploy] 已推送 build/imx6ull/gpio_test -> root@192.168.137.110:/opt/edge-ai-safety-monitor/
-rwx------ 1 root root 13740 Jan  1 01:07 /opt/edge-ai-safety-monitor/gpio_test
```

部署路径：

```text
/opt/edge-ai-safety-monitor/gpio_test
```

## 7. GPIO 输入实测

### 7.1 设备树与板端基线

- U-Boot `fdt_file`：`100ask_imx6ull-14x14.dtb`
- 板端 model：`Freescale i.MX6 ULL 14x14 EVK Board`
- 备用 custom dtb：`imx6ull-100ask-custom.dtb`

### 7.2 输入源与接线

| 项目 | 记录 |
|---|---|
| 输入源 | PIR / HC-SR501 |
| PIR VCC | J5 pin 1 `VDD_5V` |
| PIR GND | J5 pin 2 `GND` |
| PIR OUT/DO | J5 pin 15 `D0` / `CSI_DATA0` / `gpio117` |
| 空闲值 | `0` |
| 触发值 | `1` |
| 是否稳定 | 是，能连续读到 0/1 变化 |

安全说明：J5 pin 1 是 5V 电源脚，只用于 PIR 模块 VCC；不得把 J5 pin 1 或任何 5V 直接接入 GPIO。

### 7.3 测试命令

```bash
ssh root@192.168.137.110
cd /opt/edge-ai-safety-monitor
./gpio_test --sysfs 117 --watch
```

### 7.4 实测输出摘要

```text
seq=0..11 gpio=117 value=0
seq=12..17 gpio=117 value=1
seq=18..39 gpio=117 value=0
seq=40..45 gpio=117 value=1
seq=46..56 gpio=117 value=0
```

### 7.5 结论

- `gpio117` 能稳定读到 PIR DO 的 0/1 变化。
- Task03-A GPIO 输入验证通过。
- 裸门磁/裸按键因无 10k 上拉暂未测，但 PIR DO 已完成 GPIO 输入验证。
- 不再要求 10k 上拉；10k 上拉仅作为裸门磁/裸按键方案的备用需求。

## 8. 本轮边界

本记录当前只完成 GPIO 输入验证；未验证 I2C/PCA9685/PWM/MG90/MOS。
