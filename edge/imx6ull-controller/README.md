# edge/imx6ull-controller

i.MX6ULL Pro 控制端代码目录。目标：实现 `imx_safetyd`，负责 GPIO/I2C/PWM/MOS、V4L2 抓拍、本地状态机、OPi5 AI 请求、Flask 上报。

## 子目录

| 目录 | 用途 |
|---|---|
| `src/` | C/C++ 源码 |
| `include/` | 头文件 |
| `config/` | 示例配置，不放真实 IP |
| `systemd/` | systemd service 模板 |

请从 Task02 的 hello 开始，不要一开始写完整业务。
