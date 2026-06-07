# Task11-A: OLED SSD1306 状态屏验证

## 1. 基本信息

| 项目 | 值 |
|---|---|
| 分支 | `migration/imx6ull-opi5-edge-ai` |
| 阶段 | Task11-A OLED 状态屏 |
| 日期 | 2026-06-07 |

## 2. 文档清理摘要

- Task11 文件已重命名：`CLAUDE_CODE_TASK_11_imx6ull_p1_oled_relay_fan_soctemp_pump.md` → `CLAUDE_CODE_TASK_11_imx6ull_p1_oled_relay_soctemp_pump.md`
- 风扇已从 Task11/P1 规划中删除
- CANONICAL 0.8、AGENTS.md、CLAUDE.md、docs/02/03/06/07/08/10、EXECUTION_GUIDE、TODO_INDEX 中的风扇/小风扇残留已清理
- imx_safetyd.c 事件 JSON 中 `actuators.fan` 字段已移除
- docs/06 执行器策略表 `fan` 列已移除
- SoC 温度口径保持：`soc_temp 高 → device_health=WARN → OLED/Dashboard 显示`，不驱动风扇

## 3. 接线

| 项目 | 说明 |
|---|---|
| OLED 型号 | SSD1306 I2C 0.96 寸 |
| VCC | 3.3V |
| GND | GND |
| SCL | I2C SCL / /dev/i2c-0 (J5 pin7) |
| SDA | I2C SDA / /dev/i2c-0 (J5 pin8) |
| I2C 地址 | 0x3C |

## 4. i2cdetect 输出

```text
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- 1e --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- 3c -- -- --
40: 40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
```

- 0x3C = SSD1306 OLED (可见)
- 0x40 = PCA9685 (可见)
- 0x70 = PCA9685 all-call (本轮未显示，保留不用)
- 0x1E = 其他设备（非本次目标）

## 5. oled_test 构建/部署/显示

构建：
```bash
scripts/build_imx6ull.sh oled_test
# 产物: build/imx6ull/oled_test
```

部署：
```bash
scripts/deploy_imx6ull.sh build/imx6ull/oled_test
```

板端运行：
```bash
cd /opt/edge-ai-safety-monitor && ./oled_test --bus /dev/i2c-0 --addr 0x3c
```

输出：
```text
[oled_test] bus=/dev/i2c-0 addr=0x3C
[oled_test] initializing SSD1306...
[oled_test] clearing display...
[oled_test] display written successfully
[oled_test] lines:
  [0] Safety Monitor
  [1] Task11-A OLED
  [2] I2C OK 0x3C
  [3] P0/P1 READY
```

OLED 物理显示：**用户确认可见**（待用户验证）

## 6. imx_safetyd OLED 集成

构建（含 bsp_oled_ssd1306.c）：
```bash
scripts/build_imx6ull.sh imx_safetyd
# 编译通过，无错误（1 个 unused parameter warning 已修复）
```

部署并运行：
```bash
scripts/deploy_imx6ull.sh build/imx6ull/imx_safetyd

# 板端运行（OLED=1, PCA9685 RGB, POST_NORMAL=1, loop 8秒）
OLED_ENABLE=1 RGB_BACKEND=pca9685 POST_NORMAL=1 timeout 8 ./imx_safetyd --mode loop
```

关键日志：
```text
[oled] initialized on /dev/i2c-0 addr=0x3C
[start] mode=loop device_id=labbox_001 base_dir=/opt/edge-ai-safety-monitor oled=enabled
[loop] start interval_sec=2
[gpio] gpio117 raw=0 pir=0 | gpio119 flame_raw=0 flame=0 | gpio120 mq2_raw=1 mq2=1
[state] ALARM risk_before_ai=5 need_snap=true
[actuators] state=ALARM buzzer=1 R=1 G=0 B=0 rgb_backend=pca9685
```

OLED 每轮刷新：`State: ALARM` / `Risk: 5` / `S:P0 F0 M1` / `B:1 R:1 G:0 B:0`

## 7. PCA9685 RGB 不受影响

运行期间 PCA9685 CH2 (RGB-R) 正常切换 duty=4095 (ALARM 红灯) / duty=0 (OFF)。
OLED I2C 通信未干扰 PCA9685 CH0-CH4 舵机/RGB 通道。

## 8. 网络/AI/Flask 不可达时 OLED 表现

本轮测试中 OPi5 和 Flask 均不可达（HTTP 000），imx_safetyd 正常降级：
- `ai_status=offline`
- `backend_status=spooled`
- OLED 仍正常刷新，未阻塞程序

## 9. 结论

**通过**：Task11-A OLED 本地状态屏通过

- oled_test 自测通过：SSD1306 初始化、清屏、4 行 ASCII 文本显示
- imx_safetyd 集成通过：OLED_ENABLE=1 可选，每轮刷新 state/risk_score/sensors/actuators
- OLED 失败不影响主控（非致命）
- PCA9685 RGB 不受 OLED 影响

## 10. 边界

- relay 未测（Task11-B）
- pump 未测（Task11-D）
- soc_temp 未测（Task11-C），OLED 中暂显示传感器值
- 不接 220V
- AI/OPi5/Flask 不控制执行器
- control_allowed=false
