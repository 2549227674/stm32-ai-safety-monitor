# 2026-06-08 Windows portproxy 转发 OPi5 WiFi AI 服务测试

## 测试目标

验证 Windows 主机同时连接 i.MX 有线网络和 OPi5 WiFi 网络时，能否通过 portproxy 让 i.MX 访问 OPi5 WiFi AI 服务，以尝试减少 OPi5 有线网线。

## 拓扑

```
i.MX6ULL (192.168.137.110)
    ↓ eth0
Windows Ethernet (192.168.137.1:18080)
    ↓ portproxy
Windows WLAN (10.96.98.103)
    ↓
OPi5 WiFi (10.96.98.38:8080)
```

## 前置验证

| 检查项 | 结果 |
|---|---|
| Windows Ethernet IP | 192.168.137.1 |
| Windows WLAN IP | 10.96.98.103 |
| OPi5 WiFi IP | 10.96.98.38 |
| i.MX IP | 192.168.137.110 |
| Windows → OPi5 WiFi health | 成功（mock AI JSON） |
| i.MX → Windows ping | 成功（~2ms） |

## portproxy 配置

```powershell
netsh interface portproxy add v4tov4 listenaddress=192.168.137.1 listenport=18080 connectaddress=10.96.98.38 connectport=8080
New-NetFirewallRule -DisplayName "OPi5-AI-Port-Forward-18080" -Direction Inbound -Action Allow -Protocol TCP -LocalPort 18080
```

| 项目 | 值 |
|---|---|
| listenaddress | 192.168.137.1 |
| listenport | 18080 |
| connectaddress | 10.96.98.38 |
| connectport | 8080 |
| firewall rule | OPi5-AI-Port-Forward-18080 |

## 验证结果

| 测试 | 结果 |
|---|---|
| Windows → 192.168.137.1:18080/health | 成功 |
| i.MX → 192.168.137.1:18080/health | 成功 |
| i.MX → 192.168.137.1:18080/api/infer/vision | 成功（mock AI 返回 risk_hint=2, latency_ms=14） |

## AI infer 链路详情

i.MX 通过 portproxy POST 到 OPi5 WiFi AI 服务：

- 请求：POST multipart（test JPEG + metadata）
- 响应：`{"ok":true,"risk_hint":2,"control_allowed":false,"latency_ms":14,...}`
- 模型：mock-detector（mock 模式）

## 回退方案

旧有线链路仍保留：

- OPi5 有线 IP：10.0.1.120
- i.MX → OPi5 有线 AI：`http://10.0.1.120:8080/api/infer/vision`
- WSL → OPi5 有线 health：已验证可用

## 回滚命令

```powershell
netsh interface portproxy delete v4tov4 listenaddress=192.168.137.1 listenport=18080
Remove-NetFirewallRule -DisplayName "OPi5-AI-Port-Forward-18080"
```

## 结论

- **成功**：Windows portproxy 可作为减少 OPi5 网线的可选方案
- i.MX → PC:18080 → OPi5 WiFi:8080 链路已验证
- 全有线方案仍保留为最终演示回退

## 风险

- Windows 必须保持同时连接 i.MX 有线和 OPi5 WiFi
- OPi5 WiFi IP 变化后需要重建 portproxy（`netsh interface portproxy delete` + `add`）
- 手机热点延迟可能波动（本轮 latency_ms=14，可接受）
- 全有线方案仍保留为最终演示回退
