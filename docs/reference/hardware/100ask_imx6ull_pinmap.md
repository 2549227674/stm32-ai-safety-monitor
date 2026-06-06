# 100ask i.MX6ULL V1.1 Task03 硬件引脚参考

## 1. 文档目的

本文根据 `docs/reference/hardware/100ask_imx6ull_v1.1.pdf` 整理 Task03 前的硬件索引，重点服务 GPIO 输入盘点，并为后续 I2C/PWM/MOS 单模块验证保留参考。

本文只提供原理图依据，不替代板端设备树、pinmux、sysfs/libgpiod 盘点和真实 0/1 变化实测。未经真实输入变化验证，不得把 GPIO 输入写成已通过。

## 2. 当前设备树基线

| 项目 | 状态 |
|---|---|
| 默认 dtb | `100ask_imx6ull-14x14.dtb` |
| 原 custom dtb | `imx6ull-100ask-custom.dtb`，仅保留备用 |
| Task03 当前基线 | 默认 dtb：`100ask_imx6ull-14x14.dtb` |
| 板端 model | `Freescale i.MX6 ULL 14x14 EVK Board` |

## 3. U1 核心板连接器关键信号

来源：原理图第 2 页，U1 `myir_board` 连接器。表中“是否建议 Task03-A 使用”只针对 GPIO 输入验证准备，不代表电气已实测通过。

| 类别 | U1 pin / 信号 | 原理图网络/去向 | 是否建议 Task03-A 使用 | 备注 |
|---|---|---|---|---|
| GPIO | 40 / `GPIO1_IO00` | `USB_OTG1_ID`，到第 10 页 | 否 | 关联 USB OTG ID，不建议作为通用输入。 |
| GPIO | 41 / `GPIO1_IO01` | `GPIO1_IO01`，到第 5 页 | 谨慎 | 已在第 5 页连接到 ICM20608_INT，可能被传感器中断占用。 |
| GPIO | 42 / `GPIO1_IO02` | `ADC1_CH2`，到第 7 页 | 候选 | 用户要求列为候选，但需确认物理可接位置和当前 pinmux。 |
| GPIO | 43 / `GPIO1_IO03` | `ADC1_CH3`，到第 5 页 | 不优先 | 可能关联 ADC/板载功能，先不作为首选。 |
| GPIO | 44 / `GPIO1_IO04` | `ADC1_CH4`，到第 5 页 | 不优先 | 可能关联 ADC/板载功能，先不作为首选。 |
| GPIO | 45 / `GPIO1_IO05` | `GPIO1_IO05`，到第 5 页 | 候选 | 用户要求列为候选，但需确认物理可接位置和当前 pinmux。 |
| ENET | 46 / `MDIO` | `ENET_MDIO`，到第 6 页 | 否 | 以太网管理信号，不用于 Task03-A 输入。 |
| ENET | 47 / `MDC` | `ENET_MDC`，到第 6 页 | 否 | 以太网管理信号，不用于 Task03-A 输入。 |
| GPIO | 48 / `GPIO1_IO08` | `LCD_PWM`，到第 7 页 | 否 | 关联 LCD PWM，不建议占用。 |
| GPIO | 49 / `GPIO1_IO09` | `LCD_DISP`，到第 7 页 | 候选但谨慎 | 用户要求列为候选；若 LCD/display 正在使用则不要占用。 |
| UART1 | 51 / `UART1_TXD`, 52 / `UART1_RXD`, 53 / `UART1_RTS`, 54 / `UART1_CTS` | 到第 4 页 | 否 | UART 调试/通信资源，不作为 GPIO 输入首选。 |
| UART2 | 55 / `UART2_TXD`, 56 / `UART2_RXD`, 57 / `UART2_RTS`, 58 / `UART2_CTS` | ECSPI3 复用网络到第 5 页 | 否 | 已作为 SPI 相关复用网络出现，不用于 Task03-A。 |
| UART3 | 60 / `UART3_TXD`, 61 / `UART3_RXD`, 62 / `UART3_RTS`, 63 / `UART3_CTS` | 到第 12 页 | 否 | UART 资源，不作为 GPIO 输入首选。 |
| UART4 | 64 / `UART4_TXD`, 65 / `UART4_RXD` | 第 5/7/8/9 页复用 I2C1 网络 | 否 | 可能与 I2C1 复用，不用于 Task03-A。 |
| UART5 | 66 / `UART5_TXD`, 67 / `UART5_RXD` | 第 5/7/8/9 页复用 I2C2 网络 | 否 | 可能与 I2C2 复用，不用于 Task03-A。 |
| I2C1 | 64 / `I2C1_SDA`, 65 / `I2C1_SCL` | 到第 5 页 | 否 | 后续 I2C 任务参考，本轮不进入 I2C。 |
| I2C2 | 66 / `I2C2_SDA`, 67 / `I2C2_SCL` | 到第 5/7/8/9 页 | 否 | 后续 I2C 任务参考，本轮不进入 I2C。 |
| ENET1 | 69 / `ETH1_LED1`, 70 / `ETH1_LED2`; 72-76 / `ETH1_TXN/TXP/RXN/RXP` | 到第 6 页 | 否 | 以太网相关，不得误用。 |
| ENET2 | 78-85 / `ENET2_TX_CLK/RXD0/CRS_DV/RXER/RXD1/TXEN/TXD1/TXD0` | 到第 6 页 | 否 | 以太网相关，不得误用。 |
| LCD | 87-118 / `LCD_DATA0..23`, `LCD_PCLK`, `LCD_DE`, `LCD_HSYNC`, `LCD_VSYNC`, `LCD_RESET` | 到第 7 页 | 否 | LCD 正在使用或预留显示链路时不要占用。 |
| SD1 | 122-127 / `SD1_DATA3..0`, `SD1_CMD`, `SD1_CLK` | 到第 4 页 | 否 | SD 卡/启动存储相关，不得误用。 |
| CSI | 129-140 / `CSI_DATA7..0`, `CSI_VSYNC`, `CSI_HSYNC`, `CSI_PIXCLK`, `CSI_MCLK` | 到第 5 页 J5 Camera | 部分候选 | 仅 D0-D2 作为外部输入候选；必须确认未启用摄像头/CSI 功能并配上拉或稳定 3.3V DO。 |

## 4. J5 Camera 接口

来源：原理图第 5 页，J5 `CAMERA_PORT`。注意 J5 同时带 5V、3.3V 和 CSI/I2C 信号，接线时必须确认电平，不能把电源脚接入 GPIO。

| J5 pin | J5 标注 | 原理图网络 | Task03-A 说明 |
|---|---|---|---|
| 1 | `VDD_5V` | `VDD_5V` | 5V 电源脚，不得接入 GPIO。 |
| 2 | `GND1` | `GND` | 地。 |
| 3 | `VDD_3V3` | `VDD_3V3` | 3.3V 电源脚，只作供电参考。 |
| 4 | `GND2` | `GND` | 地。 |
| 7 | `I2C_SDA` | `I2C1_SDA` | Task03-B 已验证：PCA9685 SDA 接入后在 `/dev/i2c-0` 扫到 `0x40`。 |
| 8 | `I2C_SCL` | `I2C1_SCL` | Task03-B 已验证：PCA9685 SCL 接入后在 `/dev/i2c-0` 扫到 `0x40`。 |
| 15 | `D0` | `CSI_DATA0` | GPIO 已验证：PIR / HC-SR501 DO 接入 `gpio117`，可读到稳定 0/1 输入变化。 |
| 16 | `D1` | `CSI_DATA1` | GPIO 备选：当前用户记录为 `gpio118`。 |
| 17 | `D2` | `CSI_DATA2` | GPIO 备选：当前用户记录为 `gpio119`。 |
| 18 | `D3` | `CSI_DATA3` | 不优先，保留 CSI 数据线参考。 |
| 19 | `D4` | `CSI_DATA4` | 不优先，保留 CSI 数据线参考。 |
| 20 | `D5` | `CSI_DATA5` | 不优先，保留 CSI 数据线参考。 |
| 21 | `D6` | `CSI_DATA6` | 不优先，保留 CSI 数据线参考。 |
| 22 | `D7` | `CSI_DATA7` | 不优先，保留 CSI 数据线参考。 |

## 5. GPIO 候选建议

| 候选 | Linux sysfs GPIO | 建议等级 | 使用前提 |
|---|---:|---|---|
| J5 D0 / `CSI_DATA0` | `gpio117` | 已验证 | PIR / HC-SR501 DO 已验证 0/1 输入变化；裸门磁/裸按键仍需 10k 上拉或其他明确电平源。 |
| J5 D1 / `CSI_DATA1` | `gpio118` | 备选 | 需确认 pinmux 未被 CSI/摄像头占用，并使用可靠上拉/下拉。 |
| J5 D2 / `CSI_DATA2` | `gpio119` | 备选 | 需确认 pinmux 未被 CSI/摄像头占用，并使用可靠上拉/下拉。 |
| `GPIO1_IO02` | `gpio2` | 候选 | 需确认物理可接位置、电平和默认 pinmux。 |
| `GPIO1_IO05` | `gpio5` | 候选 | 需确认物理可接位置、电平和默认 pinmux。 |
| `GPIO1_IO09` | `gpio9` | 候选但谨慎 | 需确认 LCD/display 未使用，且物理位置可安全接线。 |

## 6. 暂不建议使用 / 禁止误用清单

| 引脚/资源 | 结论 | 原因 |
|---|---|---|
| ENET 相关引脚 | 不使用 | 网络链路已作为 Task02/后续部署基础，不能占用。 |
| SD1 相关引脚 | 不使用 | SD/启动存储相关，误用风险高。 |
| LCD 正在使用的引脚 | 不使用 | 显示链路可能占用，避免影响系统。 |
| PHY reset 引脚 `gpio134`/`gpio137` | 不使用 | 以太网 PHY reset 相关，误用会影响网络。 |
| `gpio133` | 不作为输入 | 当前为 sysfs `out hi`，不用于 Task03-A 输入。 |
| `gpio110`/`gpio129` | 只作无接线观察参考 | 板载 User Button，被内核按键驱动占用。 |
| J5 pin 1 `VDD_5V` | 禁止接入 GPIO | 5V 电源脚，直接接 GPIO 有损坏风险。 |

## 7. Task03-A 当前结论

- `gpio_test` 软件准备完成。
- 默认 dtb 已恢复为 `100ask_imx6ull-14x14.dtb`。
- sysfs GPIO fallback 可用。
- PIR / HC-SR501 DO 已接入 J5 D0 / `CSI_DATA0` / `gpio117` 并验证 0/1 输入变化。
- 裸门磁/裸按键因无 10k 上拉暂未测；10k 上拉仅作为裸门磁/裸按键方案的备用需求。
- GPIO 输入已通过 Task03-A。
- J5 pin 1 是 `VDD_5V`，只能作为 PIR VCC 等电源使用，不得直接接入 GPIO。
- Task03-B I2C/PCA9685 地址验证已通过：PCA9685 逻辑侧接 J5 pin 3/2/7/8，`/dev/i2c-0` 上 `0x40` ACK。
- Task03-C 空载 PWM 软件准备已完成：`pca9685_pwm_test` 已让 PCA9685 channel 0 按 1.0/1.5/2.0ms 配置运行一次。
- 用户决定跳过逻辑分析仪波形实测，不再等待截图；PWM 波形不标记已通过。
- 本轮未接 PCA9685 `V+`、未接 MG90S；MG90S/云台/MOS 本文未标记通过。
- 允许进入 Task03-D1 PCA9685 `V+` 独立供电与共地检查，以及 Task03-D2 单 MG90S 小角度动作测试。
- Task03-D MG90S 分时大幅动作（1100→1900us）已通过，Task08 云台巡检建议采用分时策略。
