# 07 UART 协议与 JSON Contract

## 7.1 协议目标

STM32 与 ESP32-CAM 之间通过 UART 通信。协议需支持 ACK/NACK、PING/PONG 心跳、SNAP_REQ/SNAP_OK/SNAP_FAIL、上传状态回报、AI 摘要回传、错误码、超时重传和离线降级。不传输图片，只传输命令、状态、图片路径和 AI 摘要。ESP32-CAM 不通过该协议控制关键执行器。

---

## 7.2 字节级帧格式

```text
0xAA 0x55 | version | msg_type | seq | cmd | len | payload | checksum
```

| 字段 | 长度 | 说明 |
|---|---:|---|
| SOF1/SOF2 | 2 | 固定 0xAA 0x55 |
| version | 1 | 协议版本，默认 0x01 |
| msg_type | 1 | 帧类型 |
| seq | 1 | 序号，用于 ACK 与重传 |
| cmd | 1 | 命令码 |
| len | 1 | payload 长度，0-64 字节 |
| payload | 0-64 | 参数，可用短 ASCII 或二进制 |
| checksum | 1 | 从 version 到 payload 的异或校验 |

---

## 7.3 命令码表

| cmd | 名称 | 方向 | Payload | 说明 |
|---:|---|---|---|---|
| 0x01 | PING | STM32→ESP | 空 | 心跳请求 |
| 0x02 | PONG | ESP→STM32 | cam,wifi,sd | 心跳响应 |
| 0x10 | SNAP_REQ | STM32→ESP | event_id,risk | 请求抓拍 |
| 0x11 | SNAP_OK | ESP→STM32 | event_id,img_id | 抓拍成功 |
| 0x12 | SNAP_FAIL | ESP→STM32 | event_id,err | 抓拍失败 |
| 0x20 | GET_STATUS | STM32→ESP | 空 | 查询 ESP 状态 |
| 0x21 | CAM_READY | ESP→STM32 | cam,wifi,sd | 摄像头准备好 |
| 0x22 | NET_OK | ESP→STM32 | ip 摘要 | 网络在线 |
| 0x23 | NET_FAIL | ESP→STM32 | err | 网络失败 |
| 0x30 | UPLOAD_REQ | STM32→ESP | event_id | 请求上传事件和图片至服务器 |
| 0x31 | UPLOAD_OK | ESP→STM32 | event_id | 上传成功 |
| 0x32 | UPLOAD_FAIL | ESP→STM32 | err | 上传失败 |
| 0x40 | AI_REQ | STM32→ESP | event_id | 请求云端 AI 解释 |
| 0x41 | AI_RESULT | ESP→STM32 | type,conf,level | AI 摘要 |
| 0x42 | AI_FAIL | ESP→STM32 | err | AI 请求失败 |
| 0x7E | ACK | 双向 | ack_seq,cmd,err=0 | 确认 |
| 0x7F | NACK | 双向 | nack_seq,cmd,err | 否认/错误 |

---

## 7.4 错误码表

| 错误码 | 名称 | 说明 |
|---:|---|---|
| 0x00 | OK | 成功 |
| 0x01 | CHECKSUM_ERR | 校验错误 |
| 0x02 | LEN_ERR | 长度错误 |
| 0x03 | CMD_UNKNOWN | 未知命令 |
| 0x04 | BUSY | ESP 正忙 |
| 0x10 | CAM_INIT_FAIL | 摄像头初始化失败 |
| 0x11 | SNAP_FAIL_CAMERA | 拍照失败 |
| 0x12 | SD_FAIL | SD 卡不可用 |
| 0x20 | WIFI_OFFLINE | Wi-Fi 离线 |
| 0x21 | HTTP_FAIL | HTTP 失败 |
| 0x22 | MQTT_FAIL | MQTT 失败 |
| 0x30 | AI_TIMEOUT | AI 超时 |
| 0x31 | AI_INVALID_JSON | AI 返回格式错误 |
| 0x32 | AI_API_ERROR | AI API 调用失败 |
| 0x33 | UPLOAD_FAIL_HTTP | 上传 HTTP 错误 |
| 0x40 | TIMEOUT | 通用超时 |

---

## 7.5 ACK/NACK 与超时重传

| 项目 | 建议值 |
|---|---|
| PING 周期 | 2 s |
| PING 超时 | 300-500 ms |
| SNAP_REQ ACK 超时 | 500 ms |
| SNAP_REQ 重传次数 | 最多 3 次 |
| SNAP 结果等待 | 3-5 s |
| 离线判定 | 连续 3 次心跳失败 |
| 恢复判定 | 连续 2 次 PONG 成功 |

---

## 7.6 Web Dashboard 状态 JSON

ESP32-CAM 或服务器提供此 JSON 用于 Web 页面刷新。

```json
{
  "schema_ver": "2.0",
  "device_id": "labbox_001",
  "timestamp": "2026-06-01T10:30:00+08:00",
  "uptime_ms": 356700,
  "state": "alarm",
  "risk_level": "alarm",
  "risk_score": 7,
  "sensors": {
    "smoke_adc": 1850,
    "flame": false,
    "pir": true,
    "door_open": false,
    "temp_c": 46.2,
    "humi_pct": 55.0,
    "vib_rms_g": 0.12,
    "vib_pp_g": 0.30
  },
  "actuators": {
    "buzzer": "fast",
    "rgb": "red",
    "fan": true,
    "relay_cutoff": false,
    "pump": false
  },
  "camera": {
    "cam_status": "online",
    "snap_result": "ok",
    "image_url": "/uploads/E000012.jpg"
  },
  "network": {
    "wifi_status": "online",
    "server_status": "online",
    "ai_status": "available"
  },
  "ai_result": {
    "ai_detected_type": "possible_smoke",
    "confidence": 0.82,
    "scene_description": "图像中疑似存在轻微烟雾",
    "suggestion": "建议检查设备柜内部是否有过热元件",
    "ai_risk_level": "warning"
  },
  "error_code": 0
}
```

---

## 7.7 事件上报 JSON

每次异常事件触发时，ESP32-CAM 将此 JSON POST 至服务器。

```json
{
  "schema_ver": "2.0",
  "event_id": "E000012",
  "device_id": "labbox_001",
  "timestamp": "2026-06-01T10:30:00+08:00",
  "event_type": "SMOKE_ALARM",
  "risk_level_local": "alarm",
  "risk_score": 7,
  "sensor_snapshot": {
    "smoke_adc": 1850,
    "flame": false,
    "pir": true,
    "door_open": false,
    "temp_c": 46.2,
    "humi_pct": 55.0,
    "vib_rms_g": 0.12
  },
  "local_action": "FAN_ON,BUZZER_FAST,RGB_RED",
  "system_state": "alarm",
  "image_filename": "E000012.jpg",
  "cam_status": "online",
  "wifi_status": "online"
}
```

---

## 7.8 MQTT Topic 建议

```text
labbox/{device_id}/status        周期状态上报
labbox/{device_id}/event         异常事件上报
labbox/{device_id}/snapshot      抓拍结果上报
labbox/{device_id}/ai_result     AI 解释结果上报
labbox/{device_id}/health        设备健康状态上报
```

不设置允许远程控制水泵、继电器、风扇的 Topic。远程命令最多用于查询状态或请求抓拍。关键执行器由 STM32 本地状态机决定。

---

## 7.9 AI 结果映射原则

| AI 结果 | STM32 处理 |
|---|---|
| normal | 可显示"AI: normal"，不降低本地风险 |
| warning | 可 OLED 显示 AI 辅助预警，可记录日志 |
| alarm | 可作为辅助证据显示，不直接升级 |
| danger | 只辅助显示和记录，不直接开水泵/继电器 |
| timeout/fail | 显示 AI_TIMEOUT/AI_FAIL，不改变本地状态 |

硬边界：**AI 结果不得覆盖 STM32 本地安全判断，不得直接控制关键执行器。**

---

## 7.10 JSON 字段规范

| 字段 | 规范 |
|---|---|
| schema_ver | 必填，字符串，"2.0" |
| device_id | 必填，设备编号 |
| timestamp | 推荐，ISO8601 格式 |
| state | 小写字符串：normal/alarm/danger 等 |
| risk_level | normal/warning/alarm/danger |
| 单位 | 写入字段名：temp_c、humi_pct、vib_rms_g |
| error_code | 数字错误码 |
| 向后兼容 | 新增字段不删除旧字段，解析端忽略未知字段 |
