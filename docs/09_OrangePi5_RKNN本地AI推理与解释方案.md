# 09 Orange Pi 5 RKNN 本地 AI 推理与解释方案

## 9.1 AI 层定位

Orange Pi 5 是第四层本地 AI 节点，负责视觉识别和可选自然语言解释。它不直接控制硬件，只通过 HTTP 返回结构化 JSON。

## 9.2 RKNN 工具链流程

推荐流程：

```text
PC/WSL 或 Windows Python
  → 使用 rknn-toolkit2 转换 ONNX/其他模型为 .rknn
  → 将 .rknn 复制到 OPi5
  → OPi5 使用 RKNN Lite / Runtime 加载模型
  → 提供 HTTP 服务给 i.MX6ULL 调用
```

模型文件不入库，只在 `edge/opi5-ai/models/README.md` 记录来源、路径、版本和转换命令。

Task05-A 已完成 OPi5 mock 服务 bring-up：OPi5 可 SSH 登录，系统为 RK3588 aarch64 Linux，Python 3.10.12、pip 22.0.2、Flask 2.2.5 可用；mock 服务部署在 `/opt/edge-ai-safety-monitor/opi5-ai/`，`/health` 与 `/api/infer/vision` 可从 WSL curl 访问，返回符合 `docs/07` 的 mock JSON，且 `control_allowed=false`。本事实只代表 mock 服务已通过，不代表真实 RKNN 已接入。

Task05-A 只读盘点到的 OPi5 候选资产包括：`/home/orangepi/rknn-toolkit2-master`、`/home/orangepi/rknn-llm`、`/home/orangepi/phase10/models/yolov8n_phase10`、`/home/orangepi/models/efficientad_models`、`/home/orangepi/models/fastsam_models`、`/home/orangepi/models/qwen3vl_models`。其中发现 `.rknn/.onnx` 等模型样式文件名，但未复制进仓库，也未验证可运行性；真实 demo 验证留到 Task05-B。

Task05-B 已完成 Qwen3-VL 2B 真实 AI 部署：OPi5 上 `qwen3vl_2b_vision.rknn`（vision encoder，850MB）+ `qwen3vl_2b_w8a8_base.rkllm`（LLM W8A8，2.37GB）通过 rknn-llm C++ demo 部署并验证；vision encoder 使用 RKNN NPU 3核，LLM 使用 RKLLM runtime；`/health` 和 `/api/infer/vision` 返回真实 VLM 结果（中文场景描述、risk_hint、vlm_result/explanation）；端到端推理 ~13s（含模型加载）；通过子进程调用单次推理 wrapper。详细记录见 `tests/opi5/2026-06-07_qwen3vl_real_ai.md`。

> ~~**[CLAUDE_CODE_TODO | INVESTIGATE]** 盘点 PC 和 OPi5 上已有 RKNN 仓库与可运行 demo~~ 已完成：Task05-B 选择 Qwen3-VL 2B，通过 rknn-llm C++ demo 部署，真实推理通过。详见 `tests/opi5/2026-06-07_qwen3vl_real_ai.md` 和 `edge/opi5-ai/models/README.md`。


## 9.3 目标检测模型

MVP 优先选择目标检测：person、fire、smoke、cat/dog 等可展示类别。若暂无火焰烟雾模型，可先用 person 检测打通链路，并在报告中说明火焰烟雾为扩展类别。

| 方案 | 优先级 | 说明 |
|---|---|---|
| 现有 RKNN demo 目标检测 | 必须优先 | 最快证明 NPU/Runtime 可用 |
| 自定义 fire/smoke 模型 | 推荐 | 需要模型来源与转换时间 |
| 从零训练 | 不做 | 超出两周课程周期 |

## 9.4 人脸识别管线

人脸识别作为第二优先级：

```text
人脸检测（SCRFD/RetinaFace 思路）
  → 人脸裁剪/对齐
  → ArcFace 类特征提取
  → 本地 known_faces 向量库 cosine similarity
  → known / unknown / confidence
```

`known_faces` 属于隐私数据，不入库，只提交目录说明和样例格式。

## 9.5 推理服务 API

实现接口：

```text
GET  /health
POST /api/infer/vision
```

`/health` 示例：

```json
{"ok": true, "service": "opi5-ai-service", "model_ready": true, "mode": "mock_or_rknn"}
```

`/api/infer/vision` 返回格式见 `docs/07_端边HTTP_JSON_Contract.md`。

## 9.6 本地 LLM 解释可选方案

rknn-llm 可作为加分项，把传感器状态和视觉结果转成一句自然语言解释。它只用于 Dashboard 展示：

输入：

```json
{"state":"ALARM","risk_score":7,"sensors":{"pir":1,"mq2":1},"objects":["person","smoke"]}
```

输出：

```json
{"summary":"检测到人员活动并伴随烟雾传感器异常，系统进入报警状态。"}
```

> **[CLAUDE_CODE_TODO | INVESTIGATE]** 确认 rknn-llm 在 OPi5 上是否能稳定运行
> - 为何 GPT 给不了：沙箱无法访问 OPi5 NPU、内存占用和现有 rknn-llm 仓库。
> - 期望产物/操作：优先不纳入 MVP；若有余力，运行最小 demo，记录延迟和内存。
> - 回填位置：docs/09 第 9.6；DEVPLAN 加分项
> - 验收：能在 OPi5 上生成简短文本，且不影响视觉服务稳定性。


## 9.7 AI 失败降级

| 失败 | 响应 |
|---|---|
| 模型未加载 | 返回 503，i.MX 继续本地状态机 |
| 推理超时 | i.MX 记录 `ai_status=timeout` |
| 低置信度 | 只记录，不提升到 ALARM |
| 服务崩溃 | systemd 重启，i.MX 降级 |

## 9.8 Dashboard 展示

Dashboard 显示 AI 摘要，而不是显示“AI 控制结果”。推荐展示：

- 模型名和版本。
- objects/faces 列表。
- `risk_hint`。
- `summary`。
- `latency_ms`。
- 图片框选可作为加分项，不作为 MVP 必需。

## 9.9 模型文件与隐私安全

`.rknn`、`.onnx`、`.pt`、`.pth`、人脸库、抓拍图片、数据集不入库。只提交 README 说明来源和本地路径占位符。
