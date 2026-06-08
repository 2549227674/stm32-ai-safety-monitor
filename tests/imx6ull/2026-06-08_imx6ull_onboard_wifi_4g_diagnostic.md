# 2026-06-08 i.MX6ULL 板载 WiFi/4G 通信硬件轻量诊断

## 测试目标

对 i.MX6ULL Pro / 100ASK 底板上的板载 WiFi/BT（RTL8723BU）与 4G（EC20/ME909S）相关硬件做轻量诊断，确认系统是否枚举到相关设备以及当前镜像驱动状态。

## i.MX6ULL 环境

- 内核：`4.9.88` (armv7l, SMP PREEMPT)
- OS：100ask 系统
- 网络接口：eth0 (192.168.137.110) UP, eth1 (10.0.1.1) DOWN (OPi5 网线已拔), wlan0/wlan1 DOWN

## USB 设备枚举

| Bus:Dev | VID:PID | 设备 | 驱动 |
|---|---|---|---|
| 001:001 | 1d6b:0002 | Linux Foundation USB 2.0 root hub | ci_hdrc |
| 001:002 | 0424:2514 | SMSC USB 2.0 Hub | hub |
| 001:003 | **0bda:b720** | **Realtek Semiconductor Corp.** | **rtl8723bu + btusb** |

`lsusb -t` 确认：
- If 0: Wireless, Driver=btusb（蓝牙）
- If 1: Wireless, Driver=btusb（蓝牙）
- If 2: Vendor Specific, Driver=rtl8723bu（WiFi）

## WiFi 诊断结果

| 项目 | 结果 |
|---|---|
| 是否枚举到 RTL8723BU | **是**（0bda:b720，USB Hub Port 1） |
| 驱动是否加载 | **是**（8723bu.ko, v4.3.6.11_12942.20141204_BTCOEX20140507-4E40） |
| wlan 接口是否出现 | **是**（wlan0 + wlan1，`iw dev` 可见） |
| cfg80211/mac80211 | **是**（CONFIG_CFG80211=y, CONFIG_MAC80211=y，built-in） |
| 接口状态 | DOWN（未连接） |
| 是否能判定 WiFi 可用 | **不确定**（驱动已加载，接口已创建，但存在已知 wpa_supplicant 兼容性问题；`wifi-pwr: disabling` 提示电源控制可能被禁用） |

### WiFi 已知问题

- dmesg: `wifi-pwr: disabling` — WiFi 电源控制可能被关闭
- dmesg: `Bluetooth: hci0: Failed to load rtl_bt/rtl8723b_config.bin` — BT 配置文件缺失
- vendor rtl8723bu 驱动（非主线 rtl8xxxu）存在已知的 `Association request to the driver failed` 兼容性问题
- 本轮不做驱动修复

## 4G 诊断结果

| 项目 | 结果 |
|---|---|
| 是否枚举到 4G 模块 | **否** |
| lsusb 中是否有 EC20/ME909S/Quectel/Huawei | **否** |
| 是否出现 ttyUSB / cdc-wdm / wwan / usb0 | **否** |
| 4G 相关内核模块 | option/usbserial 已注册（通用），但无设备绑定 |
| 是否能判定 4G 可用 | **否**（未插装 4G 模块或未上电） |

## 电源控制脚位

| 脚位 | 检查结果 |
|---|---|
| 74595_WIFI_PWREN | 系统中未找到相关控制脚本 |
| 4G_POWER_EN | 系统中未找到相关控制脚本 |

## 结论

- **WiFi**：RTL8723BU 枚举成功，驱动已加载，wlan0/wlan1 接口存在。但存在驱动兼容性和电源控制问题，未作为项目主链路。
- **4G**：未枚举到任何 4G 模块。底板具备 4G 扩展设计（Nano SIM、EC20/ME909S 接口），但当前未插装模块或未上电。
- **未修改任何系统配置**。
- **未影响现有网络**（eth0/eth1/portproxy 链路不受影响）。

## 对项目的影响

- 当前最终演示仍使用 OPi5 WiFi + Windows portproxy 或全有线回退。
- 板载 WiFi/4G 不纳入项目主链路。
- 报告中可写："底板具备 WiFi/4G 通信扩展设计，未作为最终主链路。"
