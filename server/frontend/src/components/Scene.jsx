import React from 'react'

const TWIN_W = 760, TWIN_D = 540;
const fx = (x) => x - TWIN_W / 2;
const fy = (y) => y - TWIN_D / 2;

const RACKS = [
  { x: 150, y: 96, w: 66, d: 54, h: 104 },
  { x: 150, y: 168, w: 66, d: 54, h: 104 },
  { x: 232, y: 96, w: 66, d: 54, h: 88 },
];
const EDGE_BOX = { x: 96, y: 452, w: 92, d: 62, h: 64 };
const EDGE_ANCHOR = { x: EDGE_BOX.x + EDGE_BOX.w / 2, y: EDGE_BOX.y + EDGE_BOX.d / 2 };
const CAM = { x: 648, y: 150, hz: 104, dir: 152, range: 340 };
const FLOW_SOURCES = [
  { x: 430, y: 130 }, { x: 210, y: 175 }, { x: 300, y: 205 },
  { x: 600, y: 440 }, { x: 240, y: 470 },
];

function IsoBox({ box, klass, label }) {
  const { x, y, w, d, h } = box;
  return (
    <div className={"iso-box " + (klass || "")} style={{ left: fx(x), top: fy(y), width: w, height: d }}>
      <div className="face top" style={{ width: w, height: d, transform: `translateZ(${h}px)` }}>
        {label && <span className="toplabel">{label}</span>}
      </div>
      <div className="face south" style={{ width: w, height: h, transform: `translateY(${d}px) rotateX(90deg)` }}></div>
      <div className="face east" style={{ width: h, height: d, transform: `translateX(${w}px) rotateY(-90deg)` }}></div>
    </div>
  );
}

function IsoDevice({ x, y, label, value, color, hz = 54, alarm, pulse }) {
  return (
    <div className={"iso-dev" + (alarm ? " alarm" : "")} style={{ left: fx(x), top: fy(y), "--mk": color }}>
      <div className="iso-disc"></div>
      <div className="iso-ring"></div>
      {pulse && <div className="iso-pulse"></div>}
      <div className="iso-stem" style={{ height: hz }}></div>
      <div className="iso-head" style={{ "--hz": hz + "px" }}>
        <div className="iso-chip">
          <span className="cdot"></span>{label}
          {value != null && <span className="cval">{value}</span>}
        </div>
      </div>
    </div>
  );
}

export default function IsoScene({ sim }) {
  const scen = sim.scenario;
  const off = scen === "offline";
  const alarm = scen === "alarm";
  const deg = scen === "degraded";
  const risk = sim.latest("risk");

  React.useEffect(() => {
    if (document.getElementById("iso-flow-kf")) return;
    let css = "";
    FLOW_SOURCES.forEach((s, i) => {
      const dx = (EDGE_ANCHOR.x - s.x).toFixed(1);
      const dy = (EDGE_ANCHOR.y - s.y).toFixed(1);
      css += `@keyframes isoFlow${i}{0%{transform:translate(0,0);opacity:0}10%{opacity:1}90%{opacity:1}100%{transform:translate(${dx}px,${dy}px);opacity:0}}`;
    });
    const st = document.createElement("style");
    st.id = "iso-flow-kf";
    st.textContent = css;
    document.head.appendChild(st);
  }, []);

  const devColor = (base, isAlarm) => (off ? "#59626f" : isAlarm ? "#e5484d" : base);
  const flameActive = alarm || sim.latest("flame");
  const mq2Active = alarm || sim.latest("mq2");
  const pirActive = !off && sim.latest("pir");
  const twinClass = "twin" + (off ? " is-offline" : alarm ? " is-alarm" : deg ? " is-degraded" : "");

  return (
    <div className={twinClass}>
      <div className="iso-stage">
        <div className="iso-world" style={{ "--iso-scale": 1 }}>
          <div className="iso-floor" style={{ left: fx(0), top: fy(0), width: TWIN_W, height: TWIN_D }}>
            <span className="iso-zonelabel" style={{ left: 14, top: 10 }}>巡检区 A · 设备机房</span>
            <span className="iso-zonelabel" style={{ left: 150, top: 44, color: alarm ? "var(--danger)" : undefined }}>RACK ROW 01</span>
            <span className="iso-zonelabel" style={{ left: 70, bottom: 8, color: "var(--accent)" }}>EDGE NODE</span>
          </div>
          {!off && (
            <div className="iso-fov" style={{
              left: fx(CAM.x), top: fy(CAM.y), width: CAM.range, height: CAM.range,
              transform: `rotateZ(${CAM.dir}deg)`,
            }}></div>
          )}
          {RACKS.map((r, i) => <IsoBox key={i} box={r} klass="rack" label={`R0${i + 1}`} />)}
          <IsoBox box={EDGE_BOX} klass="edge" label="OPi5" />
          {alarm && (
            <React.Fragment>
              <div className="iso-alarmglow" style={{ left: fx(195) - 90, top: fy(160) - 90, width: 220, height: 220 }}></div>
              {[0, 0.9, 1.7].map((d, i) => (
                <div key={i} className="iso-smoke" style={{ left: fx(205), top: fy(165), animationDelay: d + "s" }}></div>
              ))}
            </React.Fragment>
          )}
          {!off && FLOW_SOURCES.map((s, i) => (
            <div key={i} className="iso-dev" style={{ left: fx(s.x), top: fy(s.y) }}>
              <div className="iso-flow" style={{
                background: deg ? "var(--degraded)" : alarm ? "var(--danger)" : "var(--accent)",
                boxShadow: `0 0 8px ${deg ? "var(--degraded)" : alarm ? "var(--danger)" : "var(--accent)"}`,
                animation: `isoFlow${i} ${2.4 + i * 0.25}s linear ${i * 0.4}s infinite`,
              }}></div>
            </div>
          ))}
          {!off && (
            <div className="iso-dev" style={{ left: fx(EDGE_ANCHOR.x), top: fy(EDGE_ANCHOR.y) }}>
              <div className="iso-beam" style={{ height: 170 }}>
                <span className="up"></span>
                <span className="up" style={{ animationDelay: "0.9s" }}></span>
              </div>
              <div className="iso-head" style={{ "--hz": "204px" }}>
                <div className="iso-chip" style={{ "--mk": deg ? "var(--degraded)" : "var(--accent)" }}>
                  <span className="cdot"></span>云端 Backend
                  <span className="cval">{deg ? "降级" : "5s 心跳"}</span>
                </div>
              </div>
            </div>
          )}
          <IsoDevice x={CAM.x} y={CAM.y} hz={CAM.hz} label="CAM-01" value={off ? "断流" : "25fps"}
            color={devColor("#57c1d5", alarm)} alarm={alarm} pulse={!off} />
          <IsoDevice x={430} y={130} label="PIR" value={off ? "—" : pirActive ? "触发" : "待机"}
            color={devColor("#5d9cf5", false)} alarm={pirActive} pulse={pirActive} />
          <IsoDevice x={210} y={175} label="火焰" value={off ? "—" : flameActive ? "报警" : "正常"}
            color={devColor("#e0a23e", flameActive)} alarm={flameActive} pulse={flameActive} />
          <IsoDevice x={300} y={205} label="MQ-2" value={off ? "—" : mq2Active ? "高" : "正常"}
            color={devColor("#e0a23e", mq2Active)} alarm={mq2Active} pulse={mq2Active} />
          <IsoDevice x={600} y={440} label="DHT11 预留" value={off ? "—" : (sim.latest("temp") + "°C")}
            color={devColor("#41bd80", false)} hz={48} />
          <IsoDevice x={140} y={300} label="本地安全" value={off ? "—" : alarm ? "激活" : "待命"}
            color={devColor("#a3abba", alarm)} alarm={alarm} pulse={alarm} hz={44} />
          <IsoDevice x={240} y={470} label="OPi5 Agent" value={off ? "离线" : "在线"}
            color={devColor("#57c1d5", false)} hz={50} />
        </div>
      </div>
      <div className="twin-hud">
        <div className="twin-corner">
          <span className="hud-chip" style={{ pointerEvents: "auto" }}>
            <span className="dot" style={{ background: off ? "var(--off)" : alarm ? "var(--danger)" : deg ? "var(--degraded)" : "var(--ok)", boxShadow: off ? "none" : "0 0 5px currentColor" }}></span>
            数字孪生 · edge-opi5-001
          </span>
          <span className="hud-chip" style={{ pointerEvents: "auto" }}>
            risk {off ? "—" : risk != null ? risk.toFixed(1) : "—"}/10 · {off ? "OFFLINE" : alarm ? "ALARM" : deg ? "DEGRADED" : "NOMINAL"}
          </span>
        </div>
        <div className="twin-legend pointer-events">
          <span className="lg"><i style={{ background: "#57c1d5" }}></i>视觉 / 边缘</span>
          <span className="lg"><i style={{ background: "#5d9cf5" }}></i>移动 PIR</span>
          <span className="lg"><i style={{ background: "#e0a23e" }}></i>火焰 / 烟雾</span>
          <span className="lg"><i style={{ background: "#41bd80" }}></i>环境</span>
          <span className="lg"><i style={{ background: "var(--accent)", borderRadius: 9 }}></i>遥测流 → 边缘盒 → 云端</span>
        </div>
      </div>
      {off && (
        <div className="twin-overlaystate">
          <span style={{ fontSize: 13, color: "var(--danger)" }}>● 信号丢失 / NO SIGNAL</span>
          <span style={{ fontSize: 11, color: "var(--text-3)" }}>显示最后一次心跳快照 · 本地安全闭环持续运行</span>
        </div>
      )}
    </div>
  );
}
