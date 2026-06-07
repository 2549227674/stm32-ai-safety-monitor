# 2026-06-07 Task08-A 云台分时巡检 + 多角度抓拍测试记录

## 1. 分支与提交

- 分支：migration/imx6ull-opi5-edge-ai
- 当前阶段：Task08-A 云台分时巡检 + 多角度抓拍
- 基线：fceb310 task07-a: end-edge vertical slice with mock ai

## 2. 结论

- 舵机供电与共地：已确认（用户确认独立 5V 供电、共地、OE 接 GND）
- pca9685_set_pose 编译部署：通过
- 中位姿态：通过（1500/1500us，无抖动/卡死/压降/发热/掉线）
- 三点姿态：通过（1330/1500/1670us，分时动作稳定）
- 三点抓拍：通过（3 张 JPEG，7.3–7.9KB）
- 三点 AI infer：通过（3 次 HTTP 200，risk_hint=2）
- 最佳角度选择：通过（tie → center pan=90deg）
- Flask 上报：通过（HTTP 201，event_id=1033）
- Dashboard 显示：通过（API 返回完整 vision.scan/selected/ai_result）
- 是否抖动/卡死/压降/发热/掉线：无
- control_allowed=false：通过

## 3. 硬件接线与安全确认

### PCA9685 逻辑侧

| PCA9685 | i.MX6ULL J5 |
|---|---|
| VCC | J5 pin 3 (3.3V) |
| GND | J5 pin 2 (GND) |
| SDA | J5 pin 7 (I2C_SDA) |
| SCL | J5 pin 8 (I2C_SCL) |

### 舵机电源

| 信号 | 来源 |
|---|---|
| PCA9685 V+ | 独立 5V~6V 电源 |
| OE | 接 GND |
| CH0 | Pan (MG90S #1) |
| CH1 | Tilt (MG90S #2) |

用户已确认：独立供电、共地、无机械阻挡。

## 4. 编译与部署

### pca9685_set_pose 编译

```text
scripts/build_imx6ull.sh pca9685_set_pose
[build] CC = .../arm-buildroot-linux-gnueabihf-gcc
[build] 产物: build/imx6ull/pca9685_set_pose
```

### 部署

```text
[deploy] 已推送 build/imx6ull/pca9685_set_pose -> root@192.168.137.110:/opt/edge-ai-safety-monitor/
pan_tilt_scan_once.sh 已部署并 chmod +x
```

## 5. 姿态单测

| 点位 | pan_deg | pan_us | tilt_deg | tilt_us | 结果 | 现象 |
|---|---:|---:|---:|---:|---|---|
| left | 60 | 1330 | 90 | 1500 | 通过 | 舵机平滑移动，无抖动 |
| center | 90 | 1500 | 90 | 1500 | 通过 | 回中位稳定 |
| right | 120 | 1670 | 90 | 1500 | 通过 | 舵机平滑移动，无抖动 |

所有点位分时动作（先设 tilt 再设 pan），无同时大幅动作。
无 5V 压降、无卡死、无发热、i.MX6ULL 未掉线。

## 6. 完整扫描

### 运行命令

```bash
ssh root@192.168.137.110 'cd /opt/edge-ai-safety-monitor && \
  OPI5_URL="http://10.0.1.120:8080" \
  BACKEND_URL="http://192.168.137.1:5000" \
  DEVICE_ID="labbox_001" \
  SCAN_SEQ_BASE="8001" \
  ./pan_tilt_scan_once.sh'
```

### 扫描结果

| 点位 | frame_id | pan_deg | capture_size | AI HTTP | risk_hint |
|---|---:|---:|---|---:|---:|
| 1 (left) | 8001 | 60 | 7.3KB | 200 | 2 |
| 2 (center) | 8002 | 90 | 7.4KB | 200 | 2 |
| 3 (right) | 8003 | 120 | 7.9KB | 200 | 2 |

### 最佳角度选择

- 所有点 risk_hint 相同 (=2)
- 选择规则：tie → center (pan=90deg)
- selected: frame_id=8002, pan_deg=90, tilt_deg=90
- reason: highest_risk_hint (tie, center selected)

### Flask 上报

- HTTP: 201
- event_id: 1033
- risk_score: 5
- state: VERIFY

## 7. API / Dashboard 验证

### /api/status/latest 摘要

```json
{
  "state": "VERIFY",
  "risk_score": 5,
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "vision": {
    "frame_id": 8002,
    "pan_tilt": {"pan_deg": 90, "tilt_deg": 90},
    "scan": [
      {"frame_id": 8001, "pan_deg": 60, "tilt_deg": 90, "risk_hint": 2},
      {"frame_id": 8002, "pan_deg": 90, "tilt_deg": 90, "risk_hint": 2},
      {"frame_id": 8003, "pan_deg": 120, "tilt_deg": 90, "risk_hint": 2}
    ],
    "selected": {"frame_id": 8002, "pan_deg": 90, "tilt_deg": 90, "reason": "highest_risk_hint"}
  },
  "ai_result": {
    "risk_hint": 2,
    "summary": "检测到人员进入巡检区域。",
    "control_allowed": false
  },
  "latency_ms": {"capture_total": 1244, "ai_total": 351, "post": 0, "scan_total": 0}
}
```

Dashboard 验收：
- 最新事件显示 VERIFY ✓
- AI/视觉区显示 risk_hint、summary、objects ✓
- pan_tilt 显示最终 selected 角度 (90/90) ✓
- vision.scan 通过 raw_json 透出（Dashboard 前端可忽略）✓

## 8. 延迟

| 阶段 | ms |
|---|---:|
| move_total | (含在 scan_total 中) |
| capture_total | 1244 |
| ai_total | 351 |
| post | 340 |
| scan_total | 5879 |

注：scan_total 包含 3 次 move+settle(800ms×3) + 3 次 capture + 3 次 AI + 组装+POST。

## 9. 边界

- 本轮使用 OPi5 mock AI，不是真实 RKNN。
- 本轮 sensors 仍为模拟值，非实时 GPIO。
- 本轮不接 MOS/水泵/220V。
- 本轮不让 OPi5/Flask/AI 控制执行器。
- 本轮不提交抓拍图片、数据库、uploads、inventory.yaml。
- 抓拍图片仅保存在板端 `/opt/edge-ai-safety-monitor/captures/task08/`。
- risk_score 融合规则：`min(10, 3 + risk_hint)`，Task08-A 演示规则。
- 选择规则：mock AI 所有点 risk_hint 相同，选择 center 作为 tie-break。

## 10. 下一步

- Task08-B：把云台扫描集成进 imx_safetyd 初版。
- 或 Task07-B：降级/spool/实时 GPIO。
- 或 Task05-B：RKNN demo 验证。
