# 2026-06-08 OPi5 Qwen3-VL 真实 AI 自启动回归

## 目标

将 OPi5 AI 默认模式从 mock 切换为 Qwen3-VL 真实 AI，并验证开机自启动。

## 当前网络

- OPi5 WiFi: 10.96.98.38
- i.MX WiFi: 10.96.98.109
- PC WiFi: 10.96.98.103
- 全无线链路: 三台设备均连手机热点

## 服务

| 项目 | 结果 |
|---|---|
| qwen3vl worker 手动启动 | 通过（model_loaded:true） |
| AI API 手动启动 | 通过（mode=qwen3vl） |
| health 是否显示 qwen3vl/real | 是（mode=qwen3vl, model_ready=true） |
| 是否监听 0.0.0.0:8080 | 是 |
| systemd worker service | 不单独运行（stdin 协议不兼容 systemd） |
| systemd api service | 通过（opi5-ai-qwen3vl.service, enabled） |
| API 内嵌 worker 管理 | 通过（qwen3vl_worker_client.py 自动管理 worker 子进程） |
| 重启后 API 是否恢复 | 是（active running） |
| 重启后是否仍为 qwen3vl | 是 |

## systemd 方案说明

最初尝试将 worker 和 API 分为两个 systemd 服务，但 worker 二进制使用 stdin/stdout JSON-line 协议通信，在 systemd 下 stdin 关闭导致 worker 立即退出。

最终方案：单个 systemd 服务（opi5-ai-qwen3vl.service），API 内部通过 `qwen3vl_worker_client.py` 管理 worker 子进程生命周期。Worker 在首次 infer 请求时启动，模型加载约 5-8 秒，后续请求无需重新加载。

## infer

| 项目 | 结果 |
|---|---|
| OPi5 本地 infer | 通过（latency_ms=15393 首次, 5086 后续） |
| i.MX -> OPi5 infer（首次） | 通过（latency_ms=10220 含 worker 启动） |
| i.MX -> OPi5 infer（后续） | 通过（latency_ms=4707~5639） |
| 重启后首次 infer | 通过（latency_ms=15112 含 worker 启动） |
| 重启后后续 infer | 通过（latency_ms=4707） |
| risk_hint | 0（暗光空场景） |
| summary/explanation/vlm_result | 存在，中文 VLM 输出 |
| labels | 空（正确，VLM 不做 bbox 检测） |
| control_allowed | false |

## 模型加载说明

Qwen3-VL worker 使用持久化进程模式：
- Worker 启动时加载一次模型（encoder ~2.4s + LLM ~3.4s）
- 后续推理请求只做推理，不重新加载
- 首次请求含加载：~10-15s
- 后续请求纯推理：~5-6s

## imx_safetyd 全链路

imx_safetyd 的 AI 请求返回 HTTP 000（超时），原因是 imx_safetyd 内部 HTTP 超时设置略短于 Qwen3-VL 推理时间（~9s）。直接 curl 测试正常。这不影响自启动验证，后续可调整 imx_safetyd 超时参数。

## systemd 服务文件

模板文件（入库）：
- `config/templates/opi5-ai-qwen3vl.service.example`
- `config/templates/opi5-ai-qwen3vl.env.example`

板端文件（不入库）：
- `/etc/systemd/system/opi5-ai-qwen3vl.service`
- `/etc/edge-ai/opi5-ai-qwen3vl.env`

## mock 回退说明

mock 代码保留，可手动回退：
```bash
# 停止 qwen3vl 服务
sudo systemctl stop opi5-ai-qwen3vl.service

# 手动启动 mock
cd /opt/edge-ai-safety-monitor/opi5-ai
AI_BACKEND=mock OPI5_AI_PORT=8080 python3 app.py
```

## 结论

- **成功**：OPi5 开机后默认运行 Qwen3-VL 真实 AI，自启动，无需手动启动服务
- mock 不作为默认演示模式，仅保留为手动回退
- 模型加载只在首次请求时发生，后续推理 ~5s
- 全无线链路 i.MX -> OPi5 推理正常

## 风险

- 首次请求延迟较高（~10-15s），需在演示前预热
- imx_safetyd 内部超时需调整以适配 Qwen3-VL 推理时间
- 手机热点 IP 可能变化
