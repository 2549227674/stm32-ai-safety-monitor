# Task10-A 门磁首切片 — 跳过/暂缓

日期：2026-06-07
分支：`migration/imx6ull-opi5-edge-ai`
状态：**跳过/暂缓**（未通过）

## 器件与接线

| 项目 | 说明 |
|---|---|
| 器件 | 两线常开门磁 / 干簧管 |
| 接线 | 一端接 J5 D1 / CSI_DATA1 / gpio118，另一端接 GND |
| 外部上拉 | 未使用 10k 外部上拉 |
| 内部上拉 | 寄存器 0x00008030 显示 47K 内部上拉已启用（PUS=01, PUE=1, PKE=1） |

## 裸读测试

### 测试 1：gpio118 导出与初始读取

```bash
echo 118 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio118/direction
cat /sys/class/gpio/gpio118/value
# 结果: 0
```

### 测试 2：20 秒循环读取（门磁接 GND）

- 用户按 3 秒靠近 / 3 秒远离节奏操作磁铁
- 100 个采样（200ms 间隔），全部读到 0
- 磁铁远离时未能读到 1

### 测试 3：手动驱动高后释放

```bash
echo out > /sys/class/gpio/gpio118/direction
echo 1 > /sys/class/gpio/gpio118/value
cat /sys/class/gpio/gpio118/value  # 结果: 1
echo in > /sys/class/gpio/gpio118/direction
sleep 0.5
cat /sys/class/gpio/gpio118/value  # 结果: 1 (短暂)
# 后续快速读取: 1,0,0,0,0,0,0,0,0,0
```

GPIO 能读到 1，但释放后约 200ms 内回落到 0，说明 47K 内部上拉不足以维持高电平。

### 测试 4：门磁改接 3.3V

用户尝试门磁一端接 3.3V、一端接 gpio118：
- 磁铁靠近（开关闭合）→ gpio118 接 3.3V → 部分读到 1，但噪声大
- 磁铁远离（开关断开）→ gpio118 悬空 → 0/1 随机跳动
- 内部上拉在开关断开时把引脚拉高，与预期"断开=0"矛盾

## 结论

- gpio118 引脚本身可读 0/1（手动驱动高验证通过）
- 无外部上拉时，最简接法（门磁接 GND）裸读不稳定
- 内部 47K 上拉不足以在门磁断开时将引脚可靠拉到 1
- 需要外部 10k 上拉电阻才能获得干净 0/1 信号
- 用户决定本轮跳过门磁，不继续补 10k 排查

## 决策

**Task10-A 未通过，状态为"跳过/暂缓"。**

- 未修改 `imx_safetyd.c` door 读取逻辑
- door 不写成已通过
- 后续如需启用门磁，需添加 10k 外部上拉电阻（gpio118 → 3.3V）

## 边界

- 仅验证 door（gpio118）
- flame / mq2 / PIR raw=1 / buzzer / RGB 未验证
- 不接继电器 / 风扇 / 水泵 / MOS / 220V
- AI / OPi5 / Flask 不控制执行器
- `control_allowed` 保持 `false`
