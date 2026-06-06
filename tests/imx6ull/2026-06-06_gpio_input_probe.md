# 2026-06-06 Task03-A GPIO 输入验证准备记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前基线提交：`be1cdc5`
- 当前阶段：Task03-A GPIO 输入验证准备

## 2. 结论

- 本轮只完成 GPIO 输入验证的软件准备和盘点记录模板。
- 默认设备树已恢复为 `100ask_imx6ull-14x14.dtb`；原 `imx6ull-100ask-custom.dtb` 仅保留备用。
- 板端未安装 `gpiodetect`、`gpioinfo`、`gpioget`，本轮使用 `/sys/class/gpio` fallback。
- 已新增并交叉编译 `gpio_test`，程序只做输入读取，不做输出驱动。
- 已部署到 i.MX6ULL：`/opt/edge-ai-safety-monitor/gpio_test`
- `gpio117` 可读，当前默认 `value=0`；短接 GND 不产生变化，原因是当前接法缺少 10k 上拉或稳定 3.3V DO 输入。
- 外部 GPIO 输入实测暂缓，不算失败；只有补齐上拉/稳定输入源并贴回 0/1 变化后才可记为通过。
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

## 7. 用户待手动验证项

本轮没有猜测 GPIO 编号，也没有运行输入读取程序。请用户选择一个安全输入源，建议优先使用门磁或板载/外接按键；确认接线后再运行读取。

当前补充状态：

- 默认 dtb：`100ask_imx6ull-14x14.dtb`
- 备用 custom dtb：`imx6ull-100ask-custom.dtb`
- `gpio117`：可读，默认 `value=0`
- 短接 GND：无 0/1 变化
- 暂缓原因：缺少 10k 上拉或稳定 3.3V DO 输入
- 结论：外部 GPIO 输入待补测，不标记已通过

| 项目 | 记录 |
|---|---|
| 输入源 | 门磁 / 按键 |
| 接线 GPIO | TODO |
| 空闲值 | TODO |
| 触发值 | TODO |
| 是否稳定 | TODO |

建议命令：

```bash
ssh root@192.168.137.110
cd /opt/edge-ai-safety-monitor
./gpio_test --sysfs <GPIO_NUM> --watch
```

如果用户确认的是 gpiochip base + line，也可以使用：

```bash
./gpio_test --gpiochip gpiochip<BASE> --line <LINE> --watch
```

期望现象：

- 空闲状态持续打印同一个 `value=0` 或 `value=1`。
- 改变门磁/按键状态后，`value` 在 `0` 与 `1` 之间变化。
- 只有用户贴回真实 0/1 变化输出后，才能把 GPIO 输入写成已验证。

## 8. 本轮边界

本记录当前只完成 GPIO 输入验证准备；未验证 I2C/PCA9685/PWM/MG90/MOS。
