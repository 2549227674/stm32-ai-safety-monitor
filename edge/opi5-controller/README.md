# OPi5 Controller — 临时本地安全主控

## 背景

i.MX6ULL Pro 末期出现供电/启动异常，答辩前无法更换。将 `imx_safetyd` 的 Linux
安全主控逻辑迁移到 Orange Pi 5（RK3588S），OPi5 临时同时承担：

1. **本地安全闭环进程** (`opi5_safetyd`) — GPIO/I2C 传感器采集、状态机、执行器控制
2. **AI 推理服务** (`opi5_ai_service`) — Qwen3-VL / mock，`control_allowed=false`

两个进程独立运行，互不依赖。AI 离线时本地安全闭环仍可依据传感器进入安全态。

## 目录结构

```text
edge/opi5-controller/
├── README.md
├── src/
│   ├── opi5_safetyd.c        # 主控程序（从 imx_safetyd.c 迁移）
│   ├── pca9685_set_pwm.c     # PCA9685 PWM 工具
│   └── bsp_oled_ssd1306.c    # OLED 驱动
├── include/
│   ├── bsp_oled_ssd1306.h
│   └── font5x7.h
├── config/
│   └── opi5-safetyd.example.env
└── systemd/
    └── opi5-safetyd.service
```

## 构建

在 OPi5 上原生编译（不需要交叉编译）：

```bash
# 依赖
sudo apt install -y build-essential i2c-tools libgpiod-dev v4l-utils curl

# 编译
scripts/build_opi5_controller.sh all
```

## 运行

```bash
# 单次自检
./build/opi5-controller/opi5_safetyd --mode once --force-verify 1

# 循环运行
./build/opi5-controller/opi5_safetyd --mode loop

# systemd
sudo cp edge/opi5-controller/systemd/opi5-safetyd.service /etc/systemd/system/
sudo cp edge/opi5-controller/config/opi5-safetyd.example.env /etc/edge-ai-safety-monitor/opi5-safetyd.env
# 编辑 opi5-safetyd.env 填入真实值
sudo systemctl enable --now opi5-safetyd
```

## 引脚映射

参见 `opi5_controller_pinmap_and_i2c.md`（仓库根目录或 temp_docs/）。
所有 GPIO/I2C 号必须以板端 `gpio readall` / `i2cdetect` 实测为准。

## 安全红线

- AI 服务只返回 `risk_hint`，`control_allowed=false`
- 本地安全闭环进程是唯一执行器决策点
- 不接 220V，执行器独立 5V + 星形共地
- 断网/断 AI 时本地仍可进入安全态
