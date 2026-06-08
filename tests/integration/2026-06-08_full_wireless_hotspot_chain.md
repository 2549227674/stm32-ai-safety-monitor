# 2026-06-08 全无线热点链路验证

## 目标

验证 PC、i.MX6ULL、OPi5 均连接手机热点时，i.MX 是否能直接访问 OPi5 AI 与 PC Flask Dashboard，不依赖任何网线。

## 拓扑

```
手机热点 Redmi K70 (10.96.98.0/24)
├── PC / Windows (10.96.98.103) — Flask Dashboard
├── OPi5 (10.96.98.38) — AI Service
└── i.MX6ULL (10.96.98.109) — 本地主控 imx_safetyd
```

| 设备 | WiFi IP | 有线状态 |
|---|---|---|
| PC / Windows | 10.96.98.103 | eth 192.168.137.1（保留但非主链路） |
| OPi5 | 10.96.98.38 | eth0 DOWN（网线已拔） |
| i.MX6ULL | 10.96.98.109 | eth0/eth1 DOWN（网线已拔） |

## 连通性（全无线，所有网线已拔）

| 项目 | 结果 |
|---|---|
| i.MX → OPi5 WiFi ping | 成功（19~368ms） |
| i.MX → OPi5 AI health | 成功 |
| i.MX → OPi5 AI infer | 成功（risk_hint=2, control_allowed=false, latency_ms=14） |
| i.MX → PC WiFi ping | 成功（22~70ms） |
| i.MX → PC Flask health | 成功 |
| i.MX → PC Flask status | 成功（返回最新事件） |
| PC → i.MX WiFi ping | 成功（92ms） |
| PC → OPi5 WiFi ping | 成功（345ms） |

## AI infer 链路详情

i.MX 直接通过 WiFi POST 到 OPi5 WiFi AI 服务（无 portproxy）：

- 请求：POST multipart（test JPEG + metadata）
- 响应：`{"ok":true,"risk_hint":2,"control_allowed":false,"latency_ms=14,...}`
- 模型：mock-detector（mock 模式）

## 关键发现

- i.MX 板载 RTL8723BU WiFi 必须使用 `-D nl80211` 驱动模式
- wext 模式会连接后立即断开（已知 vendor 驱动兼容问题）
- 全无线时 eth0/eth1 接口仍保留 IP 但状态 DOWN，不影响 wlan0
- wlan0 自动成为默认路由出口

## WiFi 驱动信息

| 设备 | 驱动 | 接口 | 备注 |
|---|---|---|---|
| i.MX 板载 RTL8723BU | 8723bu.ko (v4.3.6.11) | wlan0 | 必须 nl80211 |
| OPi5 USB RTL8188ETV | 8188eu.ko (lwfinger) | wlx8420965bb9d8 | nmcli 连接 |

## 自启动方案

### i.MX WiFi 自启动

模板文件已创建：

- `config/templates/wpa_supplicant_imx.conf.example` — WPA 配置模板
- `config/templates/S45wifi-client.example` — init.d 自启动脚本模板

部署步骤（板端手动执行）：

```bash
# 1. 复制配置文件并填入真实 SSID/密码
cp config/templates/wpa_supplicant_imx.conf.example /etc/wpa_supplicant_imx.conf
vi /etc/wpa_supplicant_imx.conf  # 填入真实值

# 2. 复制自启动脚本
cp config/templates/S45wifi-client.example /etc/init.d/S45wifi-client
chmod +x /etc/init.d/S45wifi-client

# 3. 测试
/etc/init.d/S45wifi-client start
/etc/init.d/S45wifi-client status

# 4. 开机自启（/etc/init.d/rcS 会自动执行 S??* 脚本）
```

### OPi5 WiFi 自启动

OPi5 使用 NetworkManager，WiFi 连接已自动保存。确认自启动：

```bash
# 在 OPi5 上执行
nmcli con show                          # 查看连接名
nmcli con modify "<连接名>" connection.autoconnect yes
nmcli con modify "<连接名>" connection.autoconnect-retries -1

# 确认 8188eu 驱动自动加载
echo "8188eu" | sudo tee /etc/modules-load.d/8188eu.conf
```

## 回退方案

| 优先级 | 方案 | 拓扑 |
|---|---|---|
| 1（当前） | 全无线 | 三台设备均连手机热点 |
| 2 | Windows portproxy | i.MX 有线 → PC → OPi5 WiFi |
| 3 | 全有线 | i.MX → OPi5 有线 10.0.1.120 |

回退步骤：插回网线即可恢复有线链路。

## 结论

- **成功**：全无线热点链路可作为答辩演示优化方案
- i.MX 直接通过 WiFi 访问 OPi5 AI 和 PC Flask，无需 portproxy
- 三台设备均在同一手机热点子网内
- 全有线和 Windows portproxy 方案仍保留为回退

## 风险

- 手机热点 IP 可能变化（重启热点后需确认）
- 手机热点延迟波动（本轮 19~368ms）
- 手机热点可能有客户端隔离（本轮未出现）
- 答辩现场必须保留网线作为回退
- Flask 需要监听 0.0.0.0 并放行 Windows 防火墙 5000 端口
