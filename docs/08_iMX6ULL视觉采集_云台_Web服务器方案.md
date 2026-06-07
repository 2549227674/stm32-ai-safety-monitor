# 08 i.MX6ULL 视觉采集、云台与 Web 服务器方案

## 8.1 USB 摄像头职责

USB 摄像头接 i.MX6ULL，用于事件抓拍和云台巡检。它替代 ESP32-CAM 的抓拍职责，但不承担联网主控或 AI 推理。

## 8.2 V4L2 抓帧流程

```text
打开 /dev/video<TODO:VERIFY>
  → 查询格式与分辨率
  → 选择 MJPEG 优先，YUYV 作为回退
  → mmap 或 read 采集一帧
  → 若 YUYV 则转 JPEG
  → 保存到 spool/captures
  → 上传 OPi5 或 Flask
```

推荐先用命令验证：

```bash
v4l2-ctl --list-devices
v4l2-ctl --device=/dev/video<TODO:VERIFY> --list-formats-ext
```

> **[CLAUDE_CODE_TODO | VERIFY]** 确认 V4L2 工具和设备节点
> - 为何 GPT 给不了：沙箱无法访问 i.MX6ULL 摄像头。
> - 期望产物/操作：在板端安装/确认 v4l2 工具，运行 list-devices/list-formats-ext。
> - 回填位置：docs/08 第 8.2；Task04
> - 验收：记录设备节点、格式、分辨率、命令输出。


## 8.3 MJPEG / YUYV 格式选择

| 格式 | 优点 | 缺点 | 推荐 |
|---|---|---|---|
| MJPEG | 摄像头端已压缩，网络传输方便 | 部分摄像头/驱动不支持 | MVP 首选 |
| YUYV | 通用性较好 | 需要 i.MX 转 JPEG，CPU 开销大 | 回退 |
| H264 | 适合视频流 | 处理复杂，不利于两周 MVP | 不作为主线 |

MVP 分辨率：640x480；展示增强可尝试 1280x720；不建议 1080p 原始 YUYV 连续传输。

## 8.4 云台扫描策略

MVP 使用三点扫描：

| 步骤 | pan | tilt | 动作 |
|---|---|---|---|
| 1 | 60° | 90° | 停 800ms，抓拍 |
| 2 | 90° | 90° | 停 800ms，抓拍 |
| 3 | 120° | 90° | 停 800ms，抓拍 |

若 AI 返回多个角度的结果，选择最高 `score` 或最高 `risk_hint` 方向停留。

Task08-A 实测结果（2026-06-07）：

| 角度 | 脉宽 (us) | 结果 | 现象 |
|---|---|---|---|
| pan 60° | 1330 | 通过 | 平滑移动，无抖动 |
| pan 90° | 1500 | 通过 | 中位稳定 |
| pan 120° | 1670 | 通过 | 平滑移动，无抖动 |
| tilt 90° | 1500 | 通过 | 固定不动 |

- 稳定等待：800ms 足够
- 脉宽映射：MG90S 约 90°/500us，中心 1500us
- 安全范围：1100–1900us（已强制限制）
- 分时策略：先设 tilt 再设 pan，避免同时大幅动作
- 无 5V 压降、无卡死、无发热、i.MX6ULL 未掉线
- 每点抓拍成功（7.3–7.9KB JPEG）
- 工具：`pca9685_set_pose` (C helper) + `pan_tilt_scan_once.sh` (shell 脚本)


## 8.5 图片保存与上传

本地建议路径：

```text
spool/captures/<device_id>/<YYYYMMDD>/<frame_id>.jpg
```

该目录不入库。事件上报可携带图片 URL，或通过 Flask `/api/images` 获取 URL 后再写入事件。

## 8.6 Dashboard 页面设计

在现有 Dashboard 基础上扩展：

| 区域 | 展示内容 |
|---|---|
| 当前状态 | state、risk_score、device_id、seq |
| 传感器 | door/pir/flame/mq2 |
| 执行器 | relay/pump/buzzer/rgb |
| 视觉 | 最新图片、pan/tilt、frame_id |
| AI | objects、faces、risk_hint、summary、latency |
| 历史事件 | 时间、状态、风险、AI 摘要、图片链接 |

## 8.7 断网缓存

若 Flask 不可达，i.MX6ULL 将事件和图片记录到本地 spool；网络恢复后补发。缓存目录不入库，并设置最大容量或最大保留天数。

## 8.8 演示流程

1. 打开 Dashboard。
2. 触发 PIR 或按键。
3. i.MX6ULL 进入 `VERIFY`。
4. 云台转到 90° 并抓拍。
5. OPi5 返回 person/unknown/mock 结果。
6. Dashboard 显示图片、AI 摘要、risk_score。
