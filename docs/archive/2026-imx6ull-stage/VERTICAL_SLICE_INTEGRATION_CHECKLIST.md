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

- [x] Task01 完成 legacy 归档。
- [x] Task02 完成 hello 部署。
- [x] Task03 至少完成一路 GPIO 或 mock 输入。
- [x] Task04 完成抓拍或准备测试图片。
- [x] Task05 完成 OPi5 mock/RKNN 服务。
- [x] Task06 完成 Flask 扩展。

## 3. 单点检查

| 检查项 | 命令/方法 | 结果 |
|---|---|---|
| i.MX SSH | `ssh root@192.168.137.110` | 通过 |
| OPi5 health | `curl http://10.0.1.120:8080/health` | 通过（mock 模式） |
| Flask health | `curl http://192.168.137.1:5000/health` | 通过 |
| 图片抓拍 | `v4l2-ctl -d /dev/video1` | 通过（640x480 MJPG, 7.5KB） |
| AI 推理 | `curl -F image=@... http://10.0.1.120:8080/api/infer/vision` | 通过（HTTP 200, risk_hint=2, control_allowed=false） |
| 事件上报 | `curl -d @event.json http://192.168.137.1:5000/api/events` | 通过（HTTP 201, event_id=1031） |

## 4. 完整链路验收

- [x] 产生一条 `state=VERIFY` 或 `state=ALARM` 事件。
- [x] 事件包含 `risk_score`。
- [x] 事件包含 `sensors` 和 `actuators`。
- [x] 事件包含 `vision.frame_id` 或 `image_url`。
- [x] 事件包含 `ai_result.risk_hint` 和 `summary`。
- [x] AI 返回 `control_allowed=false`。
- [x] Dashboard 显示最新事件。
- [x] tests 记录端到端延迟。

## 5. 失败降级验收

- [x] OPi5 停止服务后，i.MX 不崩溃。（OPi5 错误端口测试通过，脚本未崩溃）
- [ ] Flask 停止后，i.MX 记录 spool 或错误日志。（本轮未测试）
- [ ] 摄像头不可用时，事件仍可上报。（本轮未测试，但脚本有 fallback 逻辑）
- [x] 执行器保持默认安全态。（actuators 全部默认值，control_allowed=false）

> **[CLAUDE_CODE_TODO | MEASURE]** 填写垂直切片实际验收结果
> - 为何 GPT 给不了：完整链路必须在三机真实环境中运行。
> - 期望产物/操作：Task07 执行后逐项打勾，补充命令输出和截图路径。
> - 回填位置：VERTICAL_SLICE_INTEGRATION_CHECKLIST.md
> - 验收：所有 MVP 项通过或有明确回退说明。
