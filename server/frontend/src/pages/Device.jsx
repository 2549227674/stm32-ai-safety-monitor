import React from 'react'
import { Card, Pill, Tag, timeAgo, fmtUptime } from '../components/Widgets'
import { TimeChart } from '../components/Charts'

export default function PageDevice({ sim }) {
  const dev = sim.device || {};
  const h = dev.health || {};
  const off = sim.scenario === "offline";
  const deg = sim.scenario === "degraded";
  const isReal = sim.mode === "real";
  const T = sim.thresholds;

  const services = [
    { unit: "device-agent.service", desc: "遥测采集 · 心跳 · AI 编排", state: dev.services ? dev.services.device_agent : "unknown" },
    { unit: "opi5-ai-qwen3vl.service", desc: "Qwen3-VL 2B 推理服务 (:8080)", state: dev.services ? dev.services["opi5-ai-qwen3vl"] : "unknown" },
    { unit: "opi5-safetyd.service", desc: "本地安全闭环（传感器→执行器）", state: dev.services ? dev.services["opi5-safetyd"] : "unknown" },
  ];
  const stTag = (s) => {
    if (s === "running") return <Tag level="ok">running</Tag>;
    if (s === "active") return <Tag level="ok">active</Tag>;
    if (s === "failed") return <Tag level="danger">failed</Tag>;
    if (s === "unknown") return <Tag level="warn">unknown / service not found or not running</Tag>;
    return <Tag level="warn">{s || "unknown"}</Tag>;
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16 }} data-screen-label="设备健康">
      <div className="grid" style={{ gridTemplateColumns: "minmax(0, 5fr) minmax(0, 7fr)" }}>
        <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
          <Card title="edge-opi5-001" sub="Orange Pi 5 · RK3588S"
            right={off ? <Pill level="danger" blink>离线</Pill> : <Pill level="ok" blink>在线</Pill>}>
            <dl className="kv" style={{ gridTemplateColumns: "auto 1fr" }}>
              <dt>角色</dt><dd>视觉 AI 推理 + 遥测网关 + 本地安全闭环</dd>
              <dt>边缘设备</dt><dd>Orange Pi 5 / RK3588S</dd>
              <dt>采集服务</dt><dd>OPi5 device-agent</dd>
              <dt>agent 版本</dt><dd>{dev.agent_version || "—"}</dd>
              <dt>contract</dt><dd>v{dev.contract_version || "—"}</dd>
              <dt>运行时长</dt><dd>{off ? "—" : fmtUptime(dev.uptime_s)}</dd>
              <dt>最后心跳</dt><dd style={{ color: off ? "var(--danger)" : undefined }}>{timeAgo(dev.last_seen_at)}{off ? "（丢失）" : ""}</dd>
              <dt>摄像头</dt><dd>{dev.camera_status === "online" ? "CAM-01 在线" : dev.camera_status === "mock" ? "模拟摄像头" : "离线"}</dd>
            </dl>
          </Card>
          <Card title="systemd 服务" sub="心跳随带 systemctl is-active 状态" flush>
            <table className="table">
              <tbody>
                {services.map((s) => (
                  <tr key={s.unit}>
                    <td><div className="mono" style={{ fontSize: 12, color: "var(--text)" }}>{s.unit}</div><div className="t3" style={{ fontSize: 11.5 }}>{s.desc}</div></td>
                    <td style={{ textAlign: "right", verticalAlign: "middle" }}>{stTag(s.state)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
            {isReal && services.some(s => s.state === "unknown") && (
              <p className="t3" style={{ margin: "8px 16px 12px", fontSize: 11, lineHeight: 1.6 }}>
                {'诊断：'}<code>{'systemctl is-active opi5-ai-qwen3vl.service opi5-safetyd.service'}</code>
              </p>
            )}
          </Card>
          {deg && (
            <div className="banner degraded">
              <span>◐</span>
              <span style={{ flex: 1, fontSize: 12.5 }}><b>opi5-ai-qwen3vl.service</b> 处于 failed；恢复后 AI 观察自动切回真实模型。</span>
            </div>
          )}
        </div>
        <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
          <div className="grid" style={{ gridTemplateColumns: "1fr 1fr" }}>
            <Card title="CPU 温度" sub="°C" right={<span className="num" style={{ fontSize: 16, fontWeight: 600, color: h.cpu_temp_c >= T.cpu_temp_c.warn ? "var(--warn)" : undefined }}>{h.cpu_temp_c ?? "—"}°C</span>}>
              <TimeChart data={sim.getSeries("cpu_temp")} warn={T.cpu_temp_c.warn} danger={T.cpu_temp_c.danger} height={120} color="#e0a23e" fmt={(v) => Math.round(v)} />
            </Card>
            <Card title="负载 (1m)" sub="8 cores" right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{h.cpu_load_1m ?? "—"}</span>}>
              <TimeChart data={sim.getSeries("load")} height={120} yMin={0} color="#5d9cf5" />
            </Card>
          </div>
          <Card title="内存占用" sub={"NPU 常驻模型约 3.1 GB · 共 " + ((h.mem_total_mb || 15876) / 1024).toFixed(0) + " GB"}
            right={<span className="num" style={{ fontSize: 16, fontWeight: 600 }}>{h.mem_used_mb ? (h.mem_used_mb / 1024).toFixed(1) + " GB" : "—"}</span>}>
            <TimeChart data={sim.getSeries("mem_used")} height={130} color="#a18ef5" yMin={0} yMax={h.mem_total_mb || 15876} fmt={(v) => (v / 1024).toFixed(0) + "G"} />
            {deg && <p className="t3" style={{ margin: "8px 0 0", fontSize: 11.5 }}>内存下降 ≈2.9 GB：Qwen3-VL 模型已随服务退出释放——这是判断模型未驻留的直接信号。</p>}
          </Card>
          <Card title="磁盘使用">
            <div style={{ display: "flex", alignItems: "center", gap: 14 }}>
              <div style={{ flex: 1, height: 10, background: "var(--inset)", border: "1px solid var(--line)", borderRadius: 99, overflow: "hidden" }}>
                <div style={{ width: (h.disk_used_pct || 0) + "%", height: "100%", background: "var(--accent)", opacity: 0.75 }}></div>
              </div>
              <span className="num" style={{ fontSize: 14, fontWeight: 600 }}>{h.disk_used_pct ?? "—"}%</span>
            </div>
            <p className="t3" style={{ margin: "8px 0 0", fontSize: 11.5 }}>eMMC 64 GB · 模型文件 ~4.2 GB · 滚动日志上限 2 GB</p>
          </Card>
        </div>
      </div>
    </div>
  );
}
