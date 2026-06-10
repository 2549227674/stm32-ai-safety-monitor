import React from 'react'
import { Card, Pill, Tag, Empty, timeAgo, fmtTime } from '../components/Widgets'

function NotifStatusTag({ status }) {
  if (status === "sent") return <Tag level="ok">sent</Tag>;
  if (status === "failed") return <Tag level="danger">failed</Tag>;
  if (status === "skipped") return <Tag level="warn">skipped</Tag>;
  return <Tag>—</Tag>;
}

export default function PageNotify({ sim }) {
  const N = sim.notif;
  const S = N.settings;
  const [draft, setDraft] = React.useState(() => ({ ...S, smtp_password: "" }));
  const [savedAt, setSavedAt] = React.useState(null);
  const [testing, setTesting] = React.useState(false);
  const [testResult, setTestResult] = React.useState(null);
  const [logFilter, setLogFilter] = React.useState("all");
  const [logQuery, setLogQuery] = React.useState("");

  const dirty = draft.enabled !== S.enabled || draft.alert_email_to !== S.alert_email_to || Number(draft.cooldown_seconds) !== S.cooldown_seconds || draft.smtp_host !== S.smtp_host || Number(draft.smtp_port) !== S.smtp_port || draft.smtp_user !== S.smtp_user || draft.smtp_from !== S.smtp_from || draft.smtp_password.length > 0;

  function save() {
    const patch = { enabled: draft.enabled, alert_email_to: draft.alert_email_to.trim(), cooldown_seconds: Math.max(0, Number(draft.cooldown_seconds) || 0), smtp_host: draft.smtp_host.trim(), smtp_port: Number(draft.smtp_port) || 0, smtp_user: draft.smtp_user.trim(), smtp_from: draft.smtp_from.trim() };
    sim.updateNotifSettings(patch);
    setDraft((d) => ({ ...d, ...patch, smtp_password: "" }));
    setSavedAt(Date.now());
  }

  function sendTest() {
    if (testing) return;
    setTesting(true); setTestResult(null);
    sim.sendTestEmail((entry) => { setTesting(false); setTestResult(entry); });
  }

  const logs = N.logs.filter((l) => {
    if (logFilter !== "all" && l.status !== logFilter) return false;
    if (logQuery) { const q = logQuery.toLowerCase(); return (l.alert_id + " " + l.recipient + " " + l.subject + " " + (l.error || "")).toLowerCase().includes(q); }
    return true;
  });

  const lr = N.last_result;
  const field = { display: "flex", flexDirection: "column", gap: 5 };
  const lbl = { fontSize: 11.5, color: "var(--text-3)" };

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16, maxWidth: 1080 }} data-screen-label="通知设置">
      <div className="status-strip">
        <div className="status-cell"><span className="lbl">邮件告警</span><span className="val">{S.enabled ? <Pill level="ok">已启用</Pill> : <Pill level="off">已关闭</Pill>}</span></div>
        <div className="status-cell"><span className="lbl">配置状态</span><span className="val">{S.configured ? <Tag level="ok">configured</Tag> : <Tag level="warn">未完成</Tag>}</span></div>
        <div className="status-cell"><span className="lbl">收件人</span><span className="val mono" style={{ fontSize: 12, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{S.alert_email_to || "—"}</span></div>
        <div className="status-cell"><span className="lbl">冷却时间</span><span className="val num" style={{ fontSize: 14 }}>{S.cooldown_seconds}s</span></div>
        <div className="status-cell"><span className="lbl">最近发送</span><span className="val" style={{ fontSize: 12.5 }}>{lr ? <NotifStatusTag status={lr.status} /> : <Tag>—</Tag>}<span className="sm">{lr ? timeAgo(lr.ts) : ""}</span></span></div>
      </div>
      <div className="grid" style={{ gridTemplateColumns: "minmax(0, 5fr) minmax(0, 7fr)" }}>
        <div style={{ display: "flex", flexDirection: "column", gap: 16, minWidth: 0 }}>
          <Card title="邮件配置" sub="PUT /api/notification/settings"
            right={<div style={{ display: "flex", gap: 8, alignItems: "center" }}>
              {savedAt && !dirty && <span className="t3 mono" style={{ fontSize: 11 }}>已保存 ✓</span>}
              <button className="btn primary" disabled={!dirty} style={{ opacity: dirty ? 1 : 0.45 }} onClick={save}>保存</button>
            </div>}>
            <div style={{ display: "flex", flexDirection: "column", gap: 14 }}>
              <label style={{ display: "flex", alignItems: "center", gap: 10, cursor: "pointer" }}>
                <span role="switch" aria-checked={draft.enabled} onClick={() => setDraft({ ...draft, enabled: !draft.enabled })}
                  style={{ width: 34, height: 19, borderRadius: 99, flex: "none", position: "relative", background: draft.enabled ? "var(--accent-dim)" : "var(--inset)", border: "1px solid " + (draft.enabled ? "rgba(87,193,213,0.5)" : "var(--line-strong)"), transition: "background 0.15s" }}>
                  <span style={{ position: "absolute", top: 2, left: draft.enabled ? 17 : 2, width: 13, height: 13, borderRadius: "50%", background: draft.enabled ? "var(--accent)" : "var(--text-3)", transition: "left 0.15s" }}></span>
                </span>
                <span style={{ fontSize: 13 }}>启用邮件告警</span>
                <span className="t3" style={{ fontSize: 11.5 }}>仅 danger 级告警触发</span>
              </label>
              <div style={field}><span style={lbl}>收件人 alert_email_to</span><input className="input" value={draft.alert_email_to} placeholder="user@domain.com" onChange={(e) => setDraft({ ...draft, alert_email_to: e.target.value })} /></div>
              <div style={field}><span style={lbl}>冷却时间 cooldown_seconds</span><div style={{ display: "flex", alignItems: "center", gap: 8 }}><input className="input" type="number" min="0" step="30" style={{ width: 110 }} value={draft.cooldown_seconds} onChange={(e) => setDraft({ ...draft, cooldown_seconds: e.target.value })} /><span className="t3" style={{ fontSize: 11.5 }}>秒 · 冷却期内的告警记为 skipped</span></div></div>
              <div style={{ borderTop: "1px solid var(--line)", paddingTop: 12, display: "flex", flexDirection: "column", gap: 12 }}>
                <span style={{ fontSize: 11, color: "var(--text-3)", letterSpacing: "0.08em" }}>SMTP 服务器</span>
                <div className="grid" style={{ gridTemplateColumns: "1fr 92px", gap: 10 }}>
                  <div style={field}><span style={lbl}>host</span><input className="input" value={draft.smtp_host} onChange={(e) => setDraft({ ...draft, smtp_host: e.target.value })} /></div>
                  <div style={field}><span style={lbl}>port</span><input className="input" type="number" value={draft.smtp_port} onChange={(e) => setDraft({ ...draft, smtp_port: e.target.value })} /></div>
                </div>
                <div style={field}><span style={lbl}>用户名 user</span><input className="input" value={draft.smtp_user} onChange={(e) => setDraft({ ...draft, smtp_user: e.target.value })} /></div>
                <div style={field}><span style={lbl}>密码 password</span><input className="input" type="password" value={draft.smtp_password} placeholder="●●●●●●●●（已保存，输入以更换）" autoComplete="new-password" onChange={(e) => setDraft({ ...draft, smtp_password: e.target.value })} /><span className="t3" style={{ fontSize: 11 }}>密码仅写入，后端不会回传明文。</span></div>
                <div style={field}><span style={lbl}>发件人 from</span><input className="input" value={draft.smtp_from} onChange={(e) => setDraft({ ...draft, smtp_from: e.target.value })} /></div>
              </div>
            </div>
          </Card>
          <Card title="测试邮件" sub="POST /api/notification/test-email">
            <div style={{ display: "flex", flexDirection: "column", gap: 10 }}>
              <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
                <button className="btn primary" onClick={sendTest} disabled={testing} style={{ opacity: testing ? 0.6 : 1, display: "inline-flex", alignItems: "center", gap: 8 }}>
                  {testing && <span className="spinner" style={{ width: 12, height: 12, borderWidth: 2 }}></span>}
                  {testing ? "发送中…" : "发送测试邮件"}
                </button>
                <span className="t3 mono" style={{ fontSize: 11, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>→ {S.alert_email_to || "(未配置收件人)"}</span>
              </div>
              {testResult && (
                <div className="banner" style={{ padding: "8px 12px", fontSize: 12.5, background: testResult.status === "sent" ? "var(--ok-dim)" : "var(--danger-dim)", borderColor: testResult.status === "sent" ? "rgba(65,189,128,0.35)" : "rgba(229,72,77,0.4)", color: testResult.status === "sent" ? "#9fdcbc" : "#f2b8ba" }}>
                  {testResult.status === "sent" ? <span>✓ 测试邮件已发送至 <b className="mono">{testResult.recipient}</b></span> : <span>✕ 发送失败 — <span className="mono" style={{ fontSize: 11.5 }}>{testResult.error}</span></span>}
                </div>
              )}
            </div>
          </Card>
        </div>
        <Card title="通知日志" sub="GET /api/notification/logs" flush
          right={<div style={{ display: "flex", gap: 8, alignItems: "center" }}>
            <input className="input" style={{ width: 150 }} placeholder="搜索 alert_id / 收件人…" value={logQuery} onChange={(e) => setLogQuery(e.target.value)} />
            <div className="seg">{[["all", "全部"], ["sent", "sent"], ["failed", "failed"], ["skipped", "skipped"]].map(([v, l]) => (<button key={v} className={logFilter === v ? "on" : ""} onClick={() => setLogFilter(v)}>{l}</button>))}</div>
          </div>}>
          {logs.length ? (
            <div style={{ maxHeight: 560, overflowY: "auto" }}>
              <table className="table">
                <thead><tr><th style={{ width: 86 }}>时间</th><th style={{ width: 84 }}>alert_id</th><th style={{ width: 70 }}>状态</th><th>主题 / 收件人</th></tr></thead>
                <tbody>
                  {logs.slice(0, 50).map((l) => (
                    <tr key={l.id}>
                      <td className="mono" style={{ fontSize: 11.5, color: "var(--text-3)", whiteSpace: "nowrap" }}>{fmtTime(l.ts)}</td>
                      <td className="mono" style={{ fontSize: 11.5 }}>{l.alert_id}</td>
                      <td><NotifStatusTag status={l.status} /></td>
                      <td><div style={{ fontSize: 12.5, color: "var(--text)" }}>{l.subject}</div><div className="mono t3" style={{ fontSize: 11 }}>{l.channel} → {l.recipient}</div>{l.error && <div className="mono" style={{ fontSize: 11, color: l.status === "failed" ? "var(--danger)" : "var(--warn)", marginTop: 2 }}>{l.error}</div>}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          ) : <Empty icon="✉">没有匹配的通知记录</Empty>}
        </Card>
      </div>
      <p className="t3" style={{ margin: 0, fontSize: 11.5 }}>邮件由云端 backend 经 SMTP 发出，浏览器只调用 notification API；邮件仅是告警的通知通道之一，设备端本地告警闭环不依赖邮件。</p>
    </div>
  );
}
