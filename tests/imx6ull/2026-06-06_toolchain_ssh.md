# 2026-06-06 Task02 WSL SDK / SSH / hello 测试记录

## 结论

- 分支：`migration/imx6ull-opi5-edge-ai`
- SDK 可用性：已验证，可交叉编译 ARM 32-bit hello。
- SDK relocation：已执行嵌套 SDK 目录中的 `relocate-sdk.sh`。
- hello 部署/板端运行：未通过。本轮 WSL 与 Windows 侧均无法连通 i.MX6ULL SSH，未完成 `scp` 和板端运行验收。
- 真实 IP 仅出现在本地 `config/inventory.yaml` 和本测试记录中；`config/inventory.yaml` 不提交。

## 本地配置

- SDK 外层路径：`/home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot`
- SDK 实际 host 路径：`/home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot`
- `environment-setup-*`：未找到。
- `relocate-sdk.sh`：`/home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot/relocate-sdk.sh`
- CC：`/home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc`
- 本地 inventory：使用 `imx6ull.cc` fallback；`imx6ull.sdk_env` 保持占位，因为 SDK 没有环境脚本。

## SDK 修复记录

SDK 解压后出现权限和符号链接丢失：

- `bin/` 下工具初始没有执行位。
- `arm-buildroot-linux-gnueabihf-gcc` 等 wrapper 本应是到 `toolchain-wrapper` 的 symlink，但被解压为 17 字节普通文本文件。
- `lib/*.so*` 中部分 symlink 同样被解压成普通文本文件。
- `libexec/gcc/.../cc1` 与 `arm-buildroot-linux-gnueabihf/bin/as` 初始没有执行位。

已在 SDK 目录内修复：

```bash
chmod +x .../relocate-sdk.sh .../bin/*
.../relocate-sdk.sh
# 恢复内容指向同目录已有文件的小普通文件为 symlink
chmod +x .../libexec/gcc/arm-buildroot-linux-gnueabihf/7.5.0/*
chmod +x .../arm-buildroot-linux-gnueabihf/bin/*
```

relocation 输出摘要：

```text
Relocating the buildroot SDK from /home/book/100ask_imx6ull-sdk/Buildroot_2020.02.x/output/host to /home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot ...
```

## CC 版本

命令：

```bash
/home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc --version | head -3
```

输出摘要：

```text
arm-buildroot-linux-gnueabihf-gcc.br_real (Buildroot 2020.02-gee85cab) 7.5.0
Copyright (C) 2017 Free Software Foundation, Inc.
```

## hello 编译

命令：

```bash
scripts/build_imx6ull.sh && file build/imx6ull/hello_imx6ull
```

输出：

```text
[build] CC = /home/qbz415/arm-buildroot-linux-gnueabihf_sdk-buildroot/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin/arm-buildroot-linux-gnueabihf-gcc
arm-buildroot-linux-gnueabihf-gcc.br_real (Buildroot 2020.02-gee85cab) 7.5.0
[build] 产物: build/imx6ull/hello_imx6ull
build/imx6ull/hello_imx6ull: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 4.9.0, not stripped
```

## inventory 忽略验证

命令：

```bash
git check-ignore -v config/inventory.yaml
```

输出：

```text
.gitignore:7:inventory.yaml	config/inventory.yaml
```

## scp / ssh 部署

命令：

```bash
scripts/deploy_imx6ull.sh build/imx6ull/hello_imx6ull --run
```

输出：

```text
ssh: connect to host 192.168.137.110 port 22: No route to host
```

WSL 侧诊断：

```bash
ip route
ping -c 2 -W 1 192.168.137.110
ssh -o BatchMode=yes -o ConnectTimeout=5 root@192.168.137.110 true
```

输出摘要：

```text
192.168.137.0/24 dev eth1 proto kernel scope link metric 291
192.168.137.110 dev eth1 proto kernel scope link metric 40
2 packets transmitted, 0 received, 100% packet loss
ssh: connect to host 192.168.137.110 port 22: No route to host
```

Windows OpenSSH 探测：

```bash
/mnt/c/Windows/System32/OpenSSH/ssh.exe -o BatchMode=yes -o ConnectTimeout=5 root@192.168.137.110 echo ok
```

输出：

```text
ssh: connect to host 192.168.137.110 port 22: Connection timed out
```

PC 侧网络复测：

```powershell
Get-NetIPConfiguration
Get-NetRoute -DestinationPrefix 10.0.1.0/24
Test-Connection -Count 2 192.168.137.110
Test-Connection -Count 2 10.0.1.120
Test-NetConnection -ComputerName 192.168.137.110 -Port 22 -InformationLevel Detailed
Test-NetConnection -ComputerName 10.0.1.120 -Port 22 -InformationLevel Detailed
Get-NetNeighbor -IPAddress 192.168.137.110
Get-NetAdapter -InterfaceIndex 12
```

输出摘要：

```text
PC Ethernet IPv4Address: 192.168.137.1
Route: 10.0.1.0/24 via 192.168.137.110 ifIndex 12
Ethernet adapter: Realtek Gaming GbE Family Controller, Status Up, LinkSpeed 100 Mbps
192.168.137.110 ping: failed
192.168.137.110 TCP 22: failed
10.0.1.120 ping: failed
10.0.1.120 TCP 22: failed
Neighbor 192.168.137.110 on ifIndex 12: State Incomplete, LinkLayerAddress 00-00-00-00-00-00
```

PC 侧结论：以太网口已 Up，但未收到 i.MX6ULL `192.168.137.110` 的 ARP 响应；OPi5 路径依赖 i.MX6ULL 转发，因此 i.MX6ULL 不通时 OPi5 也无法从 PC 侧连通。

## i.MX6ULL 网络状态摘要

以下为本轮开始前用户提供的人工已验证 B 方案网络状态，本轮未能通过 SSH 复测：

- i.MX6ULL eth0：`192.168.137.110/24`
- i.MX6ULL eth1：`10.0.1.1/24`
- default gateway：`192.168.137.1 dev eth0`
- `ip_forward=1`
- `/etc/init.d/S99edge_network` 已由人工创建并验证可恢复上述网络。

## 当前阻塞

Task02 的 SDK 与交叉编译门槛已通过；`scp` 与板端运行门槛未通过。需要恢复 PC/WSL 到 i.MX6ULL `192.168.137.110:22` 的连通性后重跑：

```bash
scripts/deploy_imx6ull.sh build/imx6ull/hello_imx6ull --run
```
