# 2026-06-07 Task07-B imx_safetyd-lite 实时 GPIO + 降级/spool 测试记录

## 1. 分支与提交

- 分支：migration/imx6ull-opi5-edge-ai
- 当前阶段：Task07-B imx_safetyd-lite 实时 GPIO + 降级/spool
- 基线：ea74c0e task08-a: add time-sliced pan-tilt scan demo

## 2. 结论

- i.MX SSH：通过
- GPIO117 PIR 读取：通过（raw_value=0 当前空闲，gpio export + sysfs 读取正常）
- NORMAL 场景：通过（pir=0, state=NORMAL, risk_score=0, 无抓拍/AI）
- VERIFY 场景：通过（FORCE_VERIFY=1, 抓拍+AI+Flask, event_id=1034）
- V4L2 抓拍：通过（640x480 MJPG, 7.0KB）
- OPi5 正常 AI：通过（HTTP 200, risk_hint=2, control_allowed=false）
- OPi5 失败干净降级：通过（HTTP 000, ai_result.ok=false, 不复用旧响应, risk_score=4）
- Flask 正常上报：通过（HTTP 201, event_id=1034/1035）
- Flask 失败 spool：通过（event_9004.json 写入 pending/）
- spool flush：通过（pending → sent, HTTP 201, event_id 在 Flask 可见）
- Dashboard 展示：通过（/api/status/latest 返回完整字段）
- control_allowed=false：通过（正常和降级场景均确认）
- 未提交图片/数据库/inventory：确认

## 3. 三端健康检查

### i.MX6ULL

```text
v4l2-ctl: /usr/bin/v4l2-ctl
curl: /usr/bin/curl
/dev/video0 (pxp), /dev/video1 (USB Camera)
/sys/class/gpio: exists
imx_safetyd_lite.sh: deployed and executable
```

### OPi5 /health

```json
{"ok": true, "mode": "mock", "contract_version": "1.0"}
```

### Flask /health

```json
{"ok": true, "service": "safety-monitor-backend", "version": "0.1.0"}
```

## 4. GPIO / PIR 读取

- GPIO 编号：117
- raw_value 空闲：0
- raw_value 触发：未触发（PIR 当前无人活动）
- active_high：1
- 对应 sensors.pir：raw=0 → pir=0
- 是否使用 FORCE_VERIFY：是（VERIFY 场景测试用）

注：GPIO 读取链路存在且正常工作，但本轮只测到空闲值 (0)。
PIR 物理触发需要有人走动，本轮未做人工触发测试。

## 5. NORMAL 场景

命令：

```bash
OPI5_URL="http://10.0.1.120:8080" BACKEND_URL="http://192.168.137.1:5000" \
  MODE=once POST_NORMAL=1 GPIO_PIR=117 ./imx_safetyd_lite.sh
```

结果：

```text
gpio117 raw=0 pir=0
state=NORMAL
risk_score=0
capture: skipped
ai: skipped
Flask HTTP 201, event_id=1035
total_ms=246
```

Dashboard/API：state=NORMAL, risk_score=0, sensors.pir=0

## 6. VERIFY 场景

命令：

```bash
OPI5_URL="http://10.0.1.120:8080" BACKEND_URL="http://192.168.137.1:5000" \
  MODE=once FORCE_VERIFY=1 GPIO_PIR=117 ./imx_safetyd_lite.sh
```

结果：

```text
gpio117 raw=0 (FORCE_VERIFY → pir=1)
state=VERIFY
capture: 7.0KB JPEG
OPi5 HTTP 200, 114ms
ai_result.risk_hint=2
ai_result.control_allowed=false
risk_score = 3+2 = 5
Flask HTTP 201, event_id=1034
total_ms=1147
```

/api/status/latest 摘要：

```text
state=VERIFY, risk_score=5, sensors.pir=1
ai_result.risk_hint=2, ai_result.control_allowed=false
vision.frame_id=9001, vision.capture_ok=true
diagnostics.gpio.pir_gpio=117, diagnostics.ai_status=ok
```

## 7. OPi5 失败降级

错误 OPI5_URL：`http://10.0.1.120:18080`

结果：

```text
OPi5 HTTP 000 (Connection refused)
ai_status=offline
生成新的 fallback ai_result: ok=false, mode=offline, control_allowed=false
risk_score = 3+1 = 4 (OPi5 不可达但本地传感器异常 +1)
Flask HTTP 201, event_id=1036
未复用旧 AI 响应：确认
```

## 8. Flask 失败 spool

错误 BACKEND_URL：`http://192.168.137.1:15000`

结果：

```text
Flask HTTP 000 (Connection refused)
backend_status=spooled
事件写入: /opt/edge-ai-safety-monitor/spool/safetyd-lite/pending/event_9004.json
脚本未崩溃
```

## 9. spool flush

命令：

```bash
BACKEND_URL="http://192.168.137.1:5000" MODE=flush ./imx_safetyd_lite.sh
```

结果：

```text
event_9004.json -> sent (HTTP 201)
pending/ 目录为空
sent/ 包含 event_9004.json 和 response
Dashboard /api/events 可见补发事件
```

## 10. 延迟

| 场景 | gpio | capture | ai | post | total |
|---|---:|---:|---:|---:|---:|
| NORMAL | 0 | 18 | 0 | 121 | 246 |
| VERIFY | 0 | 502 | 114 | 128 | 1147 |
| OPi5 offline | 0 | 554 | 99 | 145 | 1007 |
| Flask spool | 0 | 501 | 114 | 109 | 961 |

## 11. 边界

- 本轮使用 OPi5 mock AI，不是真实 RKNN。
- 本轮接入真实 PIR gpio117，但 PIR 当前空闲（raw=0），未做人工触发。
- 本轮 door/flame/mq2 仍为固定值 0。
- 本轮未接 MOS/水泵/220V。
- 本轮不让 OPi5/Flask/AI 控制执行器。
- 本轮 actuators 字段仅为安全态事件字段，未真实驱动执行器。
- 本轮不提交抓拍图片、数据库、uploads、inventory.yaml。
- 本轮 shell 版 imx_safetyd-lite 是 MVP 控制端雏形，不是最终 daemon。
- FORCE_VERIFY=1 是测试开关，不是正常运行逻辑。

## 12. 下一步

- Task09：最终演示脚本与证据包整理。
- 或 Task05-B：RKNN demo 验证。
- 或 Task07-C：systemd/长期运行/更完整 C 版 imx_safetyd。
