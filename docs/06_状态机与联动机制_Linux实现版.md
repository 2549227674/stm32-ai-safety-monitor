# 06 状态机与联动机制（Linux 实现版）

## 6.1 状态定义

| 状态 | 含义 | 进入条件示例 | 执行器策略 |
|---|---|---|---|
| `NORMAL` | 正常巡检 | 无异常传感器，AI 无风险 | 执行器 OFF，绿灯可选 |
| `WARN` | 轻微异常 | 单个低风险传感器触发或 AI `risk_hint` 较低 | 蜂鸣短响/记录事件，可抓拍 |
| `VERIFY` | 复核中 | 本地传感器触发，需要视觉确认 | 抓拍、请求 OPi5，执行器保持安全态 |
| `ALARM` | 报警 | 火焰/烟雾/未知人员等组合风险高 | 蜂鸣器/RGB/风扇等按规则动作 |
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

| 状态 | buzzer | RGB | fan | pump | relay |
|---|---|---|---|---|---|
| NORMAL | 0 | G | 0 | 0 | 0 |
| WARN | 间歇 | Y | 0/按需 | 0 | 0 |
| VERIFY | 0/短响 | B | 0 | 0 | 0 |
| ALARM | 1 | R | 按规则 | 按规则 | 按规则 |
| FAULT | 间歇 | 紫/黄 | 安全态 | 0 | 0 |

真实执行器是否接入、GPIO 与负载能力必须由 Task03 实测决定。

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
