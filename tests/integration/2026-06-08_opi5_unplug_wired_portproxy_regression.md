# 2026-06-08 OPi5 拔掉有线网线后的 WiFi + Windows portproxy 回归测试

## 测试目标

验证 OPi5 拔掉有线网线后，i.MX 是否仍能通过 Windows portproxy 访问 OPi5 WiFi AI 服务。

## 拓扑

```
i.MX6ULL (192.168.137.110)
    ↓ eth0 有线
Windows Ethernet (192.168.137.1:18080)
    ↓ portproxy
Windows WLAN (10.96.98.103)
    ↓ 手机热点 WiFi
OPi5 WiFi (10.96.98.38:8080)
    ✗ OPi5 有线已拔掉，不参与本轮主链路
```

## 前置状态

| 项目 | 值 |
|---|---|
| OPi5 rtl8188eu 驱动 | 8188eu，已加载 |
| OPi5 WiFi 接口 | wlx8420965bb9d8 |
| OPi5 WiFi IP | 10.96.98.38（未变化） |
| OPi5 有线 | 已拔掉 |
| Windows Ethernet IP | 192.168.137.1 |
| Windows WLAN IP | 10.96.98.103 |
| i.MX IP | 192.168.137.110 |
| portproxy | 192.168.137.1:18080 → 10.96.98.38:8080 |
| OPi5 AI 服务 | mock 模式，监听 0.0.0.0:8080 |

## 验证结果

| 项目 | 结果 |
|---|---|
| OPi5 有线网线是否已拔 | 是 |
| OPi5 旧有线 IP (10.0.1.120) 可达性 | 不可达（预期） |
| OPi5 WiFi 是否保持连接 | 是（Windows ping 成功，~160ms） |
| OPi5 WiFi IP 是否变化 | 否，仍为 10.96.98.38 |
| Windows → OPi5 WiFi:8080 health | 成功 |
| Windows → 192.168.137.1:18080 health | 成功 |
| i.MX → 192.168.137.1:18080 health | 成功 |
| i.MX → AI infer (via portproxy) | 成功（risk_hint=2, control_allowed=false, latency_ms=5） |

## AI infer 链路详情

i.MX 通过 portproxy POST 到 OPi5 WiFi AI 服务（OPi5 有线已拔）：

- 请求：POST multipart（test JPEG + metadata）
- 响应：`{"ok":true,"risk_hint":2,"control_allowed":false,"latency_ms":5,...}`
- 模型：mock-detector（mock 模式）
- 延迟：5ms（比有线时 14ms 更低，波动范围内）

## 旧有线回退状态

- OPi5 有线 IP 10.0.1.120：不可达（网线已拔）
- 回退步骤：插回 OPi5 网线，等待 eth0 获取 IP，即可恢复 `http://10.0.1.120:8080/api/infer/vision`

## 结论

- **成功**：OPi5 有线网线可拔，Windows portproxy 链路可作为减少网线方案
- i.MX → PC:18080 → OPi5 WiFi:8080 在拔线后仍正常工作
- 全有线方案仍保留为最终演示回退（插回网线即可恢复）

## 风险

- Windows 必须同时连接 i.MX 有线网络和 OPi5 WiFi
- OPi5 WiFi IP 变化后必须重建 portproxy
- 手机热点延迟可能波动（本轮 latency_ms=5，可接受）
- 全有线方案仍保留为最终演示回退
