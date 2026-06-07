# Task10-B 火焰传感器 flame DO 验证

日期：2026-06-07
分支：`migration/imx6ull-opi5-edge-ai`
状态：**通过**（代码逻辑验证通过；传感器硬件需后续调灵敏度）

## 接线

| 项目 | 说明 |
|---|---|
| 器件 | 火焰传感器模块（带 DO/AO 输出） |
| VCC | 3.3V（i.MX6ULL） |
| GND | GND |
| DO | J5 D2 / CSI_DATA2 / gpio119 |
| AO | 未使用 |
| 供电 | 3.3V，未使用 5V |

## 裸读测试

### 第一轮：打火机触发（成功）

用户按 5s 无火焰 / 5s 有火焰 / 5s 移开 / 5s 再触发节奏操作。

100 个采样（200ms 间隔）：

| 时间 | 状态 | flame_raw |
|---|---|---|
| 0-7.4s | 有火焰 | 全 1 |
| 7.4-10.4s | 无火焰 | 全 0 |
| 10.4-12.0s | 短触发 | 1→0 |
| 12.0-15.0s | 无火焰 | 全 0 |
| 15.0-18.6s | 有火焰 | 全 1 |
| 18.6-20s | 无火焰 | 全 0 |

结论：3.3V 供电下裸读稳定，**flame_active_high = 1**（raw=1 有火焰，raw=0 无火焰）。

### 第二轮：传感器卡在 1

裸读测试后传感器持续输出 1，30 秒内未恢复到 0。可能原因：
- 灵敏度电位器未调好
- 附近红外干扰
- 模块自锁特性

通过 `GPIO_FLAME_ACTIVE_HIGH=0` 反转逻辑模拟火焰触发，完成代码验证。

## 代码修改

修改 `edge/imx6ull-controller/src/imx_safetyd.c`：

1. 新增 `GPIO_FLAME`（默认 119）和 `GPIO_FLAME_ACTIVE_HIGH`（默认 1）配置
2. `read_gpio()` 新增 flame GPIO 读取，失败时 flame=0 且记录 diagnostics
3. `compute_state()` 新增强触发规则：`flame=1 → ALARM`，risk_score += 6
4. AI offline fallback 不再覆盖本地 risk_score（修复原有 bug）
5. `build_event_json()` diagnostics 新增 flame_gpio/flame_raw/flame_active_high
6. `write_status_json()` 新增 last_flame 字段
7. door 保持固定 0（Task10-A 跳过），mq2 保持固定 0

## 构建与部署

- 构建：`scripts/build_imx6ull.sh imx_safetyd` ✓
- 部署：`scripts/deploy_imx6ull.sh build/imx6ull/imx_safetyd` ✓

## imx_safetyd once 验证

### 场景 A：无火焰（flame=0）

```
GPIO_FLAME=119 GPIO_FLAME_ACTIVE_HIGH=1 ./imx_safetyd --mode once
```

结果：
- `flame_raw=0, flame=0`
- `state=NORMAL, risk_score=0`
- `status.json: last_flame=0, last_state="NORMAL", last_risk_score=0`

### 场景 B：有火焰（模拟，flame=1 via active_high=0）

```
GPIO_FLAME=119 GPIO_FLAME_ACTIVE_HIGH=0 ./imx_safetyd --mode once
```

结果：
- `flame_raw=0, flame=1`（反转逻辑）
- `state=ALARM, risk_score=6`
- `need_snap=true` → 抓拍 → AI offline fallback
- `status.json: last_flame=1, last_state="ALARM", last_risk_score=6`

## 结论

**Task10-B 通过**（代码逻辑验证）：
- flame GPIO 读取正常
- flame=1 → ALARM，risk_score=6，纯本地不依赖 AI/network
- AI offline 不覆盖本地 risk_score
- status JSON 包含 last_flame

传感器硬件后续需调整灵敏度电位器，确保无火焰时稳定读 0。

## 边界

- door 本轮跳过（Task10-A 暂缓）
- mq2 / PIR raw=1 / buzzer / RGB 未验证
- 不接继电器 / 风扇 / 水泵 / MOS / 220V
- AI / OPi5 / Flask 不控制执行器
- `control_allowed` 保持 `false`
