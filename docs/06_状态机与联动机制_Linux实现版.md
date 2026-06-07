# 06 状态机与联动机制（Linux 实现版）

## 6.1 状态定义

| 状态 | 含义 | 进入条件示例 | 执行器策略 |
|---|---|---|---|
| `NORMAL` | 正常巡检 | 无异常传感器，AI 无风险 | 执行器 OFF，绿灯可选 |
| `WARN` | 轻微异常 | 单个低风险传感器触发或 AI `risk_hint` 较低 | 蜂鸣短响/记录事件，可抓拍 |
| `VERIFY` | 复核中 | 本地传感器触发，需要视觉确认 | 抓拍、请求 OPi5，执行器保持安全态 |
| `ALARM` | 报警 | 火焰/烟雾/未知人员等组合风险高 | 蜂鸣器/RGB/继电器/水泵按规则动作 |
| `FAULT` | 故障/降级 | 摄像头、AI、网络、传感器异常 | 本地降级，记录故障，不崩溃 |

## 6.2 传感器输入定义

| 字段 | 类型 | 含义 | 默认值 |
|---|---|---|---|
| `door` | 0/1 | 门磁 | 0 |
| `pir` | 0/1 | 人体红外 | 0 |
| `flame` | 0/1 | 火焰数字输入 | 0 |
| `mq2` | 0/1 或等级 | 烟雾/气体 | 0 |
| `button` | 0/1 | 调试/消警按键 | 0 |
| `mpu_rms` | float | 可选振动 RMS | null |

## 6.3 视觉输入定义

来自 OPi5：

- `objects[]`：目标检测结果，如 person/fire/smoke。
- `faces[]`：人脸识别结果，known/unknown。
- `risk_hint`：0–10 的 AI 风险提示。
- `summary`：自然语言摘要。
- `control_allowed`：必须为 `false`。

## 6.4 `risk_score` 规则

> **[ASSUMPTION]** 初始阈值采用课程演示级规则，后续根据实测误报率调整。 —— 若不成立，对应 [CLAUDE_CODE_TODO] 处理。


| 条件 | 加分 |
|---|---|
| `door=1` | +2 |
| `pir=1` | +2 |
| `flame=1` | +6 |
| `mq2=1` | +5 |
| AI 检测到 person | +1 |
| AI 检测到 unknown face | +2 |
| AI 检测到 smoke/fire | +3 |
| OPi5 不可达但本地传感器异常 | +1 |

最终：`risk_score = clamp(local_score + ai_adjust, 0, 10)`。

> **[CLAUDE_CODE_TODO | MEASURE]** 根据实测误报率调整 risk_score 阈值
> - 为何 GPT 给不了：沙箱无法采集真实传感器误报、光照、烟雾模拟和人员走动数据。
> - 期望产物/操作：联调阶段记录不同场景 risk_score 与误报，调整加分表。
> - 回填位置：docs/06 第 6.4；tests/integration
> - 验收：至少记录 NORMAL/WARN/ALARM 三类场景，并说明阈值调整。


## 6.5 AI `risk_hint` 接入规则

1. AI 只作为风险提示，不是执行器控制源。
2. 若 `control_allowed != false`，i.MX6ULL 必须忽略该字段并记录协议异常。
3. 若 AI 超时，状态机继续使用本地传感器结果。
4. 若本地传感器强报警，AI 只能增强证据，不降低报警级别。
5. 若仅 AI 检测到 person，但本地传感器无异常，状态最多进入 `WARN` 或记录视觉事件。

## 6.6 状态转换表

| 当前 | 条件 | 下一状态 |
|---|---|---|
| NORMAL | `risk_score` 0–2 | NORMAL |
| NORMAL | `risk_score` 3–4 | WARN |
| WARN | `need_snap=true` | VERIFY |
| VERIFY | AI 返回低风险且本地异常消失 | NORMAL/WARN |
| VERIFY | `risk_score >= 6` | ALARM |
| 任意 | 火焰/烟雾本地强触发 | ALARM |
| 任意 | 摄像头/AI/网络连续失败 | FAULT 或降级 WARN |
| ALARM | 人工消警且传感器恢复 | NORMAL |

## 6.7 执行器策略

| 状态 | buzzer | RGB | pump | relay |
|---|---|---|---|---|
| NORMAL | 0 | G | 0 | 0 |
| WARN | 间歇 | Y | 0 | 0 |
| VERIFY | 0/短响 | B | 0 | 0 |
| ALARM | 1 | R | 按规则 | 按规则 |
| FAULT | 间歇 | 紫/黄 | 0 | 0 |

P0 起**真实驱动蜂鸣器（gpio122 + 三极管）与 RGB（gpio121/123/124）**：NORMAL=绿、VERIFY=蓝、ALARM=红+蜂鸣器响、FAULT=紫/黄间歇。继电器/水泵仍为 P1/P2，未真实驱动。AI/OPi5/Flask 不控制执行器，`control_allowed=false`。

> **[CLAUDE_CODE_TODO | MEASURE]** 标定 P0 误报与阈值
> - 期望产物/操作：分别记录门开关、人体触发、打火机近火焰、烟雾近 MQ-2 四类场景的 sensors 与 risk_score；按误报调整加分。
> - 回填位置：docs/06 第 6.4；tests/imx6ull。
> - 验收：至少记录 NORMAL/VERIFY/ALARM 三类场景。

## 6.8 防误报策略

- 门磁/PIR 去抖：连续 N 次采样一致才生效。
- 火焰/烟雾：短时间内重复确认；若强触发则优先报警。
- AI 结果：置信度低于阈值只记录，不提升到 ALARM。
- 人脸：unknown 不等于危险，只能与 armed/PIR/门磁组合提高风险。

## 6.9 离线降级

| 故障 | 降级行为 |
|---|---|
| OPi5 不可达 | 跳过 AI，继续本地状态机，记录 `ai_status=offline` |
| Flask 不可达 | 本地 spool 事件，稍后补发 |
| 摄像头不可用 | 不抓拍，保留传感器事件 |
| PCA9685 不可用 | 云台禁用，固定摄像头或跳过 |

## 6.10 Linux 进程伪代码

```c
while (running) {
    sensors = read_gpio_inputs();
    local_score = calc_local_score(sensors);
    state = update_fsm_pre_ai(state, sensors, local_score);

    if (state_need_snapshot(state)) {
        frame = v4l2_capture();
        ai = opi5_infer_with_timeout(frame, 1500);
    } else {
        ai = no_ai_result();
    }

    risk_score = fuse_local_and_ai(local_score, ai.risk_hint);
    state = update_fsm_post_ai(state, sensors, ai, risk_score);
    apply_actuators_by_state(state);
    post_event_or_spool(state, sensors, actuators, ai);
    sleep_until_next_cycle();
}
```

## 6.11 Task07-C C 版 imx_safetyd 落地状态

Task07-C 已落地 C 版主控程序 `edge/imx6ull-controller/src/imx_safetyd.c`，当前实现范围：

- 支持 `--mode once`、`--mode loop`、`--mode flush`。
- 通过 sysfs fallback 读取真实 PIR `gpio117`；本轮只测到空闲 `raw=0`，VERIFY 场景使用 `--force-verify 1`，未把 PIR 人工触发写成已通过。
- 实现 NORMAL/VERIFY/FAULT 最小状态机：`pir=0 -> NORMAL`，`pir=1 -> VERIFY`，GPIO 读取失败可记录 FAULT/diagnostics。
- VERIFY 时调用 `v4l2-ctl` 抓拍，再调用 `curl` 请求 OPi5 mock AI。
- AI 正常时按 `risk_score = min(10, 3 + risk_hint)` 融合；OPi5 不可达时生成干净 fallback `ai_result.ok=false/mode=offline/control_allowed=false`，`risk_score=4`。
- Flask 不可达时写入 `/opt/edge-ai-safety-monitor/spool/imx-safetyd/pending/`，`flush` 模式恢复后移动到 `sent/`。
- 每次运行写 `/opt/edge-ai-safety-monitor/run/imx_safetyd_status.json` 和 `/opt/edge-ai-safety-monitor/run/imx_safetyd.log`。
- 当前 door/flame/mq2 固定为 0，actuators 仍是安全态事件字段；**Task10 将替换为真实 GPIO 读取与真实驱动**。
- 在当前非 systemd 镜像上，可由 BusyBox/SysV `init.d/S99imx-safetyd` 管理进程；`/etc/init.d/rcS` 会遍历 `S??*`，因此具备开机自启条件，但需板端本地 `imx-safetyd.env`。

证据见 `tests/integration/2026-06-07_imx_safetyd_c.md`。

## 6.12 Task10 P0 传感器/执行器真实化（待执行）

Task10 将把 imx_safetyd 中 door/flame/mq2 的固定 0 替换为真实 GPIO 读取，并真实驱动蜂鸣器（gpio122）与 RGB（gpio121/123/124）。引脚分配以 `CANONICAL_DECISIONS.md` 第 0.6 节为唯一事实源。

关键规则：
- `flame=1 或 mq2=1 → 任意状态直接 ALARM`，该判定纯本地、不经网络/AI。
- `risk_score` 仍为 0–10 量纲：door=+2、pir=+2、flame=+6、mq2=+5，`risk_score = clamp(local_score + ai_adjust, 0, 10)`。
- 蜂鸣器必须直连 GPIO + 三极管驱动，不经 PCA9685。
- 继电器/水泵仍为 P1/P2，不在 Task10 范围。
- AI/OPi5/Flask 不控制执行器，`control_allowed` 保持 `false`。

详见 `CLAUDE_CODE_TASK_10_imx6ull_sensor_actuator_p0.md`。

### 6.12.1 Task10-B 火焰传感器验证（已通过）

Task10-B 已验证火焰传感器 flame DO 接入：
- gpio119（J5 D2），3.3V 供电，DO 高有效（raw=1 有火焰）
- flame=1 时本地直接进入 ALARM，risk_score=6，纯本地不依赖 AI/network
- AI offline 不覆盖本地 risk_score（已修复原有 fallback bug）
- 传感器灵敏度电位器后续需调整，确保无火焰时稳定读 0
- 证据见 `tests/imx6ull/2026-06-07_p0_flame_sensor.md`

### 6.12.2 Task10-C MQ-2 烟雾传感器验证（已通过）

Task10-C 已验证 MQ-2 DO 接入：
- gpio120（J5 D3），5V 供电 + DO 直连（课设短时方案，长期建议分压/电平转换）
- mq2=1 时本地直接进入 ALARM，risk_score=5，纯本地不依赖 AI/network
- flame=1 和 mq2=1 可叠加，risk_score clamp 到 10
- 需先 unbind ecspi1 驱动才能导出 gpio120
- 证据见 `tests/imx6ull/2026-06-07_p0_mq2_sensor.md`

### 6.12.3 Task10-D PIR 真实触发（沿用历史证据）

Task10-D 不重复测试，沿用 Task03-A gpio117 历史实测证据：空闲 raw=0、触发 raw=1。
- 证据见 `tests/imx6ull/2026-06-06_gpio_input_probe.md`、`tests/imx6ull/2026-06-07_p0_pir_reuse_task03a.md`

### 6.12.4 Task10-E 蜂鸣器 + RGB 输出真实驱动（已通过）

Task10-E 已验证蜂鸣器与 RGB 输出真实驱动：
- buzzer gpio122：**active low**（0=响，1=不响），经 NPN 三极管驱动
- RGB-R gpio121：active high（1=亮），红灯模块/接线待查
- RGB-G gpio123：active high（1=亮），绿灯正常
- RGB-B gpio124：active high（1=亮），蓝灯正常
- 状态驱动规则：NORMAL=绿灯、VERIFY=蓝灯、ALARM=红灯+蜂鸣器、FAULT=红蓝+蜂鸣器
- 程序退出时 all_off 清理，蜂鸣器不会常响
- `once` 模式下 NORMAL 场景完成太快（<400ms），需用 loop 模式观察灯色
- 证据见 `tests/imx6ull/2026-06-07_p0_buzzer_rgb_outputs.md`

### 6.12.5 Task10-E2 RGB PCA9685 PWM 视觉增强（已通过）

Task10-E2 将 RGB 状态灯从 J5 GPIO 增强为 PCA9685 CH2/CH3/CH4 PWM 输出：
- PCA9685 CH2=RGB-R, CH3=RGB-G, CH4=RGB-B（CH0/CH1 舵机保持不动）
- 蜂鸣器仍走 gpio122 active low，不经 PCA9685
- 新增 `pca9685_set_pwm` 工具，支持单通道 duty 设置，自动唤醒 PCA9685
- `RGB_BACKEND=gpio|pca9685` 可选，演示推荐 pca9685（亮度更高）
- PCA9685 全局频率 50Hz 不变（不影响舵机）
- RGB PCA9685 是视觉增强，不承担安全报警核心；蜂鸣器仍是 ALARM 主输出
- 证据见 `tests/imx6ull/2026-06-07_p0_rgb_pca9685_pwm.md`

## 6.13 Task11 P1 OLED / 继电器 / SoC 温度 / 隔离水箱（待执行）

P1 不改变 P0 本地安全闭环的基本原则，只增强显示、低压执行和设备热健康。引脚分配以 `CANONICAL_DECISIONS.md` 第 0.7 节为唯一事实源。

### OLED 显示

Task11-A 已验证：OLED SSD1306（/dev/i2c-0 addr 0x3C）每轮显示 `state / risk_score / sensors / actuators` 摘要。断网、断 AI、Flask 不可达时仍刷新，不影响主控程序。OLED_ENABLE=0|1 可选。证据见 `tests/imx6ull/2026-06-07_p1_oled_status_screen.md`。

### 继电器/水泵联动

Task11-B 已验证：KY-019 5V 继电器模块接 PCA9685 CH5，active high（duty=4095 吸合），默认 OFF。

| 条件 | relay | pump |
|---|---|---|
| `flame=1` → ALARM | 动作（已验证） | 开（喷淋，隔离水箱通过后） |
| `mq2=1` → ALARM | 动作（已验证） | 不强制 |
| PIR/VERIFY | OFF | 不动作 |
| NORMAL | OFF | OFF |
| FAULT | OFF（避免误动作） | OFF |

解除/恢复：传感器复位且人工消警后执行器回默认 OFF。RELAY_ENABLE=0|1 可选。证据见 `tests/imx6ull/2026-06-07_p1_relay_ch5.md`。

### SoC 温度设备热健康

- `soc_temp` 过高 → `device_health=WARN` → 上报 Dashboard/OLED 警告。
- `soc_temp` **不升级为火灾 ALARM**。
- `soc_temp` 是主控芯片温度，不是环境温度，不产生 `humidity` 字段。
- SoC 温度仅作监测上报，不驱动执行器。

### 安全红线

- AI/OPi5/Flask 不驱动 relay/pump；`control_allowed=false`。
- PCA9685 只接受 i.MX 本地状态机控制。
- 不接 220V；水与板卡物理隔离。

详见 `CLAUDE_CODE_TASK_11_imx6ull_p1_oled_relay_soctemp_pump.md`。

## 6.14 Task07-D 后续原生化增强（可选，不阻塞 MVP）

Task07-C 当前已实现 C 版 imx_safetyd，支持 once/loop/flush、真实 gpio117 空闲读取、NORMAL/VERIFY、OPi5 offline fallback、Flask spool/flush。

当前实现仍通过 v4l2-ctl/curl 子进程完成抓拍和 HTTP 通信。后续若时间充裕，可进入 Task07-D：

- D1：原生 libcurl HTTP client。
- D2：原生 V4L2 抓拍。
- D3：C 模块化拆分。

Task07-D 不改变状态机规则，只提升实现方式、错误诊断和长期运行可靠性。
