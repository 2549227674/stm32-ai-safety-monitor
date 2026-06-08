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
  ├─ 负载电源         → 水泵/继电器低压负载
  └─ GND 星形共地     → i.MX6ULL / OPi5 / PCA9685 / MOS / 负载
```

> **[CLAUDE_CODE_TODO | MEASURE]** 实测各路电源电压与空载/负载电流
> - 为何 GPT 给不了：沙箱无法连接电源模块、板卡、舵机和负载。
> - 期望产物/操作：用万用表测量 OPi5、i.MX6ULL、舵机电源、负载电源的电压；记录空载与典型工作电流。
> - 回填位置：docs/03 第 3.7 节供电预算表；docs/11 测试记录
> - 验收：测试记录包含电压、电流、负载状态、是否掉压。


## 3.3 i.MX6ULL Pro 连接设计

### GPIO 输入（P0 实际值）

引脚分配以 `CANONICAL_DECISIONS.md` 第 0.6 节为唯一事实源。所有 P0 模块、分压电阻、三极管、限流电阻均焊接/插装在**外置面包板（调试）或洞洞板（最终演示）**上，经杜邦线从 J5 引出；i.MX6ULL 核心板不改动。强弱电分区、星形共地。

| 信号 | GPIO line | 有效电平 | 上拉/分压 | 状态 |
|---|---|---|---|---|
| door | gpio118 (J5 D1) | `<TODO:VERIFY>` | 3.3V 上拉，无需分压 | 待验 |
| pir | gpio117 (J5 D0) | 高有效 | 模块自带 | 已验证读到 0/1 |
| flame | gpio119 (J5 D2) | 高有效（raw=1 有火焰） | 3.3V 供电免分压 | 已验证（裸读稳定，flame=1→ALARM） |
| mq2 | gpio120 (J5 D3) | 高有效（raw=1 触发） | 本轮课设短时方案：5V + DO 直连；**长期建议分压/电平转换** | 已验证（课设短时） |

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 J5 D1–D7 在默认 dtb 下可作 GPIO 读写
> - 为何 GPT 给不了：默认 dtb 中这些脚的 pinmux 是否为 GPIO 需板端确认；仅 gpio117(D0) 实测过。
> - 期望产物/操作：板端 `gpioinfo` 查看 D1–D7 对应 line 是否 `unused`；逐脚 `gpioget`/`gpioset` 验证电平变化。
> - 回填位置：本表"状态"列；tests/imx6ull。
> - 验收：每路能读到明确电平变化。

> **[CLAUDE_CODE_TODO | VERIFY]** 确认所有进 i.MX GPIO 的信号实测不超过 3.3V
> - 为何 GPT 给不了：i.MX6ULL GPIO 为 3.3V 且不耐 5V，模块实际供电与 DO 摆幅需实测。
> - 期望产物/操作：MQ-2 必接 10k/20k 分压；火焰/PIR 优先 3.3V 供电免分压；用万用表确认进 GPIO 的电平 ≤3.3V 再连线。
> - 回填位置：本表；tests/imx6ull。
> - 验收：所有进 GPIO 的信号实测 ≤3.3V。

电平安全补充说明：

| 模块 | 供电 | 进 GPIO 电平 | 分压要求 |
|---|---|---|---|
| 门磁 | 3.3V 上拉 | 3.3V | **无需分压**（passive 触点） |
| PIR | 优先 3.3V | 3.3V 或 5V | 3.3V 免分压；若 5V 供电则必须分压 |
| 火焰 | 优先 3.3V | 3.3V 或 5V | 3.3V 免分压；若 5V 供电则必须分压 |
| MQ-2 | 5V（加热丝） | 5V | **必须分压**（R1=10k 上、R2=20k 下） |

### 数字输出（P0）

P0 输出不依赖 MOS 模块（MOS 仍为 P1 扩展项）：

| 负载 | 控制 line | 驱动方式 | 有效电平 | 默认态 | 状态 |
|---|---|---|---|---|---|
| buzzer | gpio122 (J5 D5) | NPN 三极管（S8050）+ 基极 1k | **active low**（0=响） | OFF（gpio=1） | 已验证 |
| rgb_r | PCA9685 CH2 (推荐) / gpio121 (fallback) | PCA9685 PWM 或 GPIO + 限流 | 共阴高有效 | OFF（duty=0） | Task10-E2 PCA9685 已验证 |
| rgb_g | PCA9685 CH3 (推荐) / gpio123 (fallback) | PCA9685 PWM 或 GPIO + 限流 | 共阴高有效 | OFF（duty=0） | Task10-E2 PCA9685 已验证 |
| rgb_b | PCA9685 CH4 (推荐) / gpio124 (fallback) | PCA9685 PWM 或 GPIO + 限流 | 共阴高有效 | OFF（duty=0） | Task10-E2 PCA9685 已验证 |

> **说明**：RGB PCA9685 是视觉增强（亮度更高），蜂鸣器仍是安全报警核心输出。PCA9685 CH0/CH1 专供舵机，CH2/CH3/CH4 用于 RGB。`RGB_BACKEND` 配置可选 `gpio`（Task10-E）或 `pca9685`（Task10-E2，推荐演示）。

> **说明**：蜂鸣器不是分压问题，而是驱动问题——GPIO 只控制 NPN/MOS 三极管基极，不能直接带蜂鸣器。RGB 不是分压问题，而是限流问题——每路 LED 需要 220–330Ω 限流，或确认模块已有可靠限流。继电器/水泵不属于 Task10，本轮不写成已接入。

> **[CLAUDE_CODE_TODO | MEASURE]** 补 3.7 供电预算 P0 行
> - 期望产物/操作：万用表测 5V（MQ-2 加热 + 蜂鸣器）/3.3V（GPIO 上拉、RGB、PIR/火焰）两路电流；记录星形共地点。
> - 回填位置：docs/03 第 3.7。
> - 验收：表中写入实测电压/电流。


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


### MOS 输出（P1/P2，不属于 Task10）

MOS 用于低压水泵、继电器模块等负载开关。建议每路包括栅极串联电阻、栅极下拉、电机负载续流/保护，且默认 OFF。MOS/继电器/水泵属于 P1/P2 扩展项，本轮 Task10 不写成已接入。

| 负载 | 控制 GPIO | 负载电压 | 默认态 | 保护 |
|---|---|---|---|---|
| pump | `<TODO:FILL>` | `<TODO:VERIFY>` | OFF | `<TODO:VERIFY>` |

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 MOS 模块接线和默认 OFF
> - 为何 GPT 给不了：沙箱无法确认 MOS 型号、栅极下拉、负载电压和续流保护。
> - 期望产物/操作：先不接负载，测 GPIO 默认电平；再接 LED 验证；最后再接泵。
> - 回填位置：docs/03 第 3.3 MOS 表；Task03
> - 验收：上电未运行程序时负载不动作，程序控制后可开关。


### USB 摄像头与以太网

USB 摄像头接 i.MX6ULL USB Host，用 V4L2 抓静帧。i.MX6ULL 与 OPi5 建议通过有线网连接同一交换机/路由器，降低 WiFi 不确定性。

### 网络拓扑：全无线优先

当前优先演示方案：PC、i.MX6ULL、OPi5 均连接同一手机热点，全无线自动恢复。

```text
手机热点 (10.96.98.0/24)
├── PC / Windows — Flask Dashboard
├── OPi5 — AI Service（USB RTL8188ETV, 8188eu 驱动）
└── i.MX6ULL — 本地主控（板载 RTL8723BU, 8723bu 驱动, nl80211）
```

WiFi 硬件：

| 设备 | 硬件 | 驱动 | 天线 |
|---|---|---|---|
| i.MX6ULL 板载 | RTL8723BU / 0bda:b720 | 8723bu.ko | IPEX/U.FL 板载天线 |
| OPi5 USB | RTL8188ETV / 0bda:0179 | 8188eu.ko | USB 网卡自带 |

全无线不影响传感器/执行器/供电安全原则。水泵、继电器、舵机等供电与网络无关。详见 `CANONICAL_DECISIONS.md` 第 0.8 节。

有线网络作为回退方案保留。

## 3.4 Orange Pi 5 连接设计

Orange Pi 5 负责 RKNN 推理，建议使用稳定 5V 大电流供电，模型和图片临时文件放 NVMe 或本地存储。OPi5 不直接连接执行器。

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 Orange Pi 5 系统、供电和网络状态
> - 为何 GPT 给不了：沙箱无法访问 OPi5。
> - 期望产物/操作：记录系统版本、Python 版本、rknn 依赖、IP、SSH 可用性；真实 IP 不入库。
> - 回填位置：docs/03 第 3.4；docs/09；Task05
> - 验收：`ssh OPI5_USER@OPI5_IP`、`python3 --version`、`curl /health` 均有记录。


## 3.5 PCA9685 与 MG90 云台

MG90 云台建议先做三点扫描：pan 60°/90°/120°，tilt 固定 90°。先空载验证 PWM，再接单个舵机，再接二维云台。

## 3.6 P1 低压负载、OLED 与隔离水箱

P1 在 P0 本地安全闭环之上叠加扩展，全部走 I2C 总线 + PCA9685 空闲通道，不占用 P0 的直连 GPIO。引脚分配以 `CANONICAL_DECISIONS.md` 第 0.7 节为唯一事实源。

### OLED SSD1306 接线

| 项目 | 说明 |
|---|---|
| 型号 | SSD1306 I2C 0.96 寸 |
| SCL/SDA | 走 /dev/i2c-0（J5 pin7/8） |
| I2C 地址 | 0x3C |
| 逻辑电压 | 3.3V |
| 与 PCA9685 共总线 | 0x40 已用，0x3C 已验证（i2cdetect 同时 ACK） |
| 状态 | 已验证（Task11-A：oled_test 刷屏通过，imx_safetyd loop 刷新通过） |

> ~~**[CLAUDE_CODE_TODO | VERIFY]** I2C 0x40/0x3C 共存~~ **已验证**：i2cdetect 同时看到 0x40/0x3C；oled_test 刷屏不影响 PCA9685 RGB。证据见 `tests/imx6ull/2026-06-07_p1_oled_status_screen.md`。

### P1 低压负载表

| 负载 | 控制 | 驱动 | 负载电源 | 保护 | 默认 | 状态 |
|---|---|---|---|---|---|---|
| relay | PCA9685 CH5 | KY-019 模块自带驱动 | 独立 5V | 模块自带 | OFF | 已验证（active high，默认 OFF，ALARM 动作） |
| pump | PCA9685 CH6 | MOS（栅极下拉 + 1k 串阻） | 独立 5V | 续流二极管 | OFF | 已验证（双负载：水泵+水枪发射器并联，active high，default OFF，ALARM 动作） |

### SoC 温度

SoC 温度为纯软件读取，无外部器件、无接线、无供电、无湿度。读取路径：`/sys/class/thermal/thermal_zone0/temp`（毫摄氏度），备选 `/sys/class/hwmon/hwmon0/temp1_input`。用途：设备热健康，不是环境温度，不用于火灾 ALARM 判定。

### 隔离水箱

- 水泵在封闭水箱/水盆中，沉入水中。
- 出水管 → 喷头，喷向箱内壁，水只在箱内循环，不外溢。
- 驱动电路、i.MX、PCA9685 在干燥区，泵的电源线从干燥区 MOS 驱动板引到水箱。
- 泵独立 5V 供电，仅控制信号与系统共地；MOS 漏极串续流二极管。
- 红线：不接 220V；水与板卡物理隔离；演示前确认无水汽近板、无漏电。
- 先空泵点动确认通断，再装水；水泵不长时间空转。
- 可选降级：现场不便用水时，用 LED 或空管点动替代。

### 与 Task10-E2 的关系

PCA9685 CH2/CH3/CH4 已用于 RGB 视觉增强（Task10-E2），P1 执行器从 CH5（继电器）、CH6（水泵）开始。蜂鸣器仍为 gpio122 直连安全报警输出，不经 PCA9685。

### P1 安全边界

- PCA9685 全局 50Hz 保持不变，服从舵机与当前 RGB 视觉增强。
- 继电器、水泵只做 ON/OFF，不做调速 PWM。
- PCA9685 只输出控制信号，不承担负载电流。
- 继电器线圈、水泵必须独立供电，星形共地。
- 水泵等感性负载需要 MOS/三极管驱动和必要续流保护。
- 执行器只由 i.MX 本地状态机驱动。
- AI/OPi5/Flask 不控制执行器，`control_allowed=false`。
- 不接 220V。
- 水箱演示最后做，水与板卡物理隔离。

## 3.8 MOS 管负载驱动（扩展项）

MOS 低压负载作为扩展执行器接口保留，当前 MVP 不依赖。XY-MOS 模块控制端 GND/TRIG-PWM 为圆孔焊盘，需额外焊接排针或飞线。当前演示执行器以 PCA9685 + MG90S 分时云台为主。

负载驱动必须遵守：

- i.MX6ULL IO 不直接驱动电机或水泵。
- 负载电源与控制电源共地。
- 栅极默认下拉。
- 电机负载加续流或使用带保护模块。
- 演示用低压负载，不接 220V。

## 3.9 供电预算

| 用电对象 | 设计供电 | 估算用途 | 实测电流 |
|---|---|---|---|
| i.MX6ULL Pro | 5V/2A 独立 | Linux 控制与 USB 摄像头 | `<TODO:MEASURE>` |
| Orange Pi 5 | 5V/4A~5A 独立 | RKNN 推理与服务 | `<TODO:MEASURE>` |
| PCA9685 + MG90 | 5V~6V/3A 独立 | 舵机峰值 | `<TODO:MEASURE>` |
| 水泵 | 按模块额定电压 | 低压负载演示 | `<TODO:MEASURE>` |

## 3.10 共地与保护

所有低压系统必须共地，但高电流负载不要让电流穿过 i.MX6ULL 板卡地线。推荐星形地：电源负极作为公共点，分别回到 i.MX6ULL、OPi5、PCA9685、MOS 负载。

## 3.11 禁止事项

- 不从 i.MX6ULL 或 OPi5 的 IO/5V 引脚给舵机供电。
- 不接 220V 强电。
- 不在未确认共地时连接信号线。
- 不在程序默认态未验证前连接水泵等负载。

## 3.12 逻辑分析仪测试点

| 测试点 | 目的 | 预期 |
|---|---|---|
| I2C SCL/SDA | 验证 PCA9685 通信 | 有标准 I2C 波形，地址 ACK |
| PWM 输出 | 验证舵机脉宽 | 50Hz，脉宽随角度变化 |
| GPIO 输出 | 验证 MOS 控制 | 高低电平切换明确 |
| GPIO 输入 | 验证门磁/PIR | 触发前后电平变化 |

## 3.13 材料与实物布局建议

板卡、降压模块和负载建议固定在非导电底板上，强弱电分区，舵机线和电源线留出应力释放。调试阶段所有外设先单独验证，再进入整机联调。
