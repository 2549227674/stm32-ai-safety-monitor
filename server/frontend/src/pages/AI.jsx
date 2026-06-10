import React from 'react'
import { Card, Pill, Tag, Empty, ObservationRow, systemStatus, riskLevel, riskColor, timeAgo, fmtTime } from '../components/Widgets'
import { TimingBar } from '../components/Charts'

export default function PageAI({ sim }) {
  const [selId, setSelId] = React.useState(null);
  const list = sim.ai;
  const sel = list.find((o) => o.id === selId) || list[0];
  const deg = sim.scenario === "degraded";
  const off = sim.scenario === "offline";
  const okObs = list.filter((o) => o.ok && o.status === "ok");
  const avgTotal = okObs.length ? okObs.reduce((a, o) => a + o.run_metrics.total_ms, 0) / okObs.length : null;
  const avgTps = okObs.length ? okObs.reduce((a, o) => a + (o.run_metrics.tokens_per_s || 0), 0) / okObs.length : null;
  const failRate = list.length ? list.filter((o) => !o.ok).length / list.length : 0;

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16 }} data-screen-label="AI 推理">
      <div className="status-strip">
        <div className="status-cell"><span className="lbl">推理服务</span><span className="val">{off ? <Pill level="danger">未知</Pill> : deg ? <Pill level="danger">connection refused</Pill> : <Pill level="ok">qwen3vl · :8080</Pill>}</span></div>
        <div className="status-cell"><span className="lbl">模型</span><span className="val" style={{ fontSize: 12.5 }}>{deg ? <Tag level="degraded">mock-detector</Tag> : <Tag level="accent">Qwen3-VL 2B</Tag>}<span className="sm">{deg ? "cpu" : "RKNN-LLM · NPU×3"}</span></span></div>
        <div className="status-cell"><span className="lbl">平均端到端</span><span className="val num" style={{ fontSize: 15 }}>{avgTotal ? (avgTotal / 1000).toFixed(1) + " s" : "—"}</span></div>
        <div className="status-cell"><span className="lbl">解码速度</span><span className="val num" style={{ fontSize: 15 }}>{avgTps ? avgTps.toFixed(1) + " tok/s" : "—"}</span></div>
        <div className="status-cell"><span className="lbl">失败率 (近 24 次)</span><span className="val num" style={{ fontSize: 15, color: failRate > 0.15 ? "var(--warn)" : undefined }}>{Math.round(failRate * 100)}%</span></div>
        <div className="status-cell"><span className="lbl">常驻内存</span><span className="val num" style={{ fontSize: 15 }}>{okObs[0] ? (okObs[0].run_metrics.mem_rss_mb / 1024).toFixed(1) + " GB" : deg ? "0.14 GB" : "—"}</span></div>
      </div>
      {deg && (
        <div className="banner degraded">
          <span style={{ fontSize: 16 }}>◐</span>
          <span style={{ flex: 1 }}><b>降级模式</b> — systemd 单元 <span className="mono">opi5-ai-qwen3vl.service</span> 状态 failed；device-agent 自动回退 mock 检测器，观察结果带 <span className="mono">[mock]</span> 标记，不可用于真实研判。</span>
        </div>
      )}
      <div className="grid" style={{ gridTemplateColumns: "minmax(0, 5fr) minmax(0, 7fr)" }}>
        <Card title="观察历史" sub="window 30s · 演示加速至 18s" flush>
          <div style={{ maxHeight: 520, overflowY: "auto" }}>
            {list.length ? list.map((o) => (
              <ObservationRow key={o.id} obs={o} active={sel && sel.id === o.id} onClick={() => setSelId(o.id)} />
            )) : <Empty>暂无观察记录</Empty>}
          </div>
        </Card>
        <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
          {sel ? (
            <Card title={"观察详情 · " + sel.id} sub={fmtTime(sel.timestamp) + " · window " + (sel.window_sec || 30) + "s"}
              right={sel.ok ? (sel.status === "mock" ? <Tag level="degraded">mock</Tag> : <Tag level={sel.risk_hint >= 7 ? "danger" : sel.risk_hint >= 4 ? "warn" : "ok"}>risk_hint {sel.risk_hint}/10</Tag>) : <Tag level="warn">{sel.status}</Tag>}>
              {sel.ok ? (
                <div style={{ display: "flex", flexDirection: "column", gap: 14 }}>
                  {(sel.labels || []).length > 0 && (
                    <div style={{ display: "flex", gap: 6, flexWrap: "wrap" }}>
                      {sel.labels.map((l) => <Tag key={l} level={["flame", "fire", "smoke", "heat"].includes(l) ? "danger" : ""}>{l}</Tag>)}
                    </div>
                  )}
                  <div className="aiconsole"><span className="hl">[summary]</span>{"\n"}{sel.summary}{"\n\n"}<span className="hl">[full_text]</span>{"\n"}{sel.full_text}</div>
                  {sel.run_metrics && sel.run_metrics.imgenc_ms && (
                    <div style={{ display: "flex", flexDirection: "column", gap: 9 }}>
                      <span style={{ fontSize: 11.5, color: "var(--text-3)", letterSpacing: "0.05em" }}>推理耗时分解</span>
                      <TimingBar metrics={sel.run_metrics} />
                      <div className="kv" style={{ paddingTop: 4 }}>
                        <dt>首 token (TTFT)</dt><dd>{(sel.run_metrics.ttft_ms / 1000).toFixed(2)} s</dd>
                        <dt>输出 tokens</dt><dd>{sel.run_metrics.tokens_out} · {sel.run_metrics.tokens_per_s} tok/s</dd>
                        <dt>RSS 内存</dt><dd>{sel.run_metrics.mem_rss_mb} MB</dd>
                        <dt>control_allowed</dt><dd style={{ color: "var(--ok)" }}>false（永不下发控制）</dd>
                      </div>
                    </div>
                  )}
                </div>
              ) : (
                <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
                  <div className="aiconsole" style={{ color: "var(--warn)" }}>{sel.error}</div>
                  <p className="t3" style={{ margin: 0, fontSize: 12, lineHeight: 1.6 }}>失败的观察不会上报 risk_hint；device-agent 将在下一个 30s 窗口重试。连续失败将触发 mock 降级并产生 WARN 告警。</p>
                </div>
              )}
            </Card>
          ) : <Card title="观察详情"><Empty>从左侧选择一次观察</Empty></Card>}
        </div>
      </div>
    </div>
  );
}
