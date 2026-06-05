# 03 硬件系统设计与供电安全（i.MX6ULL + Orange Pi 5）

## 3.1 硬件模块总览

| 模块 | 作用 | 主控关系 | 状态 |
|---|---|---|---|
| i.MX6ULL Pro | Linux 本地安全控制、V4L2 抓拍、云台控制 | 第一/二层主节点 | 待实测 |
| Orange Pi 5 16GB + 128G NVMe | RKNN 本地推理、可选 Flask 最终部署 | 第四层 AI 节点 | 待实测 |
| 1080p USB 摄像头 | 图像采集 | 接 i.MX6ULL USB Host | 待实测 |
| 16 路 PWM 驱动板 | 舵机 PWM | i.MX6ULL I2C 控制 | 待实测 |
| MG90 二维云台 | 摄像头 pan/tilt | PCA9685 输出 PWM | 待实测 |
| MOS 管 | 低压负载开关 | i.MX6ULL GPIO/PWM 控制 | 待实测 |
| 逻辑分析仪 | GPIO/I2C/PWM 波形验证 | 调试工具 | 已有工具，待用于新平台 |

## 3.2 系统连接拓扑

```text
12V 电池/适配器
  ├─ DC-DC 5V/4A~5A  → Orange Pi 5
  ├─ DC-DC 5V/2A     → i.MX6ULL Pro
  ├─ DC-DC 5V~6V/3A  → PCA9685 V+ / MG90 舵机
  ├─ 负载电源         → 风扇/水泵/继电器低压负载
  └─ GND 星形共地     → i.MX6ULL / OPi5 / PCA9685 / MOS / 负载
```

> **[CLAUDE_CODE_TODO | MEASURE]** 实测各路电源电压与空载/负载电流
> - 为何 GPT 给不了：沙箱无法连接电源模块、板卡、舵机和负载。
> - 期望产物/操作：用万用表测量 OPi5、i.MX6ULL、舵机电源、负载电源的电压；记录空载与典型工作电流。
> - 回填位置：docs/03 第 3.7 节供电预算表；docs/11 测试记录
> - 验收：测试记录包含电压、电流、负载状态、是否掉压。


## 3.3 i.MX6ULL Pro 连接设计

### GPIO 输入

建议将门磁、PIR、火焰 DO、按键等作为数字输入。每个输入应明确：GPIO chip、line 编号、有效电平、是否需要上拉/下拉、线缆长度。

| 信号 | GPIO chip/line | 有效电平 | 备注 |
|---|---|---|---|
| door | `<TODO:FILL>` | `<TODO:VERIFY>` | 门磁 |
| pir | `<TODO:FILL>` | `<TODO:VERIFY>` | 人体红外 |
| flame | `<TODO:FILL>` | `<TODO:VERIFY>` | 火焰 DO |
| user_key | `<TODO:FILL>` | `<TODO:VERIFY>` | 调试按键 |

> **[CLAUDE_CODE_TODO | FILL]** 填写 i.MX6ULL GPIO chip/line 与有效电平
> - 为何 GPT 给不了：不同 i.MX6ULL Pro 板卡引脚丝印、设备树和 Linux gpiochip 编号不同。
> - 期望产物/操作：在板端运行 `gpiodetect`、`gpioinfo`，结合原理图/扩展板丝印填写表格。
> - 回填位置：docs/03 第 3.3 GPIO 表；Task03 配置文件
> - 验收：每一路能用 `gpioget` 或测试程序读到高低变化。


### I2C PCA9685

PCA9685 通过 I2C 控制 16 路 PWM，适合驱动 MG90 舵机。逻辑电源与舵机电源必须分开，舵机电源不要从 i.MX6ULL 板卡取。

| 项目 | 值 |
|---|---|
| I2C bus | `<TODO:FILL>` |
| PCA9685 地址 | `<TODO:VERIFY>` |
| 逻辑电压 | `<TODO:VERIFY>` |
| 舵机电源 | 5V~6V 独立供电，实测 `<TODO:MEASURE>` |

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 PCA9685 I2C 地址和电平兼容性
> - 为何 GPT 给不了：不同模块 A0–A5 焊盘状态不同，i.MX6ULL IO 电平也需按板卡确认。
> - 期望产物/操作：运行 `i2cdetect -y <bus>`，用万用表确认 VCC/V+，必要时使用电平转换。
> - 回填位置：docs/03 第 3.3 I2C 表；Task03
> - 验收：`i2cdetect` 能看到设备地址，空载 PWM 有逻辑分析仪证据。


### MOS 输出

MOS 用于低压风扇、水泵、继电器模块等负载开关。建议每路包括栅极串联电阻、栅极下拉、电机负载续流/保护，且默认 OFF。

| 负载 | 控制 GPIO | 负载电压 | 默认态 | 保护 |
|---|---|---|---|---|
| fan | `<TODO:FILL>` | `<TODO:VERIFY>` | OFF | `<TODO:VERIFY>` |
| pump | `<TODO:FILL>` | `<TODO:VERIFY>` | OFF | `<TODO:VERIFY>` |
| buzzer | `<TODO:FILL>` | `<TODO:VERIFY>` | OFF | `<TODO:VERIFY>` |

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 MOS 模块接线和默认 OFF
> - 为何 GPT 给不了：沙箱无法确认 MOS 型号、栅极下拉、负载电压和续流保护。
> - 期望产物/操作：先不接负载，测 GPIO 默认电平；再接 LED/小风扇验证；最后再接泵。
> - 回填位置：docs/03 第 3.3 MOS 表；Task03
> - 验收：上电未运行程序时负载不动作，程序控制后可开关。


### USB 摄像头与以太网

USB 摄像头接 i.MX6ULL USB Host，用 V4L2 抓静帧。i.MX6ULL 与 OPi5 建议通过有线网连接同一交换机/路由器，降低 WiFi 不确定性。

## 3.4 Orange Pi 5 连接设计

Orange Pi 5 负责 RKNN 推理，建议使用稳定 5V 大电流供电，模型和图片临时文件放 NVMe 或本地存储。OPi5 不直接连接执行器。

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 Orange Pi 5 系统、供电和网络状态
> - 为何 GPT 给不了：沙箱无法访问 OPi5。
> - 期望产物/操作：记录系统版本、Python 版本、rknn 依赖、IP、SSH 可用性；真实 IP 不入库。
> - 回填位置：docs/03 第 3.4；docs/09；Task05
> - 验收：`ssh OPI5_USER@OPI5_IP`、`python3 --version`、`curl /health` 均有记录。


## 3.5 PCA9685 与 MG90 云台

MG90 云台建议先做三点扫描：pan 60°/90°/120°，tilt 固定 90°。先空载验证 PWM，再接单个舵机，再接二维云台。

## 3.6 MOS 管负载驱动

负载驱动必须遵守：

- i.MX6ULL IO 不直接驱动电机或水泵。
- 负载电源与控制电源共地。
- 栅极默认下拉。
- 电机负载加续流或使用带保护模块。
- 演示用低压负载，不接 220V。

## 3.7 供电预算

| 用电对象 | 设计供电 | 估算用途 | 实测电流 |
|---|---|---|---|
| i.MX6ULL Pro | 5V/2A 独立 | Linux 控制与 USB 摄像头 | `<TODO:MEASURE>` |
| Orange Pi 5 | 5V/4A~5A 独立 | RKNN 推理与服务 | `<TODO:MEASURE>` |
| PCA9685 + MG90 | 5V~6V/3A 独立 | 舵机峰值 | `<TODO:MEASURE>` |
| 风扇/水泵 | 按模块额定电压 | 低压负载演示 | `<TODO:MEASURE>` |

## 3.8 共地与保护

所有低压系统必须共地，但高电流负载不要让电流穿过 i.MX6ULL 板卡地线。推荐星形地：电源负极作为公共点，分别回到 i.MX6ULL、OPi5、PCA9685、MOS 负载。

## 3.9 禁止事项

- 不从 i.MX6ULL 或 OPi5 的 IO/5V 引脚给舵机供电。
- 不接 220V 强电。
- 不在未确认共地时连接信号线。
- 不在程序默认态未验证前连接水泵等负载。

## 3.10 逻辑分析仪测试点

| 测试点 | 目的 | 预期 |
|---|---|---|
| I2C SCL/SDA | 验证 PCA9685 通信 | 有标准 I2C 波形，地址 ACK |
| PWM 输出 | 验证舵机脉宽 | 50Hz，脉宽随角度变化 |
| GPIO 输出 | 验证 MOS 控制 | 高低电平切换明确 |
| GPIO 输入 | 验证门磁/PIR | 触发前后电平变化 |

## 3.11 材料与实物布局建议

板卡、降压模块和负载建议固定在非导电底板上，强弱电分区，舵机线和电源线留出应力释放。调试阶段所有外设先单独验证，再进入整机联调。
