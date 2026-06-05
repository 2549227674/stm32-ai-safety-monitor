# Task 08：MG90 二维云台巡检演示

## 目标 / 范围

在 PCA9685 + MG90 云台上实现简单巡检扫描，每个角度抓拍并请求 AI，选择目标方向停留。本任务是展示增强，Task07 未完成前不要开始。

## 前置条件

Task03 PWM/舵机通过；Task04 V4L2 通过；Task05 AI 服务通过；Task07 完整链路通过。

## 操作步骤

1. 确认舵机独立供电。
2. 设定 pan/tilt 安全角度范围。
3. 执行三点扫描。
4. 每个角度停稳后抓拍。
5. 请求 AI。
6. 根据最高置信度停留。
7. Dashboard 显示 pan/tilt 与 AI 结果。

## 命令骨架

```bash
./imx_safetyd --config <IMX_CONFIG_TODO_FILL> --demo-pan-tilt
# 或单独测试
./pca9685_test --channel <PAN_CH_TODO_FILL> --angle 90
```

## 产出物

- 云台扫描代码
- `tests/imx6ull/YYYY-MM-DD_pan_tilt_demo.md`
- 演示视频/照片路径记录

## 验收标准

- [ ] pan/tilt 三点动作稳定。
- [ ] 每点能抓拍或记录失败原因。
- [ ] AI 结果带 `pan_tilt` 字段。
- [ ] 不掉电、不抖动过大。

## 禁止事项

- 不让舵机从板卡供电。
- 不让舵机超出机械限位。
- 不在云台不稳时强行长时间运行。

## 完成后回填

- 回填 docs/08 云台参数。
- 回填 docs/13 演示脚本。
- 更新 AGENTS.md 和 DEVPLAN P7。
