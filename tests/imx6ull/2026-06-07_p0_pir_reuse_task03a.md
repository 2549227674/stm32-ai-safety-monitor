# Task10-D PIR 真实触发 — 沿用 Task03-A 历史证据

日期：2026-06-07
分支：`migration/imx6ull-opi5-edge-ai`
状态：**沿用历史证据，本轮不重复测试**

## 决策

用户决定跳过 Task10-D PIR 现场重复测试，直接沿用 Task03-A 已有实测证据。

## 历史证据来源

文件：`tests/imx6ull/2026-06-06_gpio_input_probe.md`

### 接线

| 项目 | 记录 |
|---|---|
| 输入源 | PIR / HC-SR501 |
| PIR VCC | J5 pin 1 VDD_5V |
| PIR GND | J5 pin 2 GND |
| PIR OUT/DO | J5 pin 15 D0 / CSI_DATA0 / gpio117 |

### 实测结果

| 项目 | 结果 |
|---|---|
| 空闲值 | raw=0 |
| 触发值 | raw=1 |
| 稳定性 | 是，连续读到 0/1 变化 |
| 测试命令 | `./gpio_test --sysfs 117 --watch` |

### 实测输出摘要

```
seq=0..11 gpio=117 value=0
seq=12..17 gpio=117 value=1
seq=18..39 gpio=117 value=0
seq=40..45 gpio=117 value=1
seq=46..56 gpio=117 value=0
```

## 本轮操作

- 未执行板端命令
- 未修改 imx_safetyd.c（PIR 逻辑已在 Task07-C 中实现）
- 仅新增本决策记录并回填文档状态

## 边界

- 本轮未重新执行板端 GPIO 测试
- 本轮未新增代码修改
- door：Task10-A 本轮跳过
- flame：Task10-B 已通过
- mq2：Task10-C 已通过
- buzzer / RGB：仍待验（Task10-E）
- 不接继电器 / 风扇 / 水泵 / MOS / 220V
- AI / OPi5 / Flask 不控制执行器
- `control_allowed` 保持 `false`
