# 05 开发计划与 MVP 验收标准（OPi5 主线版）

## 5.1 MVP 定义

MVP 是一条可演示、可截图、可录屏的 OPi5 本地闭环：

```text
OPi5 safetyd 传感器采样 → 状态机 → 执行器
  → device-agent 心跳/遥测/AI observation
  → Flask 记录
  → React edge-console 展示
```

## 5.2 最小验收标准

| 编号 | 标准 | 验收证据 |
|---|---|---|
| M1 | py_compile backend + device-agent | 命令输出 PASS |
| M2 | Flask API smoke | `/health` + `/api/events` + `/api/devices` curl 200/201 |
| M3 | seed mock 数据 | `scripts/seed_frontend_mock_data.py` 运行成功 |
| M4 | npm build | `cd server/frontend && npm run build` 无报错 |
| M5 | device-agent 心跳上报 | Flask `/api/devices` 可见设备 |
| M6 | device-agent 遥测上报 | Flask `/api/telemetry/series` 有数据 |
| M7 | device-agent AI observation | Flask `/api/ai/observations/latest` 有数据 |
| M8 | React edge-console 可展示 | Dashboard 打开，mock/real 状态均可显示 |
| M9 | notification settings/logs | GET/PUT settings + GET logs 返回 200 |
| M10 | AI `control_allowed=false` | AI 响应中该字段为 false |

## 5.3 后续增强（不阻塞 MVP）

- 真实摄像头视频流回归
- 真实 GPIO 传感器触发
- Qwen3-VL 持久化进程优化
- Pan 舵机巡检
- MPU6050 振动监测

## 5.4 回滚策略

| 风险 | 回滚 |
|---|---|
| RKNN 接不通 | 保留 mock AI 服务完成端边闭环 |
| 摄像头不可用 | device-agent 自动回退 mock 视频 |
| Flask schema 迁移风险 | 新字段先存 raw_json 并前端解析 |
| device-agent 离线 | Flask 返回 503，前端显示无设备 |
