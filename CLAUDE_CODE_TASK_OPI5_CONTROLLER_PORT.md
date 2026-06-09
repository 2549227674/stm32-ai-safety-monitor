# Codex 执行任务：i.MX6ULL 故障切换 —— 安全主控移植到 OPi5

## 背景（必须先读）

- i.MX6ULL Pro 末期联调时出现供电/启动异常（裸板电源灯闪、串口无 U-Boot、USB 下载不枚举），
  无万用表/可调电源，无法在答辩前更换板卡。
- 决定：把本地安全主控 `imx_safetyd` 移植到 **Orange Pi 5（RK3588S，Orange Pi 官方 Ubuntu）**。
  OPi5 临时同时承担 **AI 推理服务（已有）** 与 **本地安全闭环（新移植）**，二者保持**独立进程**。
- 红线不变：FSM 进程只依据本地传感器算 `state/risk_score`；AI 服务仍只返回 `risk_hint`；
  `control_allowed=false` 进程级隔离保留；FSM 绝不接受 AI 的执行器命令。

## 本轮目标

把 `edge/imx6ull-controller/` 的控制逻辑移植为 `edge/opi5-controller/`，在 OPi5 上**原生编译**
（无需交叉编译），完成 I2C/GPIO 重映射，并对 P0 逐项**重新实测出证据**。

---

## 0. 绝对禁止

```text
- 不向已死的 i.MX6ULL 部署/scp/ssh 执行任何东西（视为不可用）
- 不直接 push；每步先给 diff 摘要，等人工确认
- 不复用 i.MX 的旧测试记录冒充 OPi5 已通过（引脚/总线全变了，必须重测）
- 不让 AI 服务进程直接驱动任何执行器；control_allowed 恒 false
- 不接 220V；执行器不从 OPi5 排针取大电流，独立 5V + 星形共地
- 未板端实测的项一律标 [CLAUDE_CODE_TODO | VERIFY/MEASURE]，不得写"已通过"
- 不新增 docs/15；docs/00–14 结构冻结
```

---

## 1. 代码迁移（保留历史）

```text
- 用 git mv 或复制方式，把 edge/imx6ull-controller/ 的可复用源码落到 edge/opi5-controller/
  （imx_safetyd 主程序、safety_fsm、pca9685、event_client、spool、oled 等模块）
- 保留 imx6ull 目录为 legacy 证据，README 注明"i.MX 硬件故障，主控迁移至 OPi5"
- 不删历史
```

要点：
- 代码本是 sysfs/libgpiod + I2C 的标准 Linux C，移植主要是**改引脚号与总线号**，不是重写逻辑。
- FSM 状态转换、risk_score 融合规则、离线降级、spool、OLED 摘要逻辑**原样保留**。

## 2. 引脚 / 总线重映射

以 `opi5_controller_pinmap_and_i2c.md`（待并入 CANONICAL）为唯一事实源：

```text
- I2C：用 orangepi-config 开 overlay，i2cdetect 找到 0x40 所在 /dev/i2c-N，写入 inventory.yaml
- GPIO：用 wiringOP(next) `gpio readall` 与 libgpiod `gpioinfo/gpiofind` 取权威 line
- 把 i.MX 的 gpio117–124 / /dev/i2c-0 全部替换为 OPi5 实测值
- 代码里不写死号；通过 inventory.yaml / 配置传入，或集中到一个 pinmap 头文件
```

> 沿用语义：buzzer active low、直连 GPIO + NPN 不经 PCA9685；
> PCA9685 CH0/1 舵机、CH2-4 RGB、CH5 继电器、CH6 水泵 MOS；MQ-2 必分压。

## 3. 原生构建与运行

```text
- 在 OPi5 上原生 gcc 编译（记录依赖：libgpiod-dev、i2c 头文件等）
- 提供运行方式：先 --mode once 单轮自检，再 --mode loop
- 进程管理：OPi5 是 systemd，提供 systemd unit（与 AI 服务相互独立）
- AI URL 改本机回环 http://127.0.0.1:8080/api/infer/vision；Flask 仍指向 PC
```

## 4. P0 逐项重测（垂直切片，每片出证据）

每片"mock/自检 → 真实"，证据写入 `tests/opi5-controller/YYYY-MM-DD_*.md`：

```text
切片 1  I2C 通路：  i2cdetect 见 0x40 → pca9685_set_pwm 空载占空比可调
切片 2  火焰 flame：flame=1 → 本地直接 ALARM（纯本地，不依赖 AI/网络）
切片 3  MQ-2：      mq2=1   → 本地直接 ALARM；分压后进 GPIO 实测 ≤3.3V
切片 4  PIR：       pir=1   → VERIFY
切片 5  蜂鸣器：    ALARM → 蜂鸣器响；退出 all_off 不常响
切片 6  RGB：       NORMAL=绿 / VERIFY=蓝 / ALARM=红（PCA9685 CH2-4）
切片 7  舵机：      先空载 PWM，再接 MG90 扫描（CH0/CH1）
切片 8（可选）继电器/水泵：先 LED 验证 CH5/CH6，再接负载，水箱空喷
```

验收：每片测试记录含真实现象描述（电平/灯色/响停/角度），未达成的明确标 TODO，不美化。

## 5. 文档更新（最小、口径一致）

```text
- CANONICAL_DECISIONS.md：新增"OPi5 临时主控引脚映射"章节（并入 pinmap 草表），
  注明 i.MX gpio117-124/i2c-0 失效、迁移到 OPi5 实测值
- README / docs/00 / docs/04：架构层级里把"第一/二层物理节点 i.MX6ULL"
  更新为"i.MX 硬件故障，控制层临时整合到 OPi5（独立进程）"
- docs/13：答辩口径加一条——"末期 i.MX 供电故障，将 Linux 安全主控整合到 OPi5 作为
  独立特权进程，AI 仍与执行器隔离，本地安全闭环独立于 AI/网络"
- i.MX 既有 tests/imx6ull/*.md：保留为"在 i.MX 上完成、板卡现已故障、主控已迁移"的迭代证据
```

## 6. 回报格式

```text
[OPi5 主控移植结果]
1. 迁移文件：edge/opi5-controller/ 新增/改动清单
2. 引脚/总线实测：/dev/i2c-N=?  0x40 是否可见  各 GPIO line=?
3. 构建：原生编译是否通过、依赖清单
4. P0 重测：切片 1–8 各自状态（通过 / TODO / 未做）+ 证据文件路径
5. 文档更新：CANONICAL / README / docs/00 / docs/04 / docs/13 改动摘要
6. 严格未执行：未碰 i.MX、未 push、未让 AI 控执行器、未把未测项写成已通过
7. 下一步建议
```
