# Task 04：i.MX6ULL V4L2 抓拍

## 目标 / 范围

验证 i.MX6ULL 上 USB 摄像头识别、格式查询、静帧抓取和图片保存。本任务不做 OPi5 推理，不做高帧率视频流。

## 前置条件

Task02 通过；摄像头接 i.MX6ULL USB Host；供电稳定。

## 操作步骤

1. 连接 USB 摄像头。
2. 查询 `/dev/video*`。
3. 查询格式和分辨率。
4. 优先抓 MJPEG；若只有 YUYV，则转换 JPEG。
5. 保存 `latest.jpg`。
6. 记录耗时和文件大小。

## 命令骨架

```bash
ssh <IMX_USER_TODO_FILL>@<IMX_IP_TODO_FILL>
v4l2-ctl --list-devices
v4l2-ctl --device=/dev/video<TODO_VERIFY> --list-formats-ext
# 运行 v4l2_capture，具体参数由实现补充
./v4l2_capture --device /dev/video<TODO_VERIFY> --output /tmp/latest.jpg
```

## 产出物

- `edge/imx6ull-controller/src/v4l2_capture.c` 或等效实现
- `tests/imx6ull/YYYY-MM-DD_v4l2_capture.md`
- 本地测试图片路径记录，不提交图片大文件

## 验收标准

- [ ] 板端生成可打开图片。
- [ ] 记录设备节点、格式、分辨率、耗时。
- [ ] 失败时有回退方案。

## 禁止事项

- 不连续传输 1080p 原始流作为 MVP 主线。
- 不提交抓拍图片到 git。

## 完成后回填

- 回填 docs/08 摄像头格式。
- 更新 AGENTS.md 摄像头状态。
- 更新 DEVPLAN P3。
