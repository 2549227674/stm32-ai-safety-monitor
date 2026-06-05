# 端边垂直切片集成验收清单

## 1. 目标链路

```text
i.MX6ULL 传感器/模拟事件
  → safety_fsm
  → V4L2 抓拍
  → OPi5 AI 服务
  → i.MX6ULL 融合
  → Flask /api/events + /api/images
  → Dashboard 展示
```

## 2. 前置条件

- [ ] Task01 完成 legacy 归档。
- [ ] Task02 完成 hello 部署。
- [ ] Task03 至少完成一路 GPIO 或 mock 输入。
- [ ] Task04 完成抓拍或准备测试图片。
- [ ] Task05 完成 OPi5 mock/RKNN 服务。
- [ ] Task06 完成 Flask 扩展。

## 3. 单点检查

| 检查项 | 命令/方法 | 结果 |
|---|---|---|
| i.MX SSH | `ssh <IMX_USER_TODO_FILL>@<IMX_IP_TODO_FILL>` | `<TODO:MEASURE>` |
| OPi5 health | `curl http://<OPI5_IP_TODO_FILL>:<PORT_TODO_FILL>/health` | `<TODO:MEASURE>` |
| Flask health | `curl http://<BACKEND_HOST_TODO_FILL>:5000/health` | `<TODO:MEASURE>` |
| 图片抓拍 | `./v4l2_capture` | `<TODO:MEASURE>` |
| AI 推理 | `curl -F image=@... /api/infer/vision` | `<TODO:MEASURE>` |
| 事件上报 | `curl -d @event.json /api/events` | `<TODO:MEASURE>` |

## 4. 完整链路验收

- [ ] 产生一条 `state=VERIFY` 或 `state=ALARM` 事件。
- [ ] 事件包含 `risk_score`。
- [ ] 事件包含 `sensors` 和 `actuators`。
- [ ] 事件包含 `vision.frame_id` 或 `image_url`。
- [ ] 事件包含 `ai_result.risk_hint` 和 `summary`。
- [ ] AI 返回 `control_allowed=false`。
- [ ] Dashboard 显示最新事件。
- [ ] tests 记录端到端延迟。

## 5. 失败降级验收

- [ ] OPi5 停止服务后，i.MX 不崩溃。
- [ ] Flask 停止后，i.MX 记录 spool 或错误日志。
- [ ] 摄像头不可用时，事件仍可上报。
- [ ] 执行器保持默认安全态。

> **[CLAUDE_CODE_TODO | MEASURE]** 填写垂直切片实际验收结果
> - 为何 GPT 给不了：完整链路必须在三机真实环境中运行。
> - 期望产物/操作：Task07 执行后逐项打勾，补充命令输出和截图路径。
> - 回填位置：VERTICAL_SLICE_INTEGRATION_CHECKLIST.md
> - 验收：所有 MVP 项通过或有明确回退说明。
