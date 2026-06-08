# Task 12：YOLO 火源检测 + 低速视觉锁定 + 双确认喷水 + 低帧视觉流（垂直切片）

> 拟落库路径：`CLAUDE_CODE_TASK_12_yolo_fire_track_spray.md`
> 配套设计评审：`tests/integration/2026-06-08_yolo_track_spray_design.md`（**先读，且 §14 评审决议通过后方可执行 12-E**）
> 唯一事实源：`CANONICAL_DECISIONS.md`；契约：`docs/07`；状态机：`docs/06`
> 分支：`migration/imx6ull-opi5-edge-ai`

## 目标 / 范围

在 OPi5 上新增 `yolo_rknn` 目标检测后端（真·火源 bbox），i.MX6ULL 新增 `imx_tracker` 进程做**低速视觉锁定**云台辅助指向，并在**本地传感器 + 视觉双确认**下由 `imx_safetyd` 本地喷水；Dashboard 以**低帧 MJPEG 标注流 + bbox 叠框**展示。按 mock 优先垂直切片推进，一片不过不进下一片。

**不在本任务**：训练模型（硬规则禁止从零训练）；改 DB schema；恢复 fan；动 P0/P1 已验证的本地安全闭环规则（只在 12-E 受控新增喷水闸）。

## 不可违反（贯穿所有切片）

1. **`control_allowed` 恒 `false`**；AI/OPi5/Flask 绝不直接驱动任何执行器。
2. **`imx_safetyd` 安全 FSM 独立于网络/AI**；停 OPi5/停 tracker/拔网，`flame=1 或 mq2=1` 仍本地直接 ALARM + 蜂鸣器/RGB。
3. **喷水必要条件永远是本地 flame/mq2**；YOLO/视觉对喷水**永不充分**。
4. **`imx_safetyd` 独占 PCA9685（/dev/i2c-0 0x40）**；`imx_tracker` 只经本地 IPC 提议云台位姿，safetyd 二次限幅后执行。
5. 舵机脉宽 **1100–1900us 不破**；先 tilt 后 pan；单步限幅 **≤10° pan / ≤5° tilt**。
6. **不提交**模型权重 `.pt/.onnx/.rknn`、抓拍图、数据集、tokenizer、真实 IP（CANONICAL 0.8）。
7. 代码注释用**英文**；用户代码放 `USER CODE`/既有 BSP 风格区域；不引用 CANONICAL 之外的引脚/通道。

## 资源（以 CANONICAL 0.7 为准，禁止另造）

```text
摄像头:   USB UVC /dev/video1（原生 MJPEG 优先，禁止 YUYV 软编码）
云台:     PCA9685 /dev/i2c-0 0x40  CH0=pan CH1=tilt（imx_safetyd 独占）
喷水:     PCA9685 CH6 → MOS → 水泵+水枪并联，active high，默认 OFF（imx_safetyd 驱动）
报警:     蜂鸣器 gpio122 active low（不经 PCA9685）；RGB CH2/3/4
本地输入: flame=gpio119  mq2=gpio120  pir=gpio117  door=gpio118
OPi5:     orangepi@10.0.1.120（真实 IP 走 inventory.yaml，不入库）；NPU RK3588
网络:     PC 192.168.137.1（ICS）/ i.MX 192.168.137.110 / OPi5 10.0.1.120（PC 已配静态路由）
```

## 切片顺序与门禁

| 切片 | 内容 | 触 safetyd | 门禁 |
|---|---|---|---|
| 12-A | OPi5 `yolo_rknn` **mock** 后端 + `/api/track/frame` + `/api/track/stream`（mock 标注 MJPEG） | 否 | 可立即执行 |
| 12-B | Dashboard 真实 bbox 叠框 + 锁定标记 + `alarm_phase` + 嵌低帧流 | 否 | 可立即执行（依赖 12-A 契约） |
| 12-C | 接**真火源权重**（luminous0219 yolov8n，airockchip→rknn） | 否 | **VERIFY 门控**（权重转得动/跑得稳） |
| 12-D | `imx_tracker`：抓帧转发 + 经 IPC 调云台，**只瞄准不喷水** | 仅加 IPC 入口 | 依赖 12-A/12-C |
| 12-E | `imx_safetyd` 双确认喷水闸（限时 + 冷却 + 降级） | **是** | **设计评审 §14 通过后**才执行 |
| 12-F | 调优与实测回填 | — | 收尾 |

**未通过当前切片不进入下一切片。**

---

## 12-A：OPi5 `yolo_rknn` mock 后端 + 追踪端点（可立即执行）

### 范围
不依赖真权重，先用 mock 把契约与两个新端点跑通；不破坏现有 `mock`/`qwen3vl` 后端。

### 操作步骤
1. 新增 `edge/opi5-ai/service/yolo_rknn_backend.py`：
   - 提供 `detect(image_bytes) -> {objects:[{label,score,bbox[x1,y1,x2,y2]}], best_fire}`。
   - **mock 模式**（无权重时）：返回伪 `fire` bbox（带轻微随机抖动，便于看锁定动起来），`score≈0.8`。
   - 预留真实推理入口（12-C 接），权重路径从环境变量读，**不硬编码、不入库**。
2. 新增 label→risk 映射（**0–10 量纲**，勿用 0–100）：`fire/smoke` 高（如 7/6），`person` 低（2）。
3. 改 `edge/opi5-ai/service/app.py`：
   - `AI_BACKEND` 增加 `yolo_rknn`；`mock`/`qwen3vl` 分支保持不变。
   - 新增 `POST /api/track/frame`：收 `image`+`metadata`，调 yolo 后端，按 bbox 算 `tracking_hint`（`target_center/frame_center/offset_px/target_label/confidence`），返回**纯感知** JSON，`control_allowed=false`，**不含 pan/tilt delta**。
   - 新增 `GET /api/track/stream`：`multipart/x-mixed-replace`，把最近一帧**标注后的 JPEG**（画 bbox）从环形缓冲推出，建议 ≤5fps、640×480；无帧时推占位图。
   - `/health` 增加 `mode=yolo_rknn` 分支与 `model_ready` 检测。
4. `/api/infer/vision` 保持：`yolo_rknn` 后端在此路径返回真实 `objects[].bbox`（VLM 路径仍 `objects=[]`，不伪造）。

### 产出物
- `edge/opi5-ai/service/yolo_rknn_backend.py`（新）
- `edge/opi5-ai/service/app.py`（改：后端分发 + 两端点）
- `tests/opi5/YYYY-MM-DD_yolo_track_mock.md`

### 验收
- [ ] `AI_BACKEND=mock`、`AI_BACKEND=qwen3vl` 回归仍通过（curl `/health`、`/api/infer/vision`）。
- [ ] `AI_BACKEND=yolo_rknn`（mock）：`/api/track/frame` 返回 `objects[].bbox` + `tracking_hint`，`control_allowed=false`。
- [ ] `/api/track/stream` 浏览器/`curl` 可见连续标注帧（mock bbox）。
- [ ] 未引入真权重、未入库任何模型文件。

### 禁止
- 不改 `mock`/`qwen3vl` 行为；`tracking_hint` 不得带 `pan_delta_deg/tilt_delta_deg`；不伪造 VLM bbox。

---

## 12-B：Dashboard 叠框 + 锁定标记 + 嵌流（可立即执行）

### 范围
把现有 `renderObjects` 从文本升级为**在图上画矩形**；嵌入 OPi5 低帧流；加锁定标记与 `alarm_phase` 指示。不改后端 DB。

### 操作步骤
1. `server/backend/templates/dashboard.html`：在视觉区加一个 `<canvas>`/SVG overlay 层覆盖 `#visionImage`；加 `<img id="trackStream">`（默认 hidden）；加 `alarm_phase` 标签位。
2. `server/backend/static/dashboard.js`：
   - 新增 `renderBoxes(objects, naturalW, naturalH)`：按 `显示尺寸/naturalWidth` 缩放 bbox 画框，标注 `label score`。**读图片真实尺寸，勿写死 640×480**。
   - 新增锁定标记（`tracking_hint.target_center`），**与现有云台姿态 `#crosshair` 视觉区分**（不同颜色/形状）。
   - 事件门控：空闲显示静帧快照；`state∈{VERIFY,ALARM}` 时显示 `#trackStream`（`src` 指向 OPi5 `/api/track/stream`，PC 已有到 10.0.1.120 静态路由）。
   - 显示 `alarm_phase`（`AIMING/SUPPRESSING`）与喷水倒计时（读 event `raw_json`）。
   - `tracking_hint`/`action_hint` 标注"**非控制源**"。
3. `style.css`：overlay/锁定标记/流容器样式。

### 产出物
- `server/backend/templates/dashboard.html`、`static/dashboard.js`、`static/style.css`（改）
- `tests/integration/YYYY-MM-DD_dashboard_bbox_stream.md`（截图路径）

### 验收
- [ ] mock 事件下，静帧上 bbox 框位置正确（缩放无偏）。
- [ ] `VERIFY/ALARM` 时低帧流可见且带框；空闲回退静帧。
- [ ] 锁定标记与云台姿态标记可区分。
- [ ] 旧事件（无 objects/无流）仍兼容显示，不报错。

### 禁止
- 不用 `localStorage/sessionStorage`；不改后端 schema；不把流当控制依据。

---

## 12-C：接真火源权重（VERIFY 门控）

### 范围
用首选权重把 mock 换成真检测；不动端点契约。

### 操作步骤（详见设计 §2.2 VERIFY）
1. 取 `luminous0219/fire-and-smoke-detection-yolov8` 的 yolov8n `.pt`（**不入库**）。
2. **走 phase10 同条链**：`airockchip/ultralytics_yolov8` 导 ONNX → `rknn-toolkit2` 转 `.rknn`（**勿用官方 ultralytics 直接 export 给 RKNN**）。
3. `yolo_rknn_backend.py` 真实模式：RKNN 加载 + 推理 + 后处理出 `fire/smoke` bbox；conf 初值 ≈0.5。
4. `edge/opi5-ai/models/README.md` 记录：权重来源、类别、转换命令、单帧 ms（**不放权重**）。

### 验收
- [ ] OPi5 单帧出合理 `fire/smoke` bbox（真实图片截图）。
- [ ] 单帧 NPU 推理 ms 记录；`/api/track/frame`、`/api/track/stream`、`/api/infer/vision` 均用真后端。
- [ ] mock/qwen3vl 回归仍通过；权重/数据集未入库。
- [ ] 失败回退：转换/精度反复不达标 → 回退 person 兜底并在报告注明火焰为扩展类别。

---

## 12-D：`imx_tracker` 低速锁定（只瞄准、不喷水）

### 范围
新增独立进程做"抓帧→OPi5→收坐标→限幅→请求 safetyd 调云台"，**不碰喷水**。`imx_safetyd` 仅新增一个 IPC 入口（设云台位姿），主 FSM 逻辑不动。

### 操作步骤
1. `imx_safetyd` 新增本地 IPC（Unix socket 或 FIFO）`set_gimbal_pose pan_us tilt_us`：safetyd 收到后**二次限幅**（1100–1900us、单步 ≤10°/≤5°、先 tilt 后 pan）再经 PCA9685 CH0/CH1 输出；安全动作进行时可拒绝/延后云台请求。**safetyd 仍是 PCA9685 唯一属主。**
2. 新增 `edge/imx6ull-controller/src/imx_tracker.c`：
   - 原生 MJPEG 连续抓帧（事件门控：VERIFY/ALARM 才拉到 ~5fps）。
   - `POST OPi5 /api/track/frame` → 取 `tracking_hint.offset_px`。
   - **本地**算 pan/tilt 增量并限幅（增量由 i.MX 算，OPi5 不给 delta）→ 经 IPC 请求 safetyd 调云台。
   - 写 `/run/imx_visual_state.json`：`{fire_confirmed, score, ts}`（供 12-E 双确认读）。
   - tracker 崩溃/超时不得影响 safetyd 安全回路。

### 产出物
- `edge/imx6ull-controller/src/imx_tracker.c`（新）
- `imx_safetyd.c`（**仅**加 IPC 入口 + 二次限幅；不动 FSM）
- `tests/imx6ull/YYYY-MM-DD_tracker_lock.md`

### 验收
- [ ] tracker 驱动云台朝 mock/真 bbox 低速锁定；单步限幅生效；脉宽不破。
- [ ] **杀掉 tracker**：safetyd 仍正常本地 ALARM；云台停在当前位，不失控。
- [ ] 全程不喷水；`control_allowed=false`。

### 禁止
- tracker 不得直接写 PCA9685；不得阻塞 safetyd；不得软编码 YUYV。

---

## 12-E：`imx_safetyd` 双确认喷水闸（设计评审通过后才执行）

> **门禁**：`tests/integration/2026-06-08_yolo_track_spray_design.md` §14 评审决议通过后方可执行；这是唯一允许改 `imx_safetyd.c` 主闭环的切片。

### 范围
在 ALARM 内新增**喷水子行为**（不新增顶层状态，作 `alarm_phase` 透出），按设计 §3 真值表实现双确认 + 限时 + 冷却 + 降级。

### 操作步骤
1. `imx_safetyd` 每个 tick 读 `/run/imx_visual_state.json` 的 `fire_confirmed`（窗口 `VISUAL_CONFIRM_WINDOW_MS`，初值 1500）。
2. 喷水闸（CH6）真值表：
   - 本地(flame|mq2)=1 **且** 视觉确认 fire → 进 `SUPPRESSING`，喷水。
   - 本地=1 且 视觉不可用/超时 → 按 `VISION_OFFLINE_AUTOSPRAY`（默认 **1**：等 `T_confirm` 后默认位自动喷；=0：仅报警等人工）。
   - 本地=0 且 视觉 fire → **不喷**，仅 WARN + 记录。
3. 喷水安全：`SPRAY_MAX_MS`（初值 3000）到时强制停泵；`SPRAY_COOLDOWN_MS`（初值 2000）冷却；`all_off()` 退出关泵。
4. env 开关：`VISION_OFFLINE_AUTOSPRAY`、`SPRAY_MAX_MS`、`SPRAY_COOLDOWN_MS`、`VISUAL_CONFIRM_WINDOW_MS`、`VISUAL_FIRE_SCORE_THR`。
5. 事件 `raw_json` 透出 `alarm_phase`，不改 DB schema。

### 验收
- [ ] 本地+视觉双确认 → 精确喷水（隔离水箱）。
- [ ] 视觉单独 fire（本地=0）→ **不喷**。
- [ ] 视觉掉线（本地=1）→ 按开关：默认位自动喷 或 仅报警。
- [ ] 限时停泵 + 冷却生效；退出 all_off。
- [ ] **独立性**：停 OPi5/tracker/拔网，本地强触发仍 ALARM + 蜂鸣器/RGB，并按降级策略动作。
- [ ] `control_allowed=false` 全程。

### 禁止
- 不让 AI/OPi5 单独触发喷水；不接 220V；水电物理隔离；不长时间空泵。

---

## 12-F：调优与实测回填

- 实测追踪环帧率、各段延迟（抓帧/推理/过网/转舵机）、喷水时长，回填 docs/07 §7.7、docs/11、设计 §7。
- 标定 `VISUAL_FIRE_SCORE_THR`、`T_confirm`、`SPRAY_MAX_MS`。

---

## 全局禁止事项

- 不改 DB schema（新字段一律走 `raw_json`）；不开 doc 15+。
- 不提交模型权重/抓拍图/数据集/tokenizer/真实 IP（走 inventory.yaml）。
- 不恢复 fan；不引用 CANONICAL 之外的引脚/通道。
- 12-E 之前不得改 `imx_safetyd.c` 主 FSM；12-E 须评审通过。
- `tracking_hint` 始终为感知提示，非命令；`control_allowed` 永 `false`。

## 完成后回填

- 契约：`docs/07`（`/api/track/frame`、`/api/track/stream`、`tracking_hint`、`objects[].bbox` 真填充、版本 1.1）。
- 状态机：`docs/06`（ALARM 内 `AIMING/SUPPRESSING` 子行为、双确认闸、限时/冷却/降级）。
- 架构/AI/视觉：`docs/04`（imx_tracker+IPC+流端点）、`docs/09`（yolo_rknn 后端 + §2.2 权重纠正）、`docs/08`（连续 MJPEG + 低速锁定 + 叠框/流）。
- 测试/答辩：`docs/11`（新用例）、`docs/13/14`（AI/云台/创新点 + 新问答）、`docs/12`（标注纯软件无新硬件）。
- 唯一事实源：`CANONICAL_DECISIONS.md`（imx_tracker 进程、PCA9685 属主、双确认闸、喷水限时/冷却、`VISION_OFFLINE_AUTOSPRAY`、重申 control_allowed=false）。
- 索引：`AGENTS.md` 状态表、`DEVPLAN.md`、`CLAUDE_CODE_TODO_INDEX.md`、`EXECUTION_GUIDE` 执行总顺序追加 Task12。
- 每切片留 `tests/...` 记录，未实测不得写"已通过"。
