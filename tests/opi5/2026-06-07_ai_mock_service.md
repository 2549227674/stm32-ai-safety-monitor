# 2026-06-07 Task05-A OPi5 AI mock 服务测试记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前阶段：Task05-A OPi5 AI mock 服务 bring-up
- 本轮边界：只部署和验证 mock AI 服务；真实 RKNN demo 接入留到 Task05-B。

## 2. 结论

- OPi5 SSH：通过，WSL 可登录 `orangepi@10.0.1.120:22`。
- Python3：`Python 3.10.12`
- pip3：存在，`pip 22.0.2`
- Flask：已安装，`flask 2.2.5`
- `/health`：通过，HTTP 200，返回 `ok=true`、`mode=mock`、`contract_version=1.0`。
- `/api/infer/vision`：通过，HTTP 200，普通 sensors 与 mq2 hazard sensors 均返回符合 `docs/07` 的 JSON。
- `control_allowed`：`false`
- RKNN：未接入，本轮只做 mock；只读盘点到若干候选目录/模型文件名，未验证可运行性。

## 3. OPi5 环境检查

执行摘要：

```text
Linux orangepi5 6.1.99-rockchip-rk3588 #1.2.2 SMP Thu Jun 26 14:42:53 CST 2025 aarch64 GNU/Linux
Python 3.10.12
pip 22.0.2 from /usr/lib/python3/dist-packages/pip (python 3.10)
flask 2.2.5
```

OPi5 home 目录中可见的相关目录摘要：

```text
/home/orangepi/Qwen2.5-3B-abliterated-rk3588
/home/orangepi/RKLLM-API-Server
/home/orangepi/curated_demo_images
/home/orangepi/fixed_sample_images_v2
/home/orangepi/librga
/home/orangepi/models
/home/orangepi/phase10
/home/orangepi/rknn-llm
/home/orangepi/rknn-toolkit2-master
/home/orangepi/vlm-sam-industrial-vision-v2
```

## 4. OPi5 本地文件结构只读盘点

执行命令：按用户提供的 `find ~ /opt ...` 命令只读盘点目录、候选 Python/shell 文件与模型样式文件名。

约束执行结果：

- 未删除、未移动、未覆盖 OPi5 现有文件。
- 未复制任何 `.rknn/.onnx/.pt/.pth/.tflite` 文件进仓库。
- 以下只表示“发现候选资产”，不表示已验证可运行。

`whoami / pwd`：

```text
orangepi
/home/orangepi
```

`/opt` 摘要：

```text
/opt
/opt/containerd
/opt/edge-ai-safety-monitor
/opt/edge-ai-safety-monitor/opi5-ai
```

候选 AI / RKNN 目录摘要：

```text
/home/orangepi/models
/home/orangepi/models/efficientad_models
/home/orangepi/models/fastsam_models
/home/orangepi/models/qwen3vl_models
/home/orangepi/phase10/edge/third_party/rknn_api
/home/orangepi/phase10/models/yolov8n_phase10
/home/orangepi/rknn-llm
/home/orangepi/rknn-llm/examples/multimodal_model_demo
/home/orangepi/rknn-llm/examples/rkllm_api_demo
/home/orangepi/rknn-llm/examples/rkllm_server_demo
/home/orangepi/rknn-toolkit2-master
/home/orangepi/rknn-toolkit2-master/rknn-toolkit-lite2
/home/orangepi/rknn-toolkit2-master/rknn-toolkit2
/home/orangepi/rknn-toolkit2-master/rknpu2/examples/rknn_api_demo
/home/orangepi/rknn-toolkit2-master/rknpu2/examples/rknn_mobilenet_demo
/home/orangepi/rknn-toolkit2-master/rknpu2/examples/rknn_yolov5_demo
/home/orangepi/vlm-sam-industrial-vision-v2/edge/third_party/rknn_api
/opt/edge-ai-safety-monitor/opi5-ai
```

候选 Python / shell 文件摘要：

```text
/home/orangepi/RKLLM-API-Server/api.py
/home/orangepi/RKLLM-API-Server/setup.sh
/home/orangepi/phase10/compare_airockchip.py
/home/orangepi/phase10/compare_onnx_rknn.py
/home/orangepi/phase10/diag_rknn.py
/home/orangepi/phase10/production_yolo_soak_runner.py
/home/orangepi/phase10/smoke_test_airockchip.py
/home/orangepi/phase10/smoke_test_rknn.py
/home/orangepi/rknn-llm/examples/multimodal_model_demo/infer.py
/home/orangepi/rknn-llm/examples/rkllm_server_demo/chat_api_flask.py
/home/orangepi/rknn-llm/rkllm-toolkit/packages/requirements.txt
/home/orangepi/rknn-toolkit2-master/rknn-toolkit-lite2/examples/resnet18/test.py
/home/orangepi/rknn-toolkit2-master/rknpu2/examples/rknn_yolov5_demo/build-linux.sh
/home/orangepi/vlm-sam-industrial-vision-v2/scripts/evaluate_efficientad_rknn_on_board.py
/opt/edge-ai-safety-monitor/opi5-ai/app.py
```

候选模型样式文件名摘要：

```text
/home/orangepi/models/efficientad_models/*/model.rknn
/home/orangepi/models/fastsam_models/fastsam_s.rknn
/home/orangepi/models/fastsam_models/fastsam_s_noquant.rknn
/home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn
/home/orangepi/models/qwen3vl_models/qwen3vl_4b_vision.rknn
/home/orangepi/phase10/models/yolov8n_phase10/yolov8n_metal_nut_cable_best.onnx
/home/orangepi/phase10/models/yolov8n_phase10/yolov8n_metal_nut_cable_best_airockchip_fp16.rknn
/home/orangepi/phase10/models/yolov8n_phase10/yolov8n_metal_nut_cable_best_fp16.rknn
/home/orangepi/rknn-toolkit2-master/rknn-toolkit-lite2/examples/dynamic_shape/mobilenet_v2_for_rk3588.rknn
/home/orangepi/rknn-toolkit2-master/rknn-toolkit-lite2/examples/resnet18/resnet18_for_rk3588.rknn
/home/orangepi/rknn-toolkit2-master/rknn-toolkit2/examples/onnx/yolov5/yolov5s_relu.onnx
```

## 5. 部署记录

本地语法检查：

```text
python3 -m py_compile edge/opi5-ai/service/app.py
结果：通过，无输出。
```

字段核对：

```text
app.py 包含 /health、/api/infer/vision、ok、contract_version、device_id、
frame_id、latency_ms、models、objects、faces、risk_hint、summary、
action_hint、control_allowed。
control_allowed 在响应中固定为 False。
```

部署脚本第一次暴露权限问题：

```text
mkdir: cannot create directory '/opt/edge-ai-safety-monitor': Permission denied
```

修复：`scripts/deploy_opi5.sh` 增加部署目录不可创建/不可写时的 sudo 创建与 `chown` fallback，仍只同步 `edge/opi5-ai/service/app.py`，不同步模型权重或图片。

最终部署输出摘要：

```text
[deploy] 已推送 OPi5 AI 服务 -> orangepi@10.0.1.120:/opt/edge-ai-safety-monitor/opi5-ai/
[deploy] 在 OPi5 上启动(示例): OPI5_AI_PORT=<PORT> python3 /opt/edge-ai-safety-monitor/opi5-ai/app.py
```

## 6. 服务启动方式

部署目录：

```text
/opt/edge-ai-safety-monitor/opi5-ai
```

实际启动命令等价于：

```bash
cd /opt/edge-ai-safety-monitor/opi5-ai
OPI5_AI_PORT=8080 python3 app.py
```

本轮为了保持 SSH 会话退出后服务继续运行，实际使用后台启动：

```bash
nohup env OPI5_AI_PORT=8080 python3 /opt/edge-ai-safety-monitor/opi5-ai/app.py \
  > /opt/edge-ai-safety-monitor/opi5-ai/opi5-ai.log 2>&1 < /dev/null &
```

启动日志摘要：

```text
flask 2.2.5
pid=3818
3818 python3 /opt/edge-ai-safety-monitor/opi5-ai/app.py
* Serving Flask app 'app'
* Debug mode: off
* Running on all addresses (0.0.0.0)
* Running on http://127.0.0.1:8080
* Running on http://10.0.1.120:8080
```

## 7. curl 验证

### 7.1 `/health`

命令摘要：

```bash
curl -sS http://10.0.1.120:8080/health | python3 -m json.tool
```

输出：

```json
{
  "contract_version": "1.0",
  "mode": "mock",
  "model_ready": true,
  "ok": true,
  "service": "opi5-ai-service"
}
```

### 7.2 `/api/infer/vision` 普通 sensors

测试图：

```text
tests/imx6ull/artifacts/2026-06-06_task04_latest.jpg
JPEG image data, baseline, precision 8, 640x480, components 3
```

metadata 摘要：

```json
{"contract_version":"1.0","device_id":"labbox_001","frame_id":1,"source":"imx6ull_v4l2","image_format":"jpeg","resolution":{"width":640,"height":480},"sensors":{"pir":1,"mq2":0,"flame":0},"state_before_ai":"VERIFY","risk_score_before_ai":3,"pan_tilt":{"pan_deg":90,"tilt_deg":90}}
```

输出摘要：

```json
{
  "ok": true,
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "frame_id": 1,
  "latency_ms": 7,
  "models": [{"name": "mock-detector", "version": "mock-0.1"}],
  "objects": [{"label": "person", "score": 0.9, "bbox": [120, 80, 260, 430]}],
  "faces": [],
  "risk_hint": 2,
  "summary": "检测到人员进入巡检区域。",
  "action_hint": "none",
  "control_allowed": false
}
```

### 7.3 `/api/infer/vision` mq2 hazard sensors

metadata 摘要：

```json
{"contract_version":"1.0","device_id":"labbox_001","frame_id":2,"source":"imx6ull_v4l2","image_format":"jpeg","resolution":{"width":640,"height":480},"sensors":{"pir":1,"mq2":1,"flame":0},"state_before_ai":"VERIFY","risk_score_before_ai":5,"pan_tilt":{"pan_deg":90,"tilt_deg":90}}
```

输出摘要：

```json
{
  "ok": true,
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "frame_id": 2,
  "latency_ms": 4,
  "models": [{"name": "mock-detector", "version": "mock-0.1"}],
  "objects": [
    {"label": "person", "score": 0.9, "bbox": [120, 80, 260, 430]},
    {"label": "smoke", "score": 0.66, "bbox": [300, 120, 500, 260]}
  ],
  "faces": [],
  "risk_hint": 4,
  "summary": "检测到人员，且传感器提示烟雾/火焰，建议复核。",
  "action_hint": "keep_alarm",
  "control_allowed": false
}
```

## 8. 边界

- 本轮未接真实 RKNN。
- 本轮未进入 i.MX6ULL→OPi5→Flask 垂直切片。
- AI 不控制执行器，`control_allowed=false`。
- 不提交模型权重、抓拍大图、密码、`inventory.yaml`。
- OPi5 本地目录盘点只记录候选资产，不表示模型或 demo 已通过验证。

## 9. 下一步

- Task05-B：盘点 PC/OPi5 现有 RKNN demo 与模型资源，选择一个最小可接入 demo 并验证运行命令。
- 或 Task06：Flask 契约扩展，视用户决策。
