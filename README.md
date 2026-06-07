# 基于 i.MX6ULL 与 Orange Pi 5 的端边协同本地 AI 多模态安全巡检系统

> 当前状态：本仓库进入平台迁移阶段。旧 STM32 + ESP32-CAM 方案作为第一版迭代历史归档，新主线为 i.MX6ULL Pro Linux + Orange Pi 5 本地 RKNN。

## 1. 项目一句话定位

本项目面向《数字系统综合训练》课程，以 i.MX6ULL Pro 嵌入式 Linux 为本地安全控制与视觉采集节点，以 Orange Pi 5 为本地 AI 推理节点，以 Flask + SQLite + Dashboard 为本地可视化节点，构建一个不依赖公网云的端边协同安全巡检系统。

## 2. 新旧架构摘要

| 项目阶段 | 第一层 | 第二层 | 第三层 | 第四层 |
|---|---|---|---|---|
| 第一版 legacy | STM32F103C8T6 本地控制 | ESP32-CAM 抓拍/联网 | Flask + SQLite + Dashboard | 云端 AI |
| 当前主线 | i.MX6ULL Linux 本地安全控制 | i.MX6ULL V4L2 + USB 摄像头 + 云台 | Flask + SQLite + Dashboard | Orange Pi 5 RKNN 本地 AI |

## 3. 先读哪几个文件

| 文件 | 给谁看 | 用途 |
|---|---|---|
| `CANONICAL_DECISIONS.md` | 人 + Claude Code | 全局唯一事实源 |
| `CLAUDE_CODE_EXECUTION_GUIDE.md` | Claude Code | 本地执行总入口 |
| `docs/00_README.md` | 人 + 老师 | 设计文档包总览 |
| `docs/07_端边HTTP_JSON_Contract.md` | 人 + Claude Code | 端边 HTTP/JSON 契约权威来源 |
| `DEVPLAN.md` | 人 + Claude Code | 两周实施看板 |
| `CLAUDE_CODE_TASK_01_repo_migration_legacy_archive.md` | Claude Code | 第一步：归档 legacy 并建立新目录 |

## 4. 统一四层架构

| 层级 | 名称 | 物理节点 | 核心职责 | 不承担的职责 |
|---|---|---|---|---|
| 第一层 | Linux 本地安全控制层 | i.MX6ULL Pro | GPIO/I2C/PWM/MOS、传感器采样、执行器控制、`risk_score`、`safety_fsm`、离线降级 | 不运行重型 AI；不承担 Dashboard 主机 |
| 第二层 | Linux 视觉采集与巡检云台层 | i.MX6ULL Pro | USB 摄像头 V4L2 抓帧、PCA9685 控制 MG90 二维云台、图片缓存与上传 | 不做模型推理；不直接覆盖安全状态机 |
| 第三层 | 本地服务器与可视化层 | PC 初期 / Orange Pi 5 最终 | Flask + SQLite + Dashboard、事件存储、图片展示、AI 结果展示 | 不直接控制蜂鸣器、继电器、风扇、水泵 |
| 第四层 | Orange Pi 5 本地 AI 推理与解释层 | Orange Pi 5 16GB + 128G NVMe | RKNN 视觉推理、人脸/目标/异常识别、可选 rknn-llm 解释 | 不直接控制执行器；只返回 `risk_hint` 与解释 |


## 5. 当前真实仓库可复用资产

| 资产 | 当前处理 |
|---|---|
| `server/backend/` | 保留并扩展。现有 Flask 后端已具备 `/health`、`/api/events`、`/api/status/latest`、`/api/images` 和 `/uploads/<filename>`。 |
| 事件 JSON 语义 | 保留 `risk_score/sensors/actuators/state`，演化为端边 HTTP/JSON Contract。 |
| `firmware/stm32/` | 归档到 legacy；BSP 分层、状态机、risk_score 思路可参考，但 HAL/Keil 实现不继续主线。 |
| `firmware/esp32cam/` | 归档到 legacy；WiFi POST 思路可参考，但 ESP32-CAM 不再承担抓拍和联网主线。 |
| `tests/` | 历史测试归档，新建 i.MX6ULL / OPi5 / integration 测试记录。 |

## 6. 最小成功标准

MVP 必须展示一条完整链路：

```text
i.MX6ULL 读取至少 1 路传感器或模拟输入
  → 本地 safety_fsm 计算 state/risk_score
  → V4L2 抓拍或使用测试图片
  → HTTP POST 到 OPi5 `/api/infer/vision`
  → OPi5 返回 mock 或 RKNN AI JSON
  → i.MX6ULL 上报 Flask `/api/events`
  → Dashboard 显示事件、图片链接、AI 摘要
```

## 7. A 类冲刺标准

- 真实 GPIO/I2C/PWM/MOS/云台至少各完成一个可观测演示。
- OPi5 跑通至少一个真实 RKNN demo 或目标检测模型。
- Dashboard 展示图片、AI 识别结果、`risk_score` 和执行器状态。
- 逻辑分析仪记录 I2C/PWM/GPIO 关键波形。
- 报告中把 STM32/ESP32 阶段写成工程迭代历史，不写成失败。

## 8. 关键待确认

> **[CLAUDE_CODE_TODO | DECIDE]** 确认最终演示是否必须完全脱离 PC
> - 为何 GPT 给不了：最终后端部署位置影响 Dashboard、SQLite 与录屏演示方案；沙箱无法知道课程现场条件。
> - 期望产物/操作：用户决定最终演示形态：PC 运行 Flask，或 OPi5 同时运行 Flask + AI。
> - 回填位置：README.md 第 8 节；DEVPLAN 最终演示阶段；docs/13 演示脚本
> - 验收：明确写入“最终演示部署位置”，并更新相关 Task。


> **[CLAUDE_CODE_TODO | FILL]** 填写三机局域网地址与 SSH 用户
> - 为何 GPT 给不了：真实 IP、用户名、端口和密钥路径只能在本地网络中确认，不能入库为真实值。
> - 期望产物/操作：创建 `config/inventory.example.yaml`，本地复制为不入库的 `inventory.yaml` 后填入 `IMX_IP=<TODO:FILL>`、`OPI5_IP=<TODO:FILL>`、`BACKEND_HOST=<TODO:FILL>`。
> - 回填位置：README.md、CLAUDE.md、Task02/05/07 的命令占位符
> - 验收：`ssh <USER>@<IMX_IP>` 与 `ssh <USER>@<OPI5_IP>` 均能登录；真实值只保存在本地。


## 9. 入库安全

见 `.gitignore`。禁止提交模型权重、抓拍图片、数据库、SSH 私钥、真实 IP、WiFi 密码。
