# 2026-06-08 OPi5 USB WiFi rtl8188eu 驱动诊断记录

## 测试目标

尝试让 OPi5 使用 0bda:0179 RTL8188ETV/RTL8188EU 类 USB WiFi，以减少一条 OPi5 有线网线。

## 边界

- 只处理 OPi5。
- 不处理 i.MX6ULL WiFi。
- 不修改 i.MX 有线网络。
- 有线网络保留为最终演示回退方案。

## 硬件

- USB WiFi VID:PID：0bda:0179
- 芯片识别：RTL8188ETV / RTL8188EU 类
- SerialNumber: 8420965BB9D8

## 原始问题

- `CONFIG_R8188EU=n`（内核未内置）
- `rtl8xxxu` 不支持 0bda:0179（无匹配 alias）
- 插入后无 wlan 接口，驱动未绑定

## OPi5 环境

- kernel：`6.1.99-rockchip-rk3588`（aarch64）
- OS：Ubuntu 22.04 Jammy（Orange Pi 1.2.2）
- headers：初始缺失；`/opt/linux-headers-current-rockchip-rk3588_1.2.2_arm64.deb` 已安装
- `/lib/modules/$(uname -r)/build`：安装后存在（指向 `/usr/src/linux-headers-6.1.99-rockchip-rk3588`）
- build-essential/gcc/make/git/dkms：已安装
- cfg80211：`CONFIG_CFG80211=y`（built-in）
- mac80211：`CONFIG_MAC80211=y`（built-in）

## 驱动处理

- 驱动来源：`https://github.com/lwfinger/rtl8188eu`（master 分支）
- commit：tarball 下载（WSL 本地 wget），因 OPi5 git clone TLS 失败
- 安装方式：`make` + `make install`（Makefile 要求 `.git` 目录，已创建 dummy `.git`）
- 编译结果：成功，生成 `8188eu.ko`（749568 bytes）
- 模块名称：`8188eu`
- firmware：`rtl8188eufw.bin` 已复制到 `/lib/firmware/rtlwifi/`
- `depmod -a`：成功
- `modprobe 8188eu`：成功
- dmesg 确认：
  ```
  8188eu: loading out-of-tree module taints kernel.
  Chip Version Info: CHIP_8188E_Normal_Chip_TSMC_D_CUT_1T1R_RomVer(0)
  usbcore: registered new interface driver r8188eu
  r8188eu 3-1:1.0 wlx8420965bb9d8: renamed from wlan0
  R8188EU: Firmware Version 11, SubVersion 1, Signature 0x88e1
  ```
- 是否出现 wlan 接口：**是**，`wlx8420965bb9d8`（MAC 命名）

## WiFi 测试

- `nmcli dev`：`wlx8420965bb9d8` 状态 `disconnected` → `connected`
- `nmcli radio wifi`：`enabled`
- `nmcli dev wifi list`：扫描到多个热点（Redmi K70、OPPO Reno15、TP-LINK 等）
- 连接测试热点：成功，`Device 'wlx8420965bb9d8' successfully activated`
- WiFi IP：`10.96.98.38/24`（热点子网）
- 默认路由：`default via 10.96.98.191 dev wlx8420965bb9d8 proto dhcp metric 600`
- IPv6 公网访问：成功（`ping www.baidu.com` 190ms）
- IPv4 公网访问：失败（默认路由走 eth0 metric 100，eth0 无公网；WiFi metric 600 未被选中）
- eth0 保持连接：`10.0.1.120/24`，有线未断

## 端边链路测试

- OPi5 本地 AI health：服务未运行（port 8080 未监听），非 WiFi 问题
- i.MX → OPi5 WiFi ping：**失败**（10.0.1.0/24 与 10.96.98.0/24 不同子网，无路由）
- 原因：手机热点创建独立子网，与有线 LAN 隔离

## 结论

- **驱动编译成功**：rtl8188eu out-of-tree 驱动在 OPi5 6.1.99 内核上编译并加载成功
- **WiFi 接口可用**：`wlx8420965bb9d8` 可扫描、可连接热点、可访问公网（IPv6）
- **有线保留**：eth0 仍为默认路由，有线网络未受影响
- **端边链路限制**：WiFi 连接手机热点时与有线 LAN 不同子网，i.MX 无法通过 WiFi IP 访问 OPi5
- **建议**：
  - WiFi 可作为 OPi5 联网的可选优化（apt 更新、模型下载等）
  - i.MX → OPi5 AI 链路仍走有线（同子网）
  - 最终演示保留有线方案
  - 如需 WiFi 替代有线，需将两者接入同一 AP（非手机热点）

## 修改文件

- 仓库测试记录：是（本文件）
- 业务代码：否
- 网络脚本：否
- inventory.example.yaml：否
- OPi5 ~/.bashrc：否（本轮不处理代理）
