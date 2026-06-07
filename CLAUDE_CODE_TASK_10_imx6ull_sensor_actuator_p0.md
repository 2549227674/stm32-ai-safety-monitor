# Task 10：i.MX6ULL P0 传感器/执行器真实化（垂直切片）

## 目标 / 范围

把本地安全闭环从"打桩"做成"真实"：门磁 / PIR / 火焰 / MQ-2 四路输入读真实电平，蜂鸣器 / RGB 两路输出真实动作。按**垂直切片**逐个端到端打通（i.MX 采样 → 本地状态机 → 上报 Flask → Dashboard 可见），一片通过再做下一片。

不在本任务：继电器、风扇、水泵、MOS、温湿度、MPU6050（P1/P2）；不改 OPi5/AI；不改 Flask schema。

## 前置条件

- Task07/Task08 已通过（imx_safetyd C 版、端边链路、Dashboard 字段展示可用）。
- `CANONICAL_DECISIONS.md` 第 0.6 节 P0 引脚表已合并并由用户板端确认 D1–D7 可作 GPIO、5V 模块电平已分压。
- 所有 P0 模块已装在外置面包板/洞洞板上，星形共地，独立供电。

## 引脚（以 CANONICAL 0.6 为准，禁止在本文另造）

```text
输入: door=gpio118(D1)  pir=gpio117(D0)  flame=gpio119(D2)  mq2=gpio120(D3)
输出: rgb_r=gpio121(D4)  buzzer=gpio122(D5,经三极管)  rgb_g=gpio123(D6)  rgb_b=gpio124(D7)
舵机: PCA9685 /dev/i2c-0 0x40 CH0/CH1 (保持不动)
```

## 切片拆分与顺序

| 切片 | 内容 | 选它的理由 |
|---|---|---|
| 10-A | **门磁**（首切片） | passive 触点、无分压、无电平风险、最易拿到干净 0/1 证据，先把"真实输入→状态机→Dashboard"骨架跑通 |
| 10-B | 火焰 flame DO | 打火机可控触发，验证强触发→ALARM |
| 10-C | MQ-2 DO | 验证分压链路 + 烟雾强触发→ALARM |
| 10-D | PIR 真实触发 | 补 raw=1 人体触发证据，解除历史 caveat |
| 10-E | 蜂鸣器 + RGB 输出 | 状态色 + ALARM 鸣响，坐实本地报警 |

未通过当前切片不进入下一切片。

## 操作步骤（每个切片通用）

1. 板端 `gpioinfo` 确认目标 line 可用且未被占用。
2. 输入切片：`gpioget` / sysfs 读裸电平，手动制造 0/1 变化（开合门磁、近火焰、近烟雾、人体走过）记录。
3. 接入 `imx_safetyd`：让对应 sensors 字段读真实 GPIO（替换固定 0）。
4. 验证状态机：制造异常 → 观察 state/risk_score 变化；强触发（flame/mq2=1）必须**纯本地直接进 ALARM**。
5. 输出切片：`gpioset` 单独验证 RGB/蜂鸣器，再接入状态机按状态联动。
6. 跑一条完整端边事件，确认 Dashboard 可见 sensors/state/actuators。

## 命令骨架

```bash
ssh <IMX_USER>@<IMX_IP>
gpioinfo | grep -E "11[7-9]|12[0-4]"
# 输入裸读
gpioget gpiochip0 118        # door
# 输出裸验
gpioset gpiochip0 122=1; sleep 1; gpioset gpiochip0 122=0   # buzzer
# 接入后跑 daemon
imx_safetyd once
imx_safetyd loop
```

## 产出物

- `edge/imx6ull-controller/src/` 中 P0 输入读取与输出驱动模块（沿用现有 bsp_*/app_* 风格，代码注释英文）。
- `imx_safetyd.c` 中 `read_gpio_inputs()` 读真实 door/flame/mq2/pir；`apply_actuators_by_state()` 真实驱动 buzzer/rgb。
- 每切片一份 `tests/imx6ull/YYYY-MM-DD_p0_<sensor>.md`，含裸电平证据、状态机变化、Dashboard 截图路径。

## 验收标准

- [ ] 10-A 门磁：`gpioget` 开合门磁读到稳定 0↔1；daemon 中 `door` 反映真实值；Dashboard 可见。
- [ ] 10-B 火焰：近火焰 `flame=1`，本地直接 ALARM（断网/断 OPi5 仍成立）。
- [ ] 10-C MQ-2：分压实测进 GPIO ≤3.3V；近烟雾 `mq2=1`，本地直接 ALARM。
- [ ] 10-D PIR：取得 `raw=1` 人体触发证据（不再用 FORCE_VERIFY 代替）。
- [ ] 10-E 输出：RGB 按 NORMAL 绿/VERIFY 蓝/ALARM 红/FAULT 紫黄 联动；ALARM 时蜂鸣器鸣响。
- [ ] **独立性**：拔网线/停 OPi5/停 Flask，制造 flame/mq2 强触发，本地仍 ALARM 且蜂鸣器/RGB 动作（这是 P0 的核心证据）。
- [ ] `control_allowed` 全程 `false`；AI/OPi5/Flask 未驱动任何执行器。

## 禁止事项

- 不引用 CANONICAL 0.6 之外的引脚；冲突以 CANONICAL 为准。
- MQ-2 不分压不接线；进 GPIO 电平未实测 ≤3.3V 不连。
- 蜂鸣器不经 PCA9685（必须直连 GPIO，保独立性）。
- 不接继电器/风扇/水泵/220V（非本任务）。
- 不把未实测切片写成"已通过"。
- 不改 OPi5/AI 服务。
- 不改 Flask DB schema。

## 完成后回填

- 回填 CANONICAL 0.6 状态列、docs/03、docs/06、docs/11、AGENTS.md。
- 解除 docs/06 中 door/flame/mq2/PIR 与执行器的历史 caveat。
- 更新 DEVPLAN 与 CLAUDE_CODE_TODO_INDEX。
