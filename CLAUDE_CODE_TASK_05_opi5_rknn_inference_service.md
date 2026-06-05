# Task 05：Orange Pi 5 RKNN 推理服务

## 目标 / 范围

在 OPi5 上部署 AI 服务，先实现 mock `/api/infer/vision`，再调查并接入一个现有 RKNN demo。本任务不要求一开始完成人脸识别或 LLM。

## 前置条件

Task01 已完成；OPi5 可上电联网；推荐 PC/WSL 可 SSH 到 OPi5。

> **[CLAUDE_CODE_TODO | INVESTIGATE]** 确认 OPi5 系统与 RKNN 依赖
> - 为何 GPT 给不了：沙箱无法访问 OPi5。
> - 期望产物/操作：运行系统版本、Python 版本、已有 RKNN demo 盘点。
> - 回填位置：Task05 前置条件；docs/09
> - 验收：形成 OPi5 资产清单和首选 demo。


## 操作步骤

1. 创建 `edge/opi5-ai/service/app.py`。
2. 实现 `/health`。
3. 实现 mock `/api/infer/vision`。
4. 写部署脚本。
5. PC curl 上传图片测试。
6. 调查现有 RKNN demo 并逐步替换 mock。

## 命令骨架

```bash
ssh <OPI5_USER_TODO_FILL>@<OPI5_IP_TODO_FILL>
python3 --version
mkdir -p /opt/edge-ai-safety-monitor/opi5-ai
# 部署后测试
curl http://<OPI5_IP_TODO_FILL>:<OPI5_AI_PORT_TODO_FILL>/health
curl -F image=@<TEST_IMAGE_TODO_FILL> -F metadata='{"device_id":"labbox_001","frame_id":1}' http://<OPI5_IP_TODO_FILL>:<OPI5_AI_PORT_TODO_FILL>/api/infer/vision
```

## 产出物

- `edge/opi5-ai/service/app.py`
- `scripts/deploy_opi5.sh`
- `edge/opi5-ai/models/README.md`
- `tests/opi5/YYYY-MM-DD_ai_service.md`

## 验收标准

- [ ] `/health` 返回 ok。
- [ ] mock infer 返回符合 docs/07 的 JSON。
- [ ] 至少完成 RKNN 资产盘点。
- [ ] 真实模型若接入，记录延迟。

## 禁止事项

- 不提交模型权重。
- 不把 mock 写成真实 RKNN 已通过。
- 不让 AI 控制执行器。

## 完成后回填

- 回填 docs/09 模型清单。
- 更新 AGENTS.md OPi5 状态。
- 更新 DEVPLAN P4。
