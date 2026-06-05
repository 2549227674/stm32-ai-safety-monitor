# Task 07：端边垂直切片联调

## 目标 / 范围

打通 i.MX6ULL → OPi5 → Flask → Dashboard 的最小完整链路。可先使用模拟传感器和 mock AI，再替换真实硬件和真 RKNN。

## 前置条件

Task02–06 已通过。至少具备：i.MX 可运行程序、OPi5 `/api/infer/vision` 可返回 JSON、Flask 可显示新事件。

## 操作步骤

1. 在 i.MX 控制端生成一个测试事件。
2. 抓拍或使用测试图片。
3. POST 到 OPi5。
4. 接收 AI JSON 并融合 `risk_score`。
5. POST 到 Flask。
6. Dashboard 验证。
7. 记录端到端延迟。

## 命令骨架

```bash
# 在 i.MX6ULL 上运行
./imx_safetyd --config <IMX_CONFIG_TODO_FILL> --once --test-event
# 在 PC 上观察
curl http://<BACKEND_HOST_TODO_FILL>:5000/api/events?limit=5
```

## 产出物

- `edge/imx6ull-controller/src/imx_safetyd.*` 初版
- `tests/integration/YYYY-MM-DD_end_edge_vertical_slice.md`
- Dashboard 截图路径记录

## 验收标准

- [ ] Dashboard 出现完整事件。
- [ ] 事件包含 sensors/actuators/risk_score/vision/ai_result。
- [ ] AI 结果中 `control_allowed=false`。
- [ ] 端到端延迟有记录。

## 禁止事项

- 不绕过 i.MX 状态机让 OPi5 直接上报控制结果。
- 不把失败请求吞掉不记录。
- 不提交真实图片大文件。

## 完成后回填

- 回填 docs/06 阈值与实际状态。
- 回填 docs/11 联调结果。
- 更新 AGENTS.md 和 DEVPLAN P6。
