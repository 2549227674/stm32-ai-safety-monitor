# OPi5 Device Agent

设备端数据采集服务，提供视频流、mock传感器、心跳、遥测批量上报、AI观察。

## 功能

- MJPEG视频流（真实摄像头或mock）
- mock传感器数据（MPU6500、环境、安全传感器）
- 设备心跳（5秒）
- 遥测批量上报（30秒）
- AI观察（30秒，调用Qwen3-VL）

## 启动

```bash
BACKEND_URL=http://<PC_IP>:5000 AI_URL=http://127.0.0.1:8080 MOCK_SENSORS=1 python3 app.py
```

## 接口

| 方法 | 路径 | 用途 |
|---|---|---|
| GET | /health | 健康检查 |
| GET | /api/status | 设备状态 |
| GET | /api/metrics/current | 最新指标 |
| GET | /api/video/stream | MJPEG视频流 |
| GET | /api/video/snapshot.jpg | 快照 |
| GET | /api/ai/latest | 最近AI观察 |
