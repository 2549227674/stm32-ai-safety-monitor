# 06 状态机与联动机制（OPi5 主线版）

## 6.1 状态定义

| 状态 | 含义 | 进入条件示例 | 执行器策略 |
|---|---|---|---|
| `NORMAL` | 正常巡检 | 无异常传感器，AI 无风险 | 执行器 OFF，绿灯 |
| `WARN` | 轻微异常 | 单个低风险传感器触发 | 蜂鸣短响/记录事件 |
| `VERIFY` | 复核中 | 本地传感器触发，需要视觉确认 | 抓拍、请求 AI，执行器保持安全态 |
| `ALARM` | 报警 | 火焰/烟雾等组合风险高 | 蜂鸣器响 + RGB 红灯 + Water MOS |
| `FAULT` | 故障/降级 | 摄像头、AI、网络异常 | 本地降级，记录故障，不崩溃 |

## 6.2 传感器输入定义

| 字段 | 类型 | 含义 | OPi5 引脚 |
|---|---|---|---|
| `pir` | 0/1 | 人体红外 | pin13 / gpio139 |
| `flame` | 0/1 | 火焰检测 | pin15 / gpio28 |
| `mq2` | 0/1 | 烟雾/气体 | pin19 / gpio49 |

## 6.3 视觉输入定义

来自本机 `opi5_ai_service`：

- `objects[]`：目标检测结果，如 person/fire/smoke。
- `risk_hint`：0–10 的 AI 风险提示。
- `summary`：自然语言摘要。
- `control_allowed`：必须为 `false`。

## 6.4 `risk_score` 规则

| 条件 | 加分 |
|---|---|
| `pir=1` | +2 |
| `flame=1` | +6 |
| `mq2=1` | +5 |
| AI 检测到 person | +1 |
| AI 检测到 smoke/fire | +3 |
| AI 服务不可达但本地传感器异常 | +1 |

最终：`risk_score = clamp(local_score + ai_adjust, 0, 10)`。

## 6.5 AI `risk_hint` 接入规则

1. AI 只作为风险提示，不是执行器控制源。
2. 若 `control_allowed != false`，本地安全主控进程必须忽略该字段并记录协议异常。
3. 若 AI 超时，状态机继续使用本地传感器结果。
4. 若本地传感器强报警，AI 只能增强证据，不降低报警级别。
5. 若仅 AI 检测到 person，但本地传感器无异常，状态最多进入 `WARN`。

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

| 状态 | buzzer | RGB | Water MOS | Pan |
|---|---|---|---|---|
| NORMAL | OFF | 绿灯 | OFF | 保持 |
| WARN | 间歇短响 | 黄灯 | OFF | 保持 |
| VERIFY | OFF/短响 | 蓝灯 | OFF | 保持 |
| ALARM | ON | 红灯 | ON | 可选扫描 |
| FAULT | 间歇 | 紫/黄 | OFF | 保持 |

OPi5 当前执行器引脚：

| 执行器 | 引脚 | 有效电平 |
|---|---|---|
| buzzer | pin26 / gpio35 | active LOW |
| RGB-R | pin21 / gpio48 | active HIGH |
| RGB-G | pin23 / gpio50 | active HIGH |
| RGB-B | pin24 / gpio52 | active HIGH |
| Water MOS | pin11 / gpio138 | active HIGH |
| Pan servo | pin7 / PWM15 | PWM 脉宽 |

AI 服务 / Flask / 前端不控制执行器；只有本地安全主控进程 `opi5_safetyd` 控制执行器，`control_allowed=false`。

## 6.8 防误报策略

- PIR 去抖：连续 N 次采样一致才生效。
- 火焰/烟雾：短时间内重复确认；若强触发则优先报警。
- AI 结果：置信度低于阈值只记录，不提升到 ALARM。

## 6.9 离线降级

| 故障 | 降级行为 |
|---|---|
| `opi5_ai_service` 不可达 | 跳过 AI，继续本地状态机，记录 `ai_status=offline` |
| Flask 不可达 | 本地 spool 事件，稍后补发 |
| 摄像头不可用 | 不抓拍，保留传感器事件 |

## 6.10 OPi5 进程伪代码

```c
while (running) {
    sensors = read_gpio_inputs();          // PIR/flame/MQ-2
    local_score = calc_local_score(sensors);
    state = update_fsm_pre_ai(state, sensors, local_score);

    if (state_need_snapshot(state)) {
        frame = capture_frame();            // USB 摄像头 or mock
        ai = opi5_ai_infer(frame, timeout); // 调用本机 AI 服务
    } else {
        ai = no_ai_result();
    }

    risk_score = fuse_local_and_ai(local_score, ai.risk_hint);
    state = update_fsm_post_ai(state, sensors, ai, risk_score);
    apply_actuators_by_state(state);        // buzzer/RGB/MOS/Pan
    report_to_flask(state, sensors, actuators, ai);
    sleep_until_next_cycle();
}
```

## 6.11 历史 i.MX 阶段摘要（已完成验证，不再部署）

> 本节为历史 i.MX 阶段的状态机记录，保留为工程迭代证据。

历史 i.MX 使用 C 版 `imx_safetyd`，GPIO 通过 J5 引出（gpio117-124），执行器包括 buzzer(gpio122)、RGB(gpio121/123/124)、PCA9685 CH5 继电器、PCA9685 CH6 水泵。Task10/Task11 已验证 P0 传感器/执行器和 P1 OLED/继电器/水泵。

历史代码位于 `edge/imx6ull-controller/`，不再部署。详见 `docs/archive/2026-imx6ull-stage/`。
