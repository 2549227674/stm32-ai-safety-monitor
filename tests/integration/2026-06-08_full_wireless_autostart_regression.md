# 2026-06-08 全无线自启动与代理清理回归测试

## 目标

验证 i.MX / OPi5 在无网线情况下，WiFi 自启动可自动连接手机热点，并保持 i.MX → OPi5 AI → PC Flask Dashboard 链路可用。

## 代理清理

| 项目 | 结果 |
|---|---|
| i.MX 默认代理是否清理 | 无代理配置（干净） |
| OPi5 默认代理是否清理 | 待手动确认（WSL 无法直接 SSH 到 OPi5 WiFi） |
| i.MX 新 shell 是否仍有代理 | 否 |
| imx_safetyd 环境是否含代理 | 否 |

## i.MX 自启动

| 项目 | 结果 |
|---|---|
| S45wifi-client 是否部署 | 是（/etc/init.d/S45wifi-client） |
| 使用驱动模式 | nl80211（wext 不兼容） |
| wpa_supplicant_imx.conf 是否部署 | 是（/etc/wpa_supplicant_imx.conf, chmod 600） |
| 模块重载 | 已加入脚本（rmmod 8723bu + modprobe 8723bu） |
| DHCP 是否成功 | 是（10.96.98.109/24） |

### 关键修复

原始 S45wifi-client 脚本的 stop/start 会导致 RTL8723BU 驱动状态异常（wlan0 显示 Connected 但 NO-CARRIER）。修复方案：在 start 中加入 `rmmod 8723bu` + `modprobe 8723bu` 模块重载。

## OPi5 自启动

| 项目 | 结果 |
|---|---|
| nmcli autoconnect | 待手动配置（WSL 无法直接 SSH 到 OPi5 WiFi） |
| 8188eu 自动加载 | 待手动确认 |

OPi5 需要手动执行：

```bash
nmcli con modify "Redmi K70" connection.autoconnect yes
nmcli con modify "Redmi K70" connection.autoconnect-retries -1
echo "8188eu" | sudo tee /etc/modules-load.d/8188eu.conf
```

## 全无线链路（有线已连接，WiFi 同时在线）

| 项目 | 结果 |
|---|---|
| i.MX → OPi5 AI health | 成功 |
| i.MX → OPi5 AI infer | 成功（risk_hint=2, control_allowed=false, latency_ms=15） |
| i.MX → PC Flask health | 成功 |
| i.MX proxy 环境 | 无代理 |

## 重启回归

重启回归需要用户手动操作（重启 i.MX / OPi5 后检查 WiFi 自动连接）。当前测试验证了：

- S45wifi-client 脚本可手动 start/stop
- wpa_supplicant 使用 nl80211 可连接
- DHCP 可获取 IP
- 全链路可通

## 回退方案

| 优先级 | 方案 |
|---|---|
| 1（当前） | 全无线热点 |
| 2 | Windows portproxy |
| 3 | 全有线 |

## 结论

- i.MX WiFi 自启动脚本已部署并验证（含模块重载修复）
- OPi5 WiFi autoconnect 待手动配置
- 全无线链路已验证可用
- 重启回归待用户手动验证
- 答辩现场保留网线回退

## 风险

- RTL8723BU 驱动需要模块重载才能可靠重启 WiFi
- 手机热点 DHCP 可能分配不同 IP
- 手机热点延迟波动
- 答辩现场必须保留网线
