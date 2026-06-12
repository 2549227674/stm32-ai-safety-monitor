# OPi5 All-in-One 部署指南

## 部署目标

所有服务运行在 Orange Pi 5 本机：

| 服务 | 端口 | 说明 |
|---|---|---|
| Flask backend + React 前端 | 5000 | 主入口，浏览器访问 |
| opi5-device-agent | 8090 | 摄像头、遥测、心跳 |
| opi5-ai (Qwen3-VL) | 8080 | AI 推理 |
| opi5-safetyd | — | GPIO/PWM 本地安全闭环 |

PC 只作为浏览器访问：`http://<OPI5_IP>:5000`

## 环境变量

统一环境文件：`/etc/edge-ai-safety-monitor.env`

```bash
DEVICE_ID=edge-opi5-001
BACKEND_URL=http://127.0.0.1:5000
AI_URL=http://127.0.0.1:8080
OPI5_DEVICE_AGENT_URL=http://127.0.0.1:8090
AGENT_PORT=8090
CAMERA_DEVICE=/dev/video0
VIDEO_MOCK=0
VIDEO_WIDTH=1280
VIDEO_HEIGHT=720
VIDEO_FPS=10
VIDEO_JPEG_QUALITY=70
```

示例文件：`config/opi5-all-in-one.env.example`

## systemd 安装

```bash
# 从项目根目录运行
sudo bash scripts/deploy_opi5_all_in_one.sh
```

或手动：

```bash
sudo cp deploy/systemd/edge-ai-backend.service /etc/systemd/system/
sudo cp deploy/systemd/opi5-device-agent.service /etc/systemd/system/
sudo cp deploy/systemd/opi5-safetyd.service /etc/systemd/system/
sudo cp deploy/systemd/opi5-ai-qwen3vl.service /etc/systemd/system/
sudo systemctl daemon-reload
```

## 启动

```bash
sudo systemctl start edge-ai-backend
sudo systemctl start opi5-device-agent
sudo systemctl start opi5-safetyd
# AI 服务需要模型文件就绪后启动
sudo systemctl start opi5-ai-qwen3vl
```

## 停止

```bash
sudo systemctl stop edge-ai-backend opi5-device-agent opi5-safetyd opi5-ai-qwen3vl
```

## 查看日志

```bash
journalctl -u edge-ai-backend -f
journalctl -u opi5-device-agent -f
journalctl -u opi5-safetyd -f
journalctl -u opi5-ai-qwen3vl -f
```

## 前端构建

```bash
cd server/frontend
npm install    # 首次
npm run build  # 构建产物到 server/backend/static/edge-console/
```

Flask 直接服务静态文件，无需 nginx。

## 视频卡顿调优

默认 1280x720 @10fps，JPEG quality=70。如卡顿：

```bash
# 在 /etc/edge-ai-safety-monitor.env 中修改：
VIDEO_WIDTH=640
VIDEO_HEIGHT=480
VIDEO_FPS=10
VIDEO_JPEG_QUALITY=60
```

修改后重启 device-agent：`sudo systemctl restart opi5-device-agent`

## SMTP QQ 邮箱配置

1. 在 QQ 邮箱设置中开启 SMTP 服务，获取授权码
2. 编辑 `/etc/edge-ai-safety-monitor.env`：

```bash
ALERT_EMAIL_ENABLED=1
SMTP_HOST=smtp.qq.com
SMTP_PORT=465
SMTP_USER=你的QQ邮箱
SMTP_PASSWORD=QQ邮箱授权码（不是QQ密码）
SMTP_FROM=你的QQ邮箱
ALERT_EMAIL_TO=接收告警的邮箱
```

3. 重启后端：`sudo systemctl restart edge-ai-backend`

### notification_config.json

- 如果 `server/backend/notification_config.json` 已存在于 OPi5 项目目录，后端会优先读取它。
- 该文件被 `.gitignore` 忽略，不会随 `git push/pull` 分发。
- fresh clone 到 OPi5 后，需要重新在前端通知设置页保存，或手动复制 `notification_config.json`，或在 `/etc/edge-ai-safety-monitor.env` 配置 SMTP。
- 不要提交 QQ 邮箱授权码到仓库。

## 部署说明

PC Flask 作为主部署方式已不再推荐。当前主方案为 OPi5 all-in-one。
