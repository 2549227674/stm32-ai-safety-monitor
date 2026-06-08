# 2026-06-08 i.MX6ULL 板载 RTL8723BU WiFi 重试记录

## 目标

测试 i.MX6ULL 板载 RTL8723BU WiFi 是否可完成扫描、关联和 DHCP。

## 硬件说明

- 板载 RTL8723BU / 0bda:b720
- 通过板载 USB Hub 枚举（Bus 001 Device 003）
- 不是外置 USB WiFi
- 天线：已接 IPEX/U.FL 板载天线

## 当前主链路保护

| 项目 | 结果 |
|---|---|
| i.MX → Windows portproxy health | 成功（eth0 192.168.137.110） |
| 是否影响 eth0/portproxy | 否 |

## WiFi 枚举

| 项目 | 结果 |
|---|---|
| lsusb | 0bda:b720 Realtek，rtl8723bu 驱动 |
| wlan 接口 | wlan0 + wlan1 存在 |
| 8723bu.ko 状态 | 已加载，v4.3.6.11_12942.20141204_BTCOEX20140507-4E40 |
| wifi-pwr / 74595 电源状态 | dmesg 显示 `wifi-pwr: disabling`，无用户态控制命令 |

## 扫描

| 接口 | 结果 |
|---|---|
| wlan0 扫描 | 成功，扫描到多个热点（2.4GHz） |
| wlan1 扫描 | 成功 |
| 是否扫描到热点 | 是（SSID 已隐藏） |

信号最强热点：-45 dBm，2.462 GHz

## 连接测试

| 驱动模式 | 结果 |
|---|---|
| wext | 短暂连接后立即断开（`CTRL-EVENT-CONNECTED` → `CTRL-EVENT-DISCONNECTED`） |
| **nl80211** | **成功连接**，稳定保持 |
| Association request to the driver failed | wext 模式出现，nl80211 模式未出现 |

### nl80211 连接详情

```
Connected to 5a:86:6c:90:74:9c (on wlan0)
SSID: Redmi K70
freq: 2437
signal: -45 dBm
tx bitrate: 72.2 MBit/s
```

## DHCP

| 项目 | 结果 |
|---|---|
| default.script 是否存在 | 是（/usr/share/udhcpc/default.script） |
| DHCP 是否成功 | 是 |
| 获取 IP | 10.96.98.109/24 |
| 网关 | 10.96.98.191 |
| DNS | 10.96.98.191 |
| ping 网关 | 成功 |
| ping 8.8.8.8 | 成功（~206ms） |

## 路由状态

```
default via 10.96.98.191 dev wlan0
default via 192.168.137.1 dev eth0
10.96.98.0/24 dev wlan0 proto kernel scope link src 10.96.98.109
192.168.137.0/24 dev eth0 proto kernel scope link src 192.168.137.110
```

wlan0 默认路由优先级高于 eth0（metric 未显式设置，按接口顺序）。eth0 主链路未受影响。

## 是否需要重新编译 8723bu.ko

| 项目 | 结果 |
|---|---|
| 是否需要 | **否** |
| 原因 | 现有 8723bu.ko + nl80211 驱动模式可成功连接、DHCP、访问公网 |

## 结论

- **成功**：i.MX 板载 RTL8723BU WiFi 可用（nl80211 驱动模式）
- 连接稳定，信号良好（-45dBm），速率 72.2MBit/s
- DHCP 正常获取 IP，可访问公网
- eth0 主链路（Windows portproxy → OPi5 AI）未受影响
- 板载 WiFi 可作为备用联网能力，不替代 OPi5 WiFi + Windows portproxy 主链路

## 使用建议

如需使用板载 WiFi 连接热点：

```bash
# 在 i.MX 上执行
wpa_supplicant -i wlan0 -c /tmp/wpa.conf -D nl80211 -B
udhcpc -i wlan0 -q -n -t 5
```

关键：必须使用 `-D nl80211`，不能用 `-D wext`（wext 会连接后立即断开）。
