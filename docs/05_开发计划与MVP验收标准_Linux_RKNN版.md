# 05 开发计划与 MVP 验收标准（Linux + RKNN 版）

## 5.1 MVP 定义

MVP 是一条可演示、可截图、可录屏的端边闭环，不追求一次性完成全部硬件和全部 AI：

```text
i.MX6ULL 事件/传感器输入
  → 本地 state/risk_score
  → V4L2 抓拍或测试图片
  → OPi5 mock/RKNN 推理
  → Flask 记录
  → Dashboard 显示
```

## 5.2 两周开发计划

| 时间 | 任务 | 输出 |
|---|---|---|
| Day 1 | Task01 | legacy 归档、新目录、迁移记录 |
| Day 1–2 | Task02 | WSL SDK、SSH、hello 部署 |
| Day 3–4 | Task03 | GPIO/I2C/PWM/MOS/舵机基础验证 |
| Day 5 | Task04 | V4L2 抓拍图片 |
| Day 6–7 | Task05 | OPi5 AI 服务 mock + 真实 RKNN 探查 |
| Day 8 | Task06 | Flask Contract 扩展 |
| Day 9–10 | Task07 | 端边垂直切片 |
| Day 10–11 | Task08 | 云台巡检演示 |
| Day 12–14 | 报告/PPT/录屏 | 完成提交物 |

## 5.3 最小验收标准

| 编号 | 标准 | 验收证据 |
|---|---|---|
| M1 | 迁移分支目录清晰 | `git status`、目录树截图 |
| M2 | i.MX6ULL 可部署程序 | hello 运行记录 |
| M3 | 至少一路硬件 IO 可控/可读 | tests/imx6ull 记录 |
| M4 | V4L2 可抓图 | 图片文件与格式记录 |
| M5 | OPi5 AI 服务可返回 JSON | curl 记录 |
| M6 | Flask 可显示新事件 | Dashboard 截图 |
| M7 | AI 不直接控制执行器 | Contract 中 `control_allowed=false` |

## 5.4 A 类冲刺标准

- 真实 RKNN 目标检测跑通。
- 云台巡检有视频或照片序列。
- 逻辑分析仪有 I2C/PWM 证据。
- 离线降级演示：断开 OPi5，i.MX6ULL 仍能本地报警并缓存事件。
- 报告中有平台迁移决策表和风险矩阵。

## 5.5 回滚策略

| 风险 | 回滚 |
|---|---|
| RKNN 接不通 | 保留 mock AI 服务完成端边闭环 |
| 摄像头格式不支持 | 先用测试图片或降分辨率 |
| 云台供电不稳 | 云台改为静态摄像头，不影响 MVP |
| Flask schema 迁移风险 | 新字段先存 raw_json 并前端解析 |
| i.MX6ULL SDK 不可用 | 先在板端原生编译或写 Python 骨架，后补交叉编译 |

> **[CLAUDE_CODE_TODO | DECIDE]** 确认 MVP 是否允许 AI 服务先 mock 再替换 RKNN
> - 为何 GPT 给不了：这取决于老师对 AI 展示深度的要求和现有 RKNN demo 可用性。
> - 期望产物/操作：用户确认答辩底线：mock 只作为联调手段，最终是否必须跑真 RKNN。
> - 回填位置：docs/05 第 5.1/5.3；DEVPLAN P4
> - 验收：MVP 标准与最终报告措辞一致。
