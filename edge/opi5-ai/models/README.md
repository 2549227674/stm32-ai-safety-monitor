# OPi5 模型目录说明

此目录只记录模型信息，不提交模型文件。模型权重、tokenizer 不入库。

## 已验证模型

| 模型 | 本地路径（OPi5） | 输入 | 输出 | 状态 |
|---|---|---|---|---|
| Qwen3-VL 2B vision encoder | `/home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn` | 448x448 RGB 图片 | image embeddings (196x2048x4) | Task05-B 已通过 |
| Qwen3-VL 2B base LLM | `/home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_base.rkllm` | image embeddings + text prompt | 中文自然语言描述 | Task05-B 已通过 |
| Qwen3-VL 2B lora_15cls LLM | `/home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_lora_15cls.rkllm` | 同上 | 更简短的检测结果 | 存在但未作为主模型使用 |

## 候选模型（未验证）

| 模型 | 本地路径 | 说明 |
|---|---|---|
| Qwen3-VL 4B vision | `/home/orangepi/models/qwen3vl_models/qwen3vl_4b_vision.rknn` | 883MB，未测试 |
| Qwen3-VL 4B base LLM | `/home/orangepi/models/qwen3vl_models/qwen3vl_4b_w8a8_base.rkllm` | 4.8GB，15GB RAM 可能紧张 |
| EfficientAD | `/home/orangepi/models/efficientad_models/*/model.rknn` | 异常检测 |
| FastSAM | `/home/orangepi/models/fastsam_models/fastsam_s.rknn` | 分割模型 |
| YOLOv8n phase10 | `/home/orangepi/phase10/models/yolov8n_phase10/` | 目标检测（含 .onnx 和 .rknn） |

## 运行方式

Qwen3-VL 通过 rknn-llm 的 C++ demo 部署：

- Vision encoder 使用 RKNN runtime（NPU 3核）
- LLM 使用 RKLLM runtime（W8A8 量化）
- 编译后的单次推理 wrapper：`/opt/edge-ai-safety-monitor/opi5-ai/qwen3vl_single_shot`

## 性能

| 阶段 | 耗时 |
|---|---|
| 图片编码（vision encoder） | ~2.3-2.5s |
| LLM 文本生成 | ~3.5-7.5s |
| 端到端（含模型加载） | ~10-14s |

## 注意

- 模型权重 (.rknn/.rkllm) 不入库
- tokenizer 不入库
- 测试图片不入库
- 当前通过子进程调用，非长期 Python 服务
