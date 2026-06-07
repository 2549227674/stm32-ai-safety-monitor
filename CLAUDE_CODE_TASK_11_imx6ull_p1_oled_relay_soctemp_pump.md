# Task 11：i.MX6ULL P1 扩展（OLED / 继电器 / SoC 温度热健康 / 隔离水箱）

## 目标 / 范围

在 P0 本地安全闭环之上叠加 P1：本地 OLED 状态屏、继电器、SoC 温度设备热健康闭环、隔离水箱喷淋演示。全部走 **I2C 总线 + PCA9685 空闲通道 + SoC 内部温度（软件）**，不占用 P0 的直连 GPIO。按垂直切片逐个端到端打通，一片通过再下一片。

不在本任务：P0 四输入/蜂鸣器/RGB（Task10）；温湿度环境传感器（已决定不做，只读 SoC 温度，无湿度）；MPU6050（P2）。

## 前置条件

- **Task10（P0）主体验收完成**：
  - door 门磁：Task10-A 跳过/暂缓（无外部 10k 上拉时裸读不稳定）；
  - flame 火焰：Task10-B 已通过（3.3V 供电，DO 高有效，flame=1→ALARM）；
  - MQ-2 烟雾：Task10-C 已通过（课设短时 5V + DO 直连接法）；
  - PIR 人体红外：Task10-D 沿用 Task03-A 历史证据（空闲 raw=0，触发 raw=1）；
  - buzzer 蜂鸣器：Task10-E 已通过（gpio122 active low）；
  - RGB 状态灯：Task10-E2 已通过（PCA9685 CH2/CH3/CH4 推荐演示）。
- `CANONICAL_DECISIONS.md` 第 0.7 节 P1 分配已合并。
- P1 器件已装外置面包板/洞洞板，星形共地，负载独立供电；MOS 控制端已焊接好排针。

## 引脚 / 资源（以 CANONICAL 0.7 为准，禁止另造）

```text
I2C /dev/i2c-0:  PCA9685=0x40  OLED_SSD1306=0x3C   (0x70=PCA9685 all-call, 保留不用)
PCA9685 通道:    CH0/CH1=舵机(不动)  CH2/CH3/CH4=RGB(Task10-E2已用,推荐演示)
                 CH5=继电器  CH6=水泵(经MOS)
蜂鸣器:          gpio122 / J5 D5 / active low / 不经 PCA9685
SoC 温度:        cat /sys/class/thermal/thermal_zone0/temp   (毫摄氏度; 备选 /sys/class/hwmon/hwmon0/temp1_input)
```

约束：PCA9685 全局 50Hz，继电器/水泵**只做开/关，不调速**；PCA9685 只出控制信号，负载电流靠 MOS/三极管 + 独立电源；执行器只由 i.MX 本地状态机驱动，`control_allowed=false`。

## 切片拆分与顺序

| 切片 | 内容 | 选它的理由 | 状态 |
|---|---|---|---|
| 11-A | **OLED 状态屏**（首切片） | I2C、最直观，点亮后给后续每片做现场可视化；断网也能看，补本地闭环可视化证据 | 待执行 |
| 11-B | 继电器 | 最简单的输出，验证 PCA9685 通道开关 + 默认 OFF | 待执行 |
| 11-C | SoC 温度热健康监测 | 零额外硬件，读 soc_temp → 写入事件/OLED → Dashboard 可见 | 待执行 |
| 11-D | 水泵隔离水箱 | 涉及水，风险最高，最后做 | 待执行 |

未通过当前切片不进入下一切片。

## 操作步骤（每片通用）

1. `i2cdetect -y 0` 确认 0x40 / 0x3C 在线（11-A）。
2. 输出切片：先用通道控制工具单独验证负载开/关，确认**上电及程序未运行时默认 OFF**（11-B/D）。
3. SoC 温度切片：先 `cat` 确认能读到温度路径与数值（11-C）。
4. 接入 `imx_safetyd`：在 `apply_actuators_by_state()` 按状态驱动继电器/水泵；在采样处读 `soc_temp` 写入事件；OLED 每轮刷新 `state/risk_score/soc_temp/关键 sensors`。
5. 制造场景验证联动：ALARM→水泵/继电器动作；SoC 温度→Dashboard/OLED 显示；断网时 OLED 仍刷新。
6. 跑完整端边事件，确认 Dashboard 可见新增字段与执行器状态。

## 命令骨架

```bash
ssh <IMX_USER>@<IMX_IP>
i2cdetect -y 0                              # 期望看到 0x40 与 0x3C
cat /sys/class/thermal/thermal_zone0/temp   # SoC 温度, 毫摄氏度
# PCA9685 通道开关 (沿用/扩展现有 pca9685 helper, 满占空=on, 0=off)
pca9685_set_channel 5 on;  sleep 1; pca9685_set_channel 5 off   # 继电器
pca9685_set_channel 6 on;  sleep 1; pca9685_set_channel 6 off   # 水泵(先空泵点动)
# OLED 自测程序刷一屏
oled_test --i2c 0 --addr 0x3c
# 接入后跑 daemon
imx_safetyd once
imx_safetyd loop
```

## 产出物

- `edge/imx6ull-controller/src/bsp_oled_ssd1306.c`（I2C SSD1306 初始化与刷屏，代码注释英文）。
- `edge/imx6ull-controller/src/` 中 PCA9685 通道开关 helper（继电器/水泵复用）与 SoC 温度读取模块。
- `imx_safetyd.c`：采样处读 `soc_temp`；`apply_actuators_by_state()` 真实驱动 relay/pump；每轮刷新 OLED。
- 每切片一份 `tests/imx6ull/YYYY-MM-DD_p1_<item>.md`，含 i2cdetect/温度读数/默认 OFF 证据/联动现象/Dashboard 截图路径。

## 验收标准

- [ ] 11-A OLED：`i2cdetect` 见 0x3C；OLED 显示 state/risk_score/soc_temp；拔网线后仍刷新。
- [ ] 11-B 继电器：默认 OFF（程序未运行不吸合）；程序可吸合/释放；3.3V 触发已确认（否则记录加三极管）。
- [ ] 11-C SoC 温度：能稳定读到 soc_temp（约 40–60°C）；写入事件与 OLED；device_health=WARN 显示警告；**不**进火灾 ALARM。
- [ ] 11-D 水泵：默认 OFF；隔离水箱内 ALARM 触发喷淋、解除停泵；无漏电、无溅水到电路。
- [ ] 全程 `control_allowed=false`；AI/OPi5/Flask 未驱动任何执行器；P0 本地闭环不受影响。

## 禁止事项

- 不引用 CANONICAL 0.7 之外的引脚/通道/地址；冲突以 CANONICAL 为准。
- 继电器/水泵不接 MOS/三极管与独立电源不上电；不从 i.MX/PCA9685 取负载电流。
- 不做 PWM 调速（50Hz 全局，仅开关）。
- 水箱：不接 220V；水与板卡物理隔离；水泵不长时间空转；未确认默认 OFF 不接泵。
- `soc_temp` 不得当环境温度用，不得据此升级火灾 ALARM；不编湿度字段。
- 不把未实测切片写成"已通过"。
- 不把蜂鸣器迁移到 PCA9685；蜂鸣器仍为 gpio122 直连安全报警输出。

## 完成后回填

- 回填 CANONICAL 0.7 状态列、docs/03、docs/06、docs/07（新增 soc_temp）、docs/10、docs/12、AGENTS.md。
- 更新 DEVPLAN 与 CLAUDE_CODE_TODO_INDEX；在 EXECUTION_GUIDE 执行总顺序追加 Task11。
