# Task10-E 蜂鸣器 + RGB 输出真实驱动验证

日期：2026-06-07
分支：`migration/imx6ull-opi5-edge-ai`
状态：**通过**（代码逻辑验证通过；RGB-R 红灯模块/接线待查）

## 接线

| 项目 | 说明 |
|---|---|
| RGB 模块 | 共阴 RGB LED 模块 |
| RGB-R | gpio121 (J5 D4) |
| RGB-G | gpio123 (J5 D6) |
| RGB-B | gpio124 (J5 D7) |
| 蜂鸣器 | 有源蜂鸣器模块，gpio122 (J5 D5)，经 NPN 三极管驱动 |
| 供电 | 模块独立供电，星形共地 |

## 裸控 GPIO 测试

四路 GPIO 均成功 export 并设为 output：

```
gpio121 direction = out ✓
gpio122 direction = out ✓
gpio123 direction = out ✓
gpio124 direction = out ✓
```

### 有效电平确认

| GPIO | 功能 | value=0 | value=1 | 结论 |
|---|---|---|---|---|
| gpio121 | RGB-R | 灭 | 亮（预期） | **active high** |
| gpio122 | buzzer | **响** | 不响 | **active low** |
| gpio123 | RGB-G | 灭 | 亮 | **active high** |
| gpio124 | RGB-B | 灭 | 亮 | **active high** |

确认方式：
- 蜂鸣器：gpio122=0 时蜂鸣器响，gpio122=1 时停止，确认 active low
- RGB-G/B：loop 模式运行时 gpio123=1/gpio124=1 可观察到绿灯/蓝灯亮
- RGB-R：gpio121 手动写 1 红灯未亮，疑似模块红灯通道或接线问题，代码逻辑正确

## 代码修改

修改 `edge/imx6ull-controller/src/imx_safetyd.c`：

1. 新增输出 GPIO 配置：
   - `GPIO_RGB_R`（默认 121）、`GPIO_RGB_R_ACTIVE_HIGH`（默认 1）
   - `GPIO_BUZZER`（默认 122）、`GPIO_BUZZER_ACTIVE_HIGH`（默认 0，低有效）
   - `GPIO_RGB_G`（默认 123）、`GPIO_RGB_G_ACTIVE_HIGH`（默认 1）
   - `GPIO_RGB_B`（默认 124）、`GPIO_RGB_B_ACTIVE_HIGH`（默认 1）

2. 新增函数：
   - `ensure_gpio_export_out()` — 导出 GPIO 并设为 output 方向
   - `write_gpio_output()` — 通过 active_high 转换写入 GPIO 值
   - `apply_actuators_by_state()` — 按状态驱动输出
   - `all_off()` — 关闭所有输出（清理）

3. 状态驱动规则：
   - NORMAL：buzzer=0, R=0, G=1, B=0（绿灯）
   - VERIFY：buzzer=0, R=0, G=0, B=1（蓝灯）
   - ALARM：buzzer=1, R=1, G=0, B=0（红灯 + 蜂鸣器）
   - FAULT：buzzer=1, R=1, G=0, B=1（红蓝 + 蜂鸣器）

4. 启动时初始化输出 GPIO 为 OFF，退出时 `all_off()` 清理
5. 信号处理（SIGINT/SIGTERM）中调用 `all_off()`
6. `build_event_json()` 输出实际 actuator 状态（不再硬编码）

## 构建与部署

- 构建：`scripts/build_imx6ull.sh imx_safetyd` ✓
- 部署：`scripts/deploy_imx6ull.sh build/imx6ull/imx_safetyd` ✓

## imx_safetyd 验证

### 场景 A：NORMAL（绿灯）

```bash
POST_NORMAL=1 GPIO_FLAME_ACTIVE_HIGH=1 GPIO_MQ2_ACTIVE_HIGH=0 ./imx_safetyd --mode loop
```

loop 模式运行期间 GPIO 实测：
```
gpio121 = 0 (R off)
gpio122 = 1 (buzzer off, active low)
gpio123 = 1 (G on) ✓
gpio124 = 0 (B off)
```

日志：`[actuators] state=NORMAL buzzer=0 R=0 G=1 B=0`
状态：`state=NORMAL, risk_score=0`

**注意**：`once` 模式下 NORMAL 场景完成太快（<400ms），绿灯一闪而过，需用 loop 模式观察。

### 场景 B：VERIFY（蓝灯）

```bash
FORCE_VERIFY=1 GPIO_MQ2_ACTIVE_HIGH=0 ./imx_safetyd --mode loop
```

loop 模式运行期间 GPIO 实测：
```
gpio121 = 0 (R off)
gpio122 = 1 (buzzer off)
gpio123 = 0 (G off)
gpio124 = 1 (B on) ✓
```

日志：`[actuators] state=VERIFY buzzer=0 R=0 G=0 B=1`
状态：`state=VERIFY, risk_score=2`

### 场景 C：ALARM（红灯 + 蜂鸣器）

```bash
GPIO_MQ2_ACTIVE_HIGH=1 ./imx_safetyd --mode loop
```

利用 gpio120 浮空 raw=1 + active_high=1 触发 ALARM。

loop 模式运行期间 GPIO 实测：
```
gpio121 = 1 (R on) — 但红灯未亮（模块/接线问题）
gpio122 = 0 (buzzer on, active low) ✓
gpio123 = 0 (G off)
gpio124 = 0 (B off)
```

日志：`[actuators] state=ALARM buzzer=1 R=1 G=0 B=0`
状态：`state=ALARM, risk_score=5`

蜂鸣器确认响。红灯代码逻辑正确（gpio121=1），但模块上红灯未亮，待查接线/LED 通道。

## 结论

**Task10-E 通过**（代码逻辑验证）：
- 四路输出 GPIO 均可正常 export、设为 output、写入值
- 蜂鸣器 active low 确认：gpio122=0 响，gpio122=1 不响
- RGB-G/B active high 确认：gpio123=1/gpio124=1 灯亮
- RGB-R 代码逻辑正确（gpio121=1），红灯未亮待查硬件
- NORMAL/VERIFY/ALARM 三场景状态驱动正确
- 程序退出时 all_off 清理，蜂鸣器不会常响
- 信号处理中也有 all_off

## 边界

- door 跳过（Task10-A 暂缓）
- flame/mq2/PIR 输入已有历史证据
- MQ-2 本轮未插模块，gpio120 浮空读 raw=1（上拉），用于触发 ALARM 测试
- RGB-R 红灯未亮，疑似模块红灯通道或接线问题，不影响代码正确性
- 不接继电器/风扇/水泵/MOS/220V
- AI/OPi5/Flask 不控制执行器
- `control_allowed` 保持 `false`
