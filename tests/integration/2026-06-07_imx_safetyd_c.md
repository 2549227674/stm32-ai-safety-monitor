# 2026-06-07 Task07-C C 版 imx_safetyd 测试记录

## 1. 分支与提交

- 分支：migration/imx6ull-opi5-edge-ai
- 当前阶段：Task07-C C 版 imx_safetyd
- 基线：ab17f8b task07-b: add imx safetyd lite gpio degradation spool

## 2. 结论

- C 程序编译：通过
- 部署到 i.MX：通过
- NORMAL 场景：通过
- VERIFY 场景：通过（FORCE_VERIFY=1）
- OPi5 offline 降级：通过
- Flask spool：通过
- spool flush：通过
- loop 模式：通过
- systemd：不支持（当前板端无 `systemctl`）
- status JSON：通过
- Dashboard/API 展示：通过
- control_allowed=false：通过
- 未提交图片/数据库/inventory：确认

## 3. 编译与部署

构建命令：

```bash
scripts/build_imx6ull.sh imx_safetyd
```

输出摘要：

```text
[build] CC = .../arm-buildroot-linux-gnueabihf-gcc
arm-buildroot-linux-gnueabihf-gcc.br_real (Buildroot 2020.02-gee85cab) 7.5.0
[build] 产物: build/imx6ull/imx_safetyd
```

ELF 检查：

```text
build/imx6ull/imx_safetyd: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 4.9.0, not stripped
```

部署：

```bash
scripts/deploy_imx6ull.sh build/imx6ull/imx_safetyd
```

输出摘要：

```text
[deploy] 已推送 build/imx6ull/imx_safetyd -> <IMX_USER>@<IMX_HOST>:/opt/edge-ai-safety-monitor/
```

help smoke：

```text
/opt/edge-ai-safety-monitor/imx_safetyd --mode once|loop|flush [options]
支持 --opi5-url / --backend-url / --device-id / --gpio-pir / --gpio-active-high / --capture-device / --base-dir / --post-normal / --force-verify / --interval-sec
```

## 4. NORMAL 场景

命令摘要：

```bash
/opt/edge-ai-safety-monitor/imx_safetyd \
  --mode once \
  --opi5-url http://<OPI5_HOST>:<OPI5_AI_PORT> \
  --backend-url http://<BACKEND_HOST>:<BACKEND_PORT> \
  --device-id labbox_001 \
  --gpio-pir 117 \
  --post-normal 1
```

结果：

```text
gpio117 raw=0 pir=0 active_high=1 force_verify=0
state=NORMAL
risk_score=0
need_snap=false
ai_status=skipped
Flask HTTP 201
seq=9002
post_ms=111
total_ms=117
```

说明：本轮 PIR 真实 GPIO 只复现空闲值 `raw=0`，未做人工走动触发。

## 5. VERIFY 场景

命令摘要：

```bash
/opt/edge-ai-safety-monitor/imx_safetyd \
  --mode once \
  --opi5-url http://<OPI5_HOST>:<OPI5_AI_PORT> \
  --backend-url http://<BACKEND_HOST>:<BACKEND_PORT> \
  --device-id labbox_001 \
  --gpio-pir 117 \
  --force-verify 1
```

结果：

```text
gpio117 raw=0 pir=1 active_high=1 force_verify=1
state=VERIFY
risk_score_before_ai=3
capture_ok=true
capture_file=/opt/edge-ai-safety-monitor/captures/imx-safetyd/safetyd_9003.jpg
capture_size=约 7.2KB
OPi5 HTTP 200
risk_hint=2
control_allowed=false
risk_score=5
Flask HTTP 201
seq=9003
capture_ms=430
ai_ms=124
post_ms=104
total_ms=664
```

生成事件摘要：

```json
{
  "seq": 9003,
  "state": "VERIFY",
  "risk_score": 5,
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "vision": {"frame_id": 9003, "capture_ok": true, "pan_tilt": {"pan_deg": 90, "tilt_deg": 90}},
  "ai_result": {"ok": true, "risk_hint": 2, "control_allowed": false},
  "diagnostics": {"program": "imx_safetyd_c", "ai_status": "ok", "backend_status": "posted", "force_verify": true}
}
```

## 6. OPi5 offline

错误 OPi5 URL：`http://<OPI5_HOST>:18080`

结果：

```text
OPi5 HTTP 000
ai_status=offline
fallback ai_result.ok=false
fallback ai_result.mode=offline
fallback ai_result.control_allowed=false
fallback ai_result.error=opi5_unreachable
risk_score=4
Flask HTTP 201
seq=9004
未复用旧 AI 响应：确认
```

fallback AI JSON 摘要：

```json
{
  "ok": false,
  "contract_version": "1.0",
  "model_ready": false,
  "mode": "offline",
  "models": [],
  "objects": [],
  "faces": [],
  "risk_hint": 0,
  "summary": "OPi5 AI service unavailable; local safety logic only",
  "action_hint": "local_only",
  "control_allowed": false,
  "error": "opi5_unreachable"
}
```

## 7. Flask spool / flush

错误 backend URL：`http://<BACKEND_HOST>:15000`

结果：

```text
Flask HTTP 000
backend_status=spooled
pending/event_9005.json 存在
程序未崩溃
```

pending 摘要：

```text
/opt/edge-ai-safety-monitor/spool/imx-safetyd/pending/event_9005.json
seq=9005
state=VERIFY
risk_score=5
ai_status=ok
backend_status=spooled
spool_path=/opt/edge-ai-safety-monitor/spool/imx-safetyd/pending/event_9005.json
control_allowed=false
```

flush 命令：

```bash
/opt/edge-ai-safety-monitor/imx_safetyd \
  --mode flush \
  --backend-url http://<BACKEND_HOST>:<BACKEND_PORT>
```

flush 结果：

```text
event_9005.json -> sent (HTTP 201)
flush done: 1/1 sent
pending/ 为空
sent/ 包含 event_9005.json、event_9005.response.txt、event_9005.http
```

Flask API 验证：

```text
event_id=1043 seq=9005 state=VERIFY risk=5 control_allowed=False
```

## 8. loop / systemd

loop 命令：

```bash
timeout 8 /opt/edge-ai-safety-monitor/imx_safetyd \
  --mode loop \
  --interval-sec 2 \
  --opi5-url http://<OPI5_HOST>:<OPI5_AI_PORT> \
  --backend-url http://<BACKEND_HOST>:<BACKEND_PORT> \
  --post-normal 0
```

loop 输出摘要：

```text
loop start interval_sec=2
gpio117 raw=0 pir=0
state=NORMAL
skip NORMAL and POST_NORMAL=0
重复 4 个周期
loop stopped
```

systemd：

```text
systemctl not available
```

结论：当前 i.MX6ULL 镜像不支持 systemd。本轮已新增 `edge/imx6ull-controller/systemd/imx-safetyd.service` 模板，但不作为当前板端验收阻塞项。

## 9. status JSON

路径：

```text
/opt/edge-ai-safety-monitor/run/imx_safetyd_status.json
```

VERIFY 后摘要：

```json
{
  "program": "imx_safetyd_c",
  "last_seq": 9003,
  "last_state": "VERIFY",
  "last_pir": 1,
  "last_risk_score": 5,
  "last_ai_status": "ok",
  "last_backend_status": "posted",
  "last_error": null
}
```

flush 后摘要：

```json
{
  "program": "imx_safetyd_c",
  "last_state": "FLUSH",
  "last_backend_status": "flushed",
  "last_error": null
}
```

运行日志路径：

```text
/opt/edge-ai-safety-monitor/run/imx_safetyd.log
```

## 10. 边界

- 本轮仍使用 OPi5 mock AI，不是真实 RKNN。
- 本轮 C 程序调用 `v4l2-ctl` / `curl` 子进程，不是原生 V4L2 / HTTP 库实现。
- 本轮不接 MOS/水泵/220V。
- 本轮不让 OPi5/Flask/AI 控制执行器。
- 本轮 actuators 字段仍为安全态事件字段，未真实驱动执行器。
- 本轮接入真实 PIR gpio117，但只测到空闲 `raw=0`；VERIFY 由 `FORCE_VERIFY=1` 触发，未完成人工 PIR 触发。
- 本轮不提交抓拍图片、数据库、uploads、inventory.yaml。

## 11. 下一步

- Task09：最终演示脚本与证据包整理。
- Task05-B：RKNN demo 验证。
- Task07-D：原生 libcurl / 原生 V4L2 / 更完整 C 模块化，视时间决定。
