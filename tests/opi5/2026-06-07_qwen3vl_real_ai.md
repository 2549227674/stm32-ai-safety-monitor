# 2026-06-07 Task05-B Qwen3-VL 2B 真实 AI 部署测试记录

## 1. 分支与提交

- 分支：`migration/imx6ull-opi5-edge-ai`
- 当前阶段：Task05-B Qwen3-VL 2B 真实 AI 部署
- 基线：`9e3ef95 docs: record task07-d native c upgrade roadmap`

## 2. 结论

- OPi5 环境检查：通过
- Qwen3-VL 资产盘点：通过
- Qwen3-VL demo：通过
- Qwen3-VL 接入 /api/infer/vision：通过
- AI_BACKEND=mock 回归：通过
- AI_BACKEND=qwen3vl：通过
- /health：通过
- /api/infer/vision：通过
- vlm_result / explanation：通过
- i.MX imx_safetyd 回归：未做（本轮只验证 OPi5 服务端）
- control_allowed=false：通过
- 未提交模型/图片/tokenizer/数据集/inventory：确认

## 3. OPi5 环境

```text
Linux orangepi5 6.1.99-rockchip-rk3588 #1.2.2 SMP Thu Jun 26 14:42:53 CST 2025 aarch64 GNU/Linux
Ubuntu 22.04.5 LTS (Jammy Jellyfish)
Python 3.10.12
pip 22.0.2

numpy OK 2.2.6
cv2 OK 4.13.0
flask OK 2.2.5
rknnlite OK (no version)
rknn FAIL (not installed, not needed for rkllm path)
transformers FAIL (not installed, not needed for C++ demo)
tokenizers FAIL (not installed)
PIL OK 12.2.0

NPU: RKNPU fdab0000.npu, rknpu driver 0.9.8, platform RK3588
RGA: rga3 + rga2 probed successfully
/dev/dri: renderD128, renderD129
```

## 4. Qwen3-VL 2B 资产清单

| 项目 | 路径 | 是否存在 | 说明 |
|---|---|---|---|
| 模型目录 | `/home/orangepi/models/qwen3vl_models/` | 是 | |
| vision encoder (2B) | `qwen3vl_2b_vision.rknn` | 是 | 850MB |
| LLM base (2B) | `qwen3vl_2b_w8a8_base.rkllm` | 是 | 2.37GB, W8A8 |
| LLM lora_15cls (2B) | `qwen3vl_2b_w8a8_lora_15cls.rkllm` | 是 | 2.37GB, W8A8, 15类微调 |
| vision encoder (4B) | `qwen3vl_4b_vision.rknn` | 是 | 883MB |
| LLM base (4B) | `qwen3vl_4b_w8a8_base.rkllm` | 是 | 4.8GB |
| LLM lora_15cls (4B) | `qwen3vl_4b_w8a8_lora_15cls.rkllm` | 是 | 4.8GB |
| rknn-llm 仓库 | `/home/orangepi/rknn-llm/` | 是 | 含 deploy demo |
| multimodal demo | `rknn-llm/examples/multimodal_model_demo/` | 是 | 含 C++ deploy 源码 |
| demo README | `multimodal_model_demo/README.md` | 是 | |
| 测试图片 | demo.jpg | 是 | 随 demo 自带 |

## 5. Qwen3-VL demo 运行

### 5.1 原始交互式 demo

编译方式：在 OPi5 上原生编译（aarch64 g++ 11.4 + cmake 3.22），从原 deploy 目录构建。

```bash
cd /home/orangepi/rknn-llm/examples/multimodal_model_demo/deploy
sudo mkdir build && cd build
sudo cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64
sudo make -j4 && sudo make install
```

运行命令：

```bash
export LD_LIBRARY_PATH=./lib
echo "请用一句话描述画面内容。
exit" | ./demo demo.jpg \
  /home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn \
  /home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_base.rkllm \
  2048 4096 3 "<|vision_start|>" "<|vision_end|>" "<|image_pad|>"
```

输出：

```text
rkllm init success
LLM Model loaded in  6579.48 ms
ImgEnc Model loaded in  2676.09 ms
ImgEnc Model inference took  3004.74 ms

robot: 画面中显示一个室内场景，有一个人在操作设备，周围环境较为昏暗，空气中弥漫着淡淡的烟雾，但没有明显的火焰或爆炸迹象。该人穿着深色衣物，正在使用手持工具进行作业。

存在人员：是的，画面中有1名人员。
存在烟雾：是的，空气中弥漫着烟雾。
不存在火焰：没有可见的火焰。
是否存在明显安全异常：虽然有烟雾，但未见火灾或爆炸等严重安全异常。整体环境看似正常，仅因操作产生轻微烟雾。

结论：存在人员、存在烟雾，无火焰或明显安全异常。
```

### 5.2 单次模式 C++ wrapper

编写 `qwen3vl_single_shot.cpp`，基于原始 demo 的 image_enc 和 rkllm API，改为单次推理+JSON 输出。通过 cmake 原生编译。

运行命令：

```bash
cd /opt/edge-ai-safety-monitor/opi5-ai
./qwen3vl_single_shot /tmp/test_capture.jpg \
  /home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn \
  /home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_base.rkllm \
  "请用一句话描述画面内容，并判断是否存在人员、烟雾、火焰或明显安全异常。" 3
```

输出：

```json
{"ok":true,"text":"画面中是一个非常昏暗的室内场景...未观察到人员、烟雾、火焰或明显安全异常。","total_ms":12948,"imgenc_ms":2407,"llm_ms":7483,"llm_load_ms":2408,"enc_load_ms":638}
```

### 5.3 关键性能指标

| 阶段 | 耗时 | 说明 |
|---|---|---|
| LLM 模型加载 | ~2200-6500 ms | 首次较慢，后续有缓存 |
| Vision encoder 加载 | ~640-980 ms | |
| 图片编码推理 | ~2200-2500 ms | 448x448 输入 |
| LLM 文本生成 | ~3500-7500 ms | 取决于输出长度 |
| 端到端（含加载） | ~10000-14000 ms | 单次进程模式 |
| 端到端（不含加载） | ~5700-10000 ms | |

### 5.4 lora_15cls 模型测试

```bash
./qwen3vl_single_shot /tmp/test_capture.jpg \
  /home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn \
  /home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_lora_15cls.rkllm \
  "请用一句话描述画面内容，并判断是否存在人员、烟雾、火焰或明显安全异常。" 3
```

输出：

```text
画面中显示一个正在燃烧的物体，周围有明显的火光和浓烟。
存在人员、烟雾、火焰或明显安全异常。
```

注：lora_15cls 模型更倾向于输出检测结果，但可能产生幻觉（测试图实际为暗光室内无火焰）。本轮选择 base 模型。

## 6. 服务接入

### 6.1 新增/修改文件

| 文件 | 说明 |
|---|---|
| `edge/opi5-ai/service/qwen3vl_single_shot.cpp` | C++ 单次推理 wrapper，输出 JSON |
| `edge/opi5-ai/service/qwen3vl_backend.py` | Python wrapper，调用 C++ binary |
| `edge/opi5-ai/service/risk_mapping.py` | VLM 文本 → risk_hint/labels 映射 |
| `edge/opi5-ai/service/app.py` | 更新：支持 AI_BACKEND=mock/qwen3vl |
| `scripts/deploy_opi5.sh` | 更新：同步新 Python 文件 |

### 6.2 部署文件

```text
/opt/edge-ai-safety-monitor/opi5-ai/
  app.py                    # Flask 服务（mock/qwen3vl 双模式）
  qwen3vl_backend.py        # Python → C++ binary wrapper
  risk_mapping.py           # VLM 文本 → risk 映射
  qwen3vl_single_shot       # C++ 二进制（RKNN vision + RKLLM LLM）
  librknnrt.so              # RKNN runtime
  librkllmrt.so             # RKLLM runtime
```

### 6.3 启动命令

```bash
# mock 模式
cd /opt/edge-ai-safety-monitor/opi5-ai
AI_BACKEND=mock OPI5_AI_PORT=8080 python3 app.py

# qwen3vl 模式
cd /opt/edge-ai-safety-monitor/opi5-ai
AI_BACKEND=qwen3vl OPI5_AI_PORT=8080 python3 app.py
```

### 6.4 AI_BACKEND=mock 回归

```bash
curl http://10.0.1.120:8080/health
```

```json
{
  "contract_version": "1.0",
  "mode": "mock",
  "model_ready": true,
  "ok": true,
  "service": "opi5-ai-service"
}
```

```bash
curl -F image=@test.jpg \
  -F 'metadata={"device_id":"labbox_001","frame_id":1,"sensors":{"pir":1}}' \
  http://10.0.1.120:8080/api/infer/vision
```

```json
{
  "ok": true,
  "mode": "mock",
  "risk_hint": 2,
  "objects": [{"label": "person", "score": 0.9, "bbox": [120, 80, 260, 430]}],
  "control_allowed": false
}
```

### 6.5 AI_BACKEND=qwen3vl /health

```json
{
  "contract_version": "1.0",
  "mode": "qwen3vl",
  "model_ready": true,
  "models": [
    {"backend": "rknn-llm", "name": "qwen3-vl-2b", "version": "local"}
  ],
  "ok": true,
  "service": "opi5-ai-service"
}
```

### 6.6 AI_BACKEND=qwen3vl /api/infer/vision

```json
{
  "ok": true,
  "contract_version": "1.0",
  "device_id": "labbox_001",
  "frame_id": 503,
  "latency_ms": 14214,
  "models": [{"name": "qwen3-vl-2b", "version": "local", "backend": "rknn-llm"}],
  "objects": [],
  "faces": [],
  "risk_hint": 0,
  "summary": "画面中是一个非常昏暗的室内场景...未观察到人员、烟雾、火焰或明显安全异常。",
  "explanation": "...(完整 VLM 输出)...",
  "vlm_result": {
    "model": "qwen3-vl-2b",
    "description": "...",
    "labels": [],
    "confidence": null,
    "raw_text": "...",
    "timing": {
      "total_ms": 12908,
      "imgenc_ms": 2490,
      "llm_ms": 7138
    }
  },
  "action_hint": "none",
  "control_allowed": false
}
```

## 7. 契约核验

| 字段 | 值 | 说明 |
|---|---|---|
| ok | true | |
| mode | qwen3vl | |
| model_ready | true | |
| models | `[{"name":"qwen3-vl-2b","backend":"rknn-llm","version":"local"}]` | |
| summary | VLM 真实输出 | 中文场景描述 |
| explanation | VLM 完整输出 | 向后兼容新增字段 |
| vlm_result | 存在 | 向后兼容新增字段 |
| risk_hint | 0-2（正常场景）| 由 risk_mapping 从 VLM 文本映射 |
| action_hint | "none" | |
| control_allowed | false | 安全红线 |
| objects | [] | Qwen3-VL 不提供 bbox，不伪造 |
| faces | [] | |

## 8. i.MX / Flask 回归

本轮未执行 i.MX imx_safetyd 回归测试。OPi5 服务端接口与 Task07 时一致（/health + /api/infer/vision），i.MX C 主控无需修改即可调用。

## 9. 边界

- 本轮没有训练模型。
- 本轮没有提交 .rknn/.rkllm/.onnx/.pt/.pth/.bin/tokenizer。
- 本轮没有提交抓拍图片、人脸库、数据集。
- Qwen3-VL 主要提供图像解释和风险提示，不保证 bbox。
- objects[] 为空，因为 Qwen3-VL 是 VLM 不是目标检测模型。
- objects[].bbox 保留给 YOLO/目标检测模型后续增强。
- AI 仍不控制执行器，control_allowed=false。
- 当前接入方式为 C++ 子进程调用（每次请求加载模型），不是长期原生 Python 库服务。
- 模型加载耗时 ~10s，端到端推理 ~13s；后续可优化为持久化进程。
- risk_mapping 通过关键词+否定检测实现，不是精确 NLP。

## 10. 下一步

- Task05-C：优化推理延迟（持久化进程、减少模型加载开销）。
- Task05-D：补 YOLO/RKNN 目标检测 bbox。
- Task09：最终演示脚本与证据包。
