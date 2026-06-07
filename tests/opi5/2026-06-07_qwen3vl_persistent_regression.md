# 2026-06-07 Task05-C Qwen3-VL 持久化优化 + i.MX 端边回归测试记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前阶段：Task05-C Qwen3-VL 持久化优化 + i.MX 端边回归
- 基线：`328cccd task05-b: integrate qwen3-vl real ai service`

## 2. 结论

- Qwen3-VL worker 编译/部署：通过
- worker 常驻加载模型：通过
- worker health：通过
- worker 连续推理：通过
- single-shot 延迟基线：~13s（每次加载模型）
- worker 延迟：~8.4s（首次 ~13s，后续 ~8.4s）
- Flask AI 服务 QWEN3VL_MODE=worker：通过
- AI_BACKEND=mock 回归：通过
- AI_BACKEND=qwen3vl worker：通过
- i.MX imx_safetyd 回归：通过
- Flask /api/events：通过（HTTP 201）
- Dashboard/API 展示：通过（vlm_result/explanation/risk_hint 可见）
- control_allowed=false：通过
- 未提交模型/图片/tokenizer/inventory：确认

## 3. worker 实现说明

- 新增文件：
  - `edge/opi5-ai/service/qwen3vl_worker.cpp` — C++ 持久化 worker，stdin/stdout JSON-line 协议
  - `edge/opi5-ai/service/qwen3vl_worker_client.py` — Python 客户端，管理 worker 子进程
- 更新文件：
  - `edge/opi5-ai/service/qwen3vl_backend.py` — 支持 QWEN3VL_MODE=worker|single_shot
  - `edge/opi5-ai/service/risk_mapping.py` — 扩展否定前缀列表
- worker 通信方式：stdin/stdout JSON-line 协议
- 启动时加载模型：是
- 避免每次重新加载：是（模型加载一次，后续请求只做推理）
- fallback：single_shot 模式仍可用，QWEN3VL_MODE=single_shot

## 4. 构建与部署

```bash
# 构建（OPi5 原生编译）
DEPLOY=/home/orangepi/rknn-llm/examples/multimodal_model_demo/deploy
SVC=/opt/edge-ai-safety-monitor/opi5-ai
# 使用 cmake，OpenCV 静态链接，rpath 指向 SVC
cd /home/orangepi/qwen3vl_worker_build
cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64
make -j4
```

部署文件：

```text
/opt/edge-ai-safety-monitor/opi5-ai/
  app.py                    # Flask 服务
  qwen3vl_backend.py        # 后端调度（worker/single_shot）
  qwen3vl_worker_client.py  # Python worker 客户端
  risk_mapping.py           # VLM 文本 → risk 映射
  qwen3vl_worker            # C++ 持久化 worker
  qwen3vl_single_shot       # C++ 单次推理（fallback）
  librknnrt.so              # RKNN runtime
  librkllmrt.so             # RKLLM runtime
```

未入库：模型权重、runtime 库、二进制文件、测试图片。

## 5. worker 直接测试

health 输出：

```json
{"ok":true,"service":"qwen3vl-worker","model_loaded":true,"model":"qwen3-vl-2b","backend":"rknn-llm"}
```

连续 3 次推理（中文 prompt）：

| 次序 | total_ms | imgenc_ms | llm_ms |
|---:|---:|---:|---:|
| 1 | 9488 | 2331 | 7146 |
| 2 | 8373 | 2228 | 6124 |
| 3 | 8878 | 2662 | 6197 |

首次包含模型加载，后续无加载开销。

## 6. Flask AI 服务测试

### QWEN3VL_MODE=worker

```bash
AI_BACKEND=qwen3vl QWEN3VL_MODE=worker OPI5_AI_PORT=8080 python3 app.py
```

/health：mode=qwen3vl, model_ready=true

/api/infer/vision 第 2 次请求：

```json
{
  "ok": true,
  "risk_hint": 0,
  "summary": "画面中是一个非常昏暗的室内场景...未观察到人员、烟雾、火焰或明显安全异常。",
  "action_hint": "none",
  "control_allowed": false,
  "latency_ms": 9069
}
```

vlm_result.labels: []
vlm_result.timing: imgenc_ms=2868, llm_ms=6169, total_ms=9063

## 7. mock 回归

```bash
AI_BACKEND=mock OPI5_AI_PORT=8080 python3 app.py
```

/health：mode=mock, model_ready=true

/api/infer/vision：

```json
{
  "ok": true,
  "risk_hint": 2,
  "objects": [{"label": "person", "score": 0.9, "bbox": [120, 80, 260, 430]}],
  "control_allowed": false
}
```

mock bbox 仍存在，control_allowed=false。

## 8. i.MX 端边回归

OPi5 状态：qwen3vl worker + Flask AI 服务（AI_BACKEND=qwen3vl, QWEN3VL_MODE=worker）

i.MX 命令：

```bash
/opt/edge-ai-safety-monitor/imx_safetyd --mode once \
  --opi5-url http://10.0.1.120:8080 \
  --backend-url http://192.168.137.1:5000 \
  --force-verify 1
```

结果：

```text
capture: safetyd_9009.jpg (454ms)
ai: HTTP 200, risk_hint=0, control_allowed=false (7108ms)
flask: HTTP 201 posted (135ms)
total: 7705ms
```

Flask /api/status/latest 摘要：

```json
{
  "seq": 9009,
  "state": "VERIFY",
  "risk_score": 3,
  "risk_hint": 0,
  "summary": "画面中呈现的是一个完全黑暗的场景...无法辨认出任何人员、烟雾、火焰或明显的安全异常。",
  "vlm_result.labels": [],
  "vlm_result.timing": {"imgenc_ms": 2404, "llm_ms": 4496, "total_ms": 6926},
  "control_allowed": false,
  "models": [{"name": "qwen3-vl-2b", "backend": "rknn-llm"}]
}
```

Dashboard API 中可见 summary、risk_hint、vlm_result。

## 9. 延迟对比

| 模式 | 第1次 ms | 第2次 ms | 第3次 ms | 说明 |
|---|---:|---:|---:|---|
| single-shot | ~13000 | ~13000 | ~13000 | 每次加载模型 |
| worker direct | 9488 | 8373 | 8878 | 首次含加载 |
| Flask qwen3vl worker | 13776 | 9069 | - | 包含 HTTP/包装开销 |
| i.MX end-to-end | - | 7705 | - | 抓拍+AI+上报 |

## 10. 边界

- 本轮仍不训练模型。
- 本轮仍不提交 .rknn/.rkllm/.onnx/.pt/.pth/.bin/tokenizer。
- 本轮仍不提交抓拍图片、人脸库、数据集。
- Qwen3-VL 主要提供图像解释和风险提示，不保证 bbox。
- objects[] 为空，因为 Qwen3-VL 是 VLM 不是目标检测模型。
- AI 仍不控制执行器，control_allowed=false。
- risk_mapping 否定检测已扩展（增加"不能""无法""不确定"等前缀）。
- worker 模式比 single-shot 快约 35%（后续请求）。

## 11. 下一步

- Task05-D：YOLO/RKNN 目标检测 bbox 补充。
- Task09：最终演示脚本与证据包。
- 报告/PPT 整理。
