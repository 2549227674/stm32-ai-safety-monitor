# CLAUDE.md — 本地 Claude Code 工作指南

## 1. 项目主线

当前主线是 `Edge AI Safety Monitor`：i.MX6ULL Pro 负责 Linux 本地安全控制、V4L2 抓拍与云台；Orange Pi 5 负责 RKNN 本地推理；Flask 后端保留并扩展。旧 STM32 / ESP32-CAM 工程只作为 legacy 归档，不再作为主线修复目标。

必须先读：

1. `CANONICAL_DECISIONS.md`
2. `CLAUDE_CODE_EXECUTION_GUIDE.md`
3. 当前要执行的 `CLAUDE_CODE_TASK_xx_*.md`
4. 与任务相关的 `docs/0x_*.md`

## 2. 仓库工作方式

推荐分支：

```bash
git checkout main
git pull
git checkout -b migration/imx6ull-opi5-edge-ai
```

不要在 `main` 上直接大规模移动目录。先执行 Task 01 归档旧代码，再进入新主线。

## 3. 允许做的事

- 重写根目录文档与 docs/00–14。
- 移动旧 `firmware/stm32/`、`firmware/esp32cam/` 到 `legacy/2026-stm32-esp32/`。
- 在 `edge/imx6ull-controller/` 新建 Linux 控制程序。
- 在 `edge/opi5-ai/` 新建 OPi5 AI 服务。
- 在 `common/contracts/` 维护 Contract 文档或 schema。
- 在 `server/backend/` 增量扩展字段与页面，保持旧接口兼容。
- 在 `scripts/` 添加 build/deploy 脚本，环境相关值用占位符或读取本地配置。
- 在 `tests/` 写入真实测试记录。

## 4. 禁止做的事

| 禁止事项 | 原因 |
|---|---|
| 不继续死磕 STM32→ESP32-CAM UART 桥接 | 该链路已退出新主线；只归档事实 |
| 不删除旧代码历史 | legacy 是答辩中的工程迭代证据 |
| 不重写 Flask 为另一个框架 | 现有 Flask + SQLite + Dashboard 是最可复用资产 |
| 不提交真实 IP/密码/密钥/模型权重/图片/数据库 | 入库安全 |
| 不让 OPi5/Flask 直接控制执行器 | 安全闭环必须留在 i.MX6ULL |
| 不把未实测硬件写成已通过 | 防止报告与真实状态不一致 |

## 5. 命令与配置原则

所有真实环境变量从不入库的本地配置读取。仓库内只放 `.example` 文件。

```bash
# 示例：真实值不要写死到脚本中
cp config/inventory.example.yaml config/inventory.yaml
# 编辑本地 inventory.yaml，禁止提交
```

Task02 已在 WSL 中确认 i.MX6ULL SDK 可交叉编译。当前 SDK 未提供 `environment-setup-*`，本地
`config/inventory.yaml` 使用 `imx6ull.cc` 指向 `arm-buildroot-linux-gnueabihf-gcc`，`scripts/build_imx6ull.sh`
已支持该 fallback。证据见 `tests/imx6ull/2026-06-06_toolchain_ssh.md`。


## 6. 端边 Contract 原则

`docs/07_端边HTTP_JSON_Contract.md` 是唯一权威来源。代码实现字段必须和 docs/07 同步。旧事件字段继续支持：

```json
{"type":"event","device_id":"labbox_001","seq":1,"state":"NORMAL","risk_score":0,"need_snap":false,"sensors":{},"actuators":{}}
```

新增字段只允许向后兼容，不允许破坏旧 Dashboard 事件显示。

## 7. 完成任务后的固定回填

每完成一个 Task：

1. 更新 `AGENTS.md` 状态表。
2. 更新 `DEVPLAN.md` 看板。
3. 在 `tests/<scope>/YYYY-MM-DD_*.md` 记录真实命令、结果、截图/波形路径。
4. 移除或关闭对应 `[CLAUDE_CODE_TODO]`。
5. 提交信息使用：`taskXX: 简短说明`。
