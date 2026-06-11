/* ============================================================
   EdgeSim — 前端数据引擎
   支持两种模式：
   1. 真实数据模式：连接后端 API + SSE，获取真实设备数据
   2. Mock 模式：当无真实设备时，自动生成模拟数据

   启动逻辑：
   - 尝试 GET /api/devices 检测是否有在线设备
   - 有设备 → 订阅 SSE + 轮询 API
   - 无设备 / API 不可达 → 启动 mock 引擎
   ============================================================ */
(function () {
  "use strict";

  const HIST_SEC = 720;
  const AI_EVERY = 18;
  const HB_EVERY = 5;

  const rnd = (a, b) => a + Math.random() * (b - a);
  const g = (s) => (Math.random() + Math.random() + Math.random() - 1.5) * s;
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const r1 = (v) => Math.round(v * 10) / 10;
  const r2 = (v) => Math.round(v * 100) / 100;

  const THRESHOLDS = {
    risk_score: { warn: 4, danger: 7, unit: "score", label: "风险分数" },
    cpu_temp_c: { warn: 75, danger: 85, unit: "°C", label: "CPU 温度" },
    "mpu6500.vibration_score": { warn: 4, danger: 7, unit: "score", label: "震动分数" },
    smoke_score: { warn: 3, danger: 6, unit: "score", label: "烟雾分数" },
  };

  const METRICS = [
    "risk", "vib", "ax", "ay", "az", "gx", "gy", "gz",
    "temp", "hum", "lux", "cpu_temp", "mem_used", "load", "disk",
    "pir", "flame", "mq2", "smoke_score",
  ];

  const NORMAL_SUMMARIES = [
    "画面为室内巡检区域，画面清晰，未发现人员、烟雾或火焰，整体无异常。",
    "巡检区域内设备摆放整齐，未检测到移动目标或热源异常，状态正常。",
    "画面静止，巡检区域可见，传感器遥测在正常区间内，无需人工关注。",
    "检测到一名工作人员在画面边缘短暂经过，无烟雾火焰特征，风险较低。",
  ];
  const NORMAL_FULL = [
    "根据当前摄像头画面与过去 30 秒遥测摘要：画面中为固定巡检区域，可见设备机柜与走线，光照均匀，无遮挡。未检测到人员驻留、烟雾扩散或火焰高亮区域。震动分数与环境温湿度均处于正常区间，设备 CPU 温度稳定。综合判断风险等级为低，无需人工介入。建议保持当前巡检频率。",
    "画面显示巡检点位视野完整，边缘有轻微反光属于正常灯光反射，不构成火焰特征。遥测摘要显示 MQ-2 与火焰传感器均未触发，震动幅度小。判断现场无安全异常，本次观察不需要人工关注。",
  ];
  const ALARM_SUMMARIES = [
    "画面下方检测到明显火焰亮区并伴随烟雾扩散，火焰传感器同时触发，建议立即人工确认。",
    "检测到持续火焰特征与上升烟柱，MQ-2 烟雾信号为高，风险等级很高，请立即处置。",
  ];
  const ALARM_FULL = [
    "根据当前摄像头画面与过去 30 秒遥测摘要：画面右下区域出现高亮橙色火焰特征，边缘伴随灰白色烟雾向上扩散，与火焰传感器（flame=1）和 MQ-2 烟雾信号一致，属于多源交叉确认。环境温度短时上升，震动无明显异常。综合判断为火灾风险，风险提示 8/10。请立即人工确认现场情况。注意：本系统不输出任何执行器控制命令，本地告警闭环已由设备端独立完成。",
  ];
  const ALARM_LABELS = [["flame", "smoke"], ["fire", "smoke", "heat"]];
  const NORMAL_LABELS = [[], ["person"], [], ["normal_scene"]];

  /* ---------------- state ---------------- */
  const sim = {
    mode: "detecting",  // detecting | real | mock
    scenario: "normal",
    speed: 1,
    ready: false,
    buf: {},
    ai: [],
    alerts: [],
    events: [],
    device: null,
    listeners: new Set(),
    thresholds: THRESHOLDS,
    _tick: 0,
    _pirHold: 0,
    _cpuBase: 52,
    _lastSeen: 0,
    _aiSeq: 412,
    _alertSeq: 1280,
    _notifSeq: 9032,
    notif: {
      settings: {
        enabled: true,
        configured: true,
        alert_email_to: "ops-oncall@example.com",
        smtp_host: "smtp.example.com",
        smtp_port: 465,
        smtp_user: "alert-bot@example.com",
        smtp_from: "边缘巡检告警 <alert-bot@example.com>",
        cooldown_seconds: 300,
      },
      logs: [],
      last_sent_at: 0,
      last_result: null,
    },
    _alarmEntered: 0,
    _degradedCount: 0,
    _timer: null,
    _sseSource: null,
    _pollTimer: null,
    _realDeviceId: null,
    videoOnline: false,
    videoError: null,
    cameraStatus: "unknown",
    _realtimeTs: null,
    _realtimeAge: 0,
  };
  METRICS.forEach((m) => (sim.buf[m] = []));

  function push(key, t, v) {
    const b = sim.buf[key];
    b.push({ t, v });
    if (b.length > HIST_SEC) b.shift();
  }
  function latest(key) {
    const b = sim.buf[key];
    return b.length ? b[b.length - 1].v : null;
  }

  function pushEvent(level, source, msg, t) {
    sim.events.unshift({ ts: t || nowSec(), level, source, msg, id: Math.random().toString(36).slice(2) });
    if (sim.events.length > 250) sim.events.pop();
  }
  function pushAlert(level, metric, msg, t) {
    const alert = { ts: t || nowSec(), level, metric, msg, id: "alr_" + (++sim._alertSeq), email: null };
    sim.alerts.unshift(alert);
    if (sim.alerts.length > 80) sim.alerts.pop();
    pushEvent(level, "alert", msg, t);
    maybeNotify(alert);
  }

  /* ---------------- email notification (后端 notification API 的 mock) ---------------- */
  function maybeNotify(alert) {
    const N = sim.notif;
    const s = N.settings;
    if (alert.level !== "danger") return;                     // 仅 danger 级触发邮件
    if (!s.enabled || !s.configured) return;
    const t = alert.ts;
    if (N.last_sent_at && t - N.last_sent_at < s.cooldown_seconds) {
      N.logs.unshift({
        id: "ntf_" + (++sim._notifSeq), ts: t, alert_id: alert.id, channel: "email",
        recipient: s.alert_email_to, subject: subjectFor(alert),
        status: "skipped", error: `cooldown 未过 (${s.cooldown_seconds}s)`,
      });
      alert.email = { status: "skipped", ts: t };
      return;
    }
    const fail = Math.random() < 0.12; // 偶发 SMTP 失败，用于展示 failed 状态
    const entry = {
      id: "ntf_" + (++sim._notifSeq), ts: t, alert_id: alert.id, channel: "email",
      recipient: s.alert_email_to, subject: subjectFor(alert),
      status: fail ? "failed" : "sent",
      error: fail ? "SMTPException: connection to host timed out (30s)" : null,
    };
    N.logs.unshift(entry);
    if (N.logs.length > 120) N.logs.pop();
    if (!fail) N.last_sent_at = t;
    N.last_result = { status: entry.status, ts: t, error: entry.error };
    alert.email = { status: entry.status, ts: t };
    pushEvent(fail ? "warn" : "info", "notify",
      fail ? `邮件告警发送失败 → ${s.alert_email_to}（SMTP 超时）` : `邮件告警已发送 → ${s.alert_email_to}`, t);
  }
  function subjectFor(alert) {
    return `[EdgeSafety] 告警 · ${alert.metric} · edge-opi5-001`;
  }

  const nowSec = () => Math.floor(Date.now() / 1000);

  /* ============================================================
     数据归一化：统一后端返回格式
     ============================================================ */

  function normalizeDevice(dev) {
    if (!dev) return null;
    const s = dev.status || {};
    const hb = s.heartbeat || s || {};
    const h = hb.health || s.health || dev.health || {};
    const svcs = hb.services || s.services || dev.services || {};
    const aiInfo = hb.ai || s.ai || dev.ai || {};

    // Camera fields - could be object or string
    const rawCam = hb.camera || s.camera || dev.camera;
    const cam = typeof rawCam === "object" && rawCam !== null ? rawCam : {};
    const cameraStatus = hb.camera_status || s.camera_status || dev.camera_status || cam.status || (typeof rawCam === "string" ? rawCam : "unknown");
    const videoMode = hb.video_mode || s.video_mode || dev.video_mode || cam.mode || "unknown";
    const videoAvail = hb.video_available != null ? hb.video_available : s.video_available != null ? s.video_available : dev.video_available != null ? dev.video_available : cam.available;

    return {
      device_id: dev.device_id || hb.device_id,
      online: dev.online != null ? dev.online : (dev.last_seen_at ? (nowSec() - new Date(dev.last_seen_at).getTime() / 1000 < 15) : false),
      last_seen_at: dev.last_seen_at,
      agent_version: hb.agent_version || dev.agent_version || "—",
      contract_version: hb.contract_version || dev.contract_version || "—",
      ip: hb.ip || dev.ip || "—",
      uptime_s: hb.uptime_s || dev.uptime_s || null,
      health: {
        cpu_temp_c: h.cpu_temp_c ?? null,
        mem_used_mb: h.mem_used_mb ?? null,
        mem_total_mb: h.mem_total_mb ?? 15876,
        cpu_load_1m: h.cpu_load_1m ?? null,
        disk_used_pct: h.disk_used_pct ?? null,
      },
      services: {
        device_agent: svcs.device_agent || "unknown",
        "opi5-ai-qwen3vl": svcs["opi5-ai-qwen3vl"] || "unknown",
        "opi5-safetyd": svcs["opi5-safetyd"] || "unknown",
      },
      ai: {
        ok: aiInfo.ok != null ? aiInfo.ok : false,
        mode: aiInfo.mode || "unknown",
        model_ready: aiInfo.model_ready || false,
        error: aiInfo.error || null,
      },
      agent_url: hb.agent_url || s.agent_url || dev.agent_url || null,
      agent_port: hb.agent_port || s.agent_port || dev.agent_port || 8090,
      camera: cam,
      camera_status: cameraStatus,
      video_mode: videoMode,
      video_available: !!videoAvail,
    };
  }

  function normalizeAlert(a) {
    if (!a) return null;
    const ts = a.timestamp ? Math.floor(new Date(a.timestamp).getTime() / 1000) : (a.ts || nowSec());
    return {
      id: a.id || ("alr_" + Math.random().toString(36).slice(2)),
      ts,
      level: a.level || "info",
      metric: a.metric || "",
      msg: a.message || a.msg || "",
      source: a.source || a.device_id || "",
      email: a.email || null,
    };
  }

  function normalizeObservation(obs) {
    if (!obs) return null;
    const ts = obs.timestamp ? Math.floor(new Date(obs.timestamp).getTime() / 1000) : (obs.ts || nowSec());
    const id = obs.id || ("obs_" + ts);
    return {
      ...obs,
      id,
      timestamp: ts,
      ok: obs.ok != null ? obs.ok : (obs.status === "ok"),
      status: obs.status || (obs.ok ? "ok" : "error"),
      risk_hint: obs.risk_hint ?? null,
      summary: obs.summary || "",
      full_text: obs.full_text || obs.explanation || "",
      labels: obs.labels || [],
      run_metrics: obs.run_metrics || null,
      control_allowed: obs.control_allowed != null ? obs.control_allowed : false,
    };
  }

  function normalizePoint(p) {
    if (!p) return null;
    const t = p.t ? (typeof p.t === "number" ? p.t : Math.floor(new Date(p.t).getTime() / 1000))
      : p.timestamp ? Math.floor(new Date(p.timestamp).getTime() / 1000)
      : nowSec();
    const v = p.v != null ? p.v : p.value != null ? p.value : null;
    return { t, v };
  }

  function normalizeNotificationLog(l) {
    if (!l) return null;
    const ts = l.timestamp ? Math.floor(new Date(l.timestamp).getTime() / 1000) : (l.ts || nowSec());
    return {
      id: l.id || ("ntf_" + Math.random().toString(36).slice(2)),
      ts,
      alert_id: l.alert_id || "—",
      channel: l.channel || "email",
      recipient: l.recipient || "",
      subject: l.subject || "",
      status: l.status || "unknown",
      error: l.error || null,
      raw_json: l.raw_json || null,
    };
  }

  /* ============================================================
     真实数据模式：连接后端 API
     ============================================================ */

  async function connectReal() {
    try {
      const res = await fetch('/api/devices', { signal: AbortSignal.timeout(3000) });
      const data = await res.json();
      if (data.ok && data.devices && data.devices.length > 0) {
        // 只选择 online=true 的设备
        const onlineDevice = data.devices.find((d) => d.online);
        if (!onlineDevice) {
          console.log("[EdgeSim] 后端可达但无在线设备，切换到 mock 模式");
          pushEvent("info", "system", "后端可达但无在线设备，使用 mock 数据预览", nowSec());
          return false;
        }
        const dev = normalizeDevice(onlineDevice);
        sim.mode = "real";
        sim._realDeviceId = dev.device_id;
        sim.device = dev;
        sim.cameraStatus = dev.camera_status;
        sim.videoOnline = dev.video_available || dev.camera_status === "online" || dev.camera_status === "mock";
        pushEvent("info", "system", `真实设备已连接: ${dev.device_id}`, nowSec());
        console.log("[EdgeSim] 真实设备已连接:", dev.device_id);

        await loadInitialData(dev.device_id);
        connectSSE(dev.device_id);
        startPolling(dev.device_id);

        sim.ready = true;
        emit();
        return true;
      }
    } catch (e) {
      console.log("[EdgeSim] 后端不可达，切换到 mock 模式:", e.message);
    }
    return false;
  }

  async function loadInitialData(deviceId) {
    const t = nowSec();

    // 加载设备详情
    try {
      const res = await fetch(`/api/devices/${deviceId}`);
      const data = await res.json();
      if (data.ok && data.device) {
        sim.device = normalizeDevice(data.device);
        sim.cameraStatus = sim.device.camera_status;
        sim.videoOnline = sim.device.video_available || sim.device.camera_status === "online" || sim.device.camera_status === "mock";
        sim._lastSeen = sim.device.last_seen_at ? new Date(sim.device.last_seen_at).getTime() / 1000 : nowSec();
      }
    } catch (e) { /* ignore */ }

    // 加载阈值
    try {
      const res = await fetch(`/api/thresholds?device_id=${deviceId}`);
      const data = await res.json();
      if (data.ok && data.thresholds) {
        Object.keys(data.thresholds).forEach((k) => {
          if (sim.thresholds[k]) {
            sim.thresholds[k].warn = data.thresholds[k].warn;
            sim.thresholds[k].danger = data.thresholds[k].danger;
            sim.thresholds[k].label = data.thresholds[k].label || sim.thresholds[k].label;
            sim.thresholds[k].unit = data.thresholds[k].unit || sim.thresholds[k].unit;
          }
        });
      }
    } catch (e) { /* ignore */ }

    // 加载历史事件（最近 100 条）
    try {
      const res = await fetch(`/api/events?device_id=${deviceId}&limit=100`);
      const data = await res.json();
      if (data.ok && data.events && data.events.length) {
        const existingIds = new Set(sim.events.map((e) => e.id));
        for (const ev of data.events.reverse()) {
          const level = ev.risk_score >= 7 ? "danger" : ev.risk_score >= 4 ? "warn" : "info";
          const id = ev.event_id || ev.id || Math.random().toString(36).slice(2);
          if (existingIds.has(id)) continue;
          sim.events.push({
            ts: t,
            level,
            source: ev.type || "event",
            msg: `${ev.state || ""} · risk ${ev.risk_score}`,
            id,
          });
        }
        if (sim.events.length > 250) sim.events.splice(0, sim.events.length - 250);
      }
    } catch (e) { /* ignore */ }

    // 加载 AI 观察历史（最近 24 条）
    try {
      const res = await fetch(`/api/ai/observations?device_id=${deviceId}&limit=24`);
      const data = await res.json();
      if (data.ok && data.observations && data.observations.length) {
        const existingIds = new Set(sim.ai.map((o) => o.id));
        for (const raw of data.observations) {
          const obs = normalizeObservation(raw);
          if (obs && !existingIds.has(obs.id)) {
            sim.ai.push(obs);
          }
        }
        // Keep sorted newest first
        sim.ai.sort((a, b) => (b.timestamp || 0) - (a.timestamp || 0));
        if (sim.ai.length > 24) sim.ai.length = 24;
      }
    } catch (e) { /* ignore */ }

    // 加载告警
    try {
      const res = await fetch(`/api/alerts?device_id=${deviceId}&limit=20`);
      const data = await res.json();
      if (data.ok && data.alerts) {
        sim.alerts = data.alerts.map(normalizeAlert).filter(Boolean);
      }
    } catch (e) { /* ignore */ }

    // 加载遥测历史（最近 10 分钟）
    const telemetryMetrics = [
      { metric: "risk_score", key: "risk" },
      { metric: "cpu_temp_c", key: "cpu_temp" },
      { metric: "mpu6500.vibration_score", key: "vib" },
      { metric: "pir", key: "pir" },
      { metric: "flame", key: "flame" },
      { metric: "mq2", key: "mq2" },
      { metric: "smoke", key: "smoke_score" },
    ];
    await Promise.all(telemetryMetrics.map(async ({ metric, key }) => {
      try {
        const res = await fetch(`/api/telemetry/series?device_id=${deviceId}&metric=${metric}&seconds=600`);
        const data = await res.json();
        if (data.ok && data.points) {
          data.points.map(normalizePoint).filter(Boolean).forEach((p) => push(key, p.t, p.v));
        }
      } catch (e) { /* ignore */ }
    }));

    // 加载通知设置
    try {
      const res = await fetch('/api/notification/settings');
      const data = await res.json();
      if (data.ok && data.settings) {
        Object.assign(sim.notif.settings, data.settings);
      }
    } catch (e) { /* ignore */ }

    // 加载通知日志
    try {
      const res = await fetch('/api/notification/logs?limit=20');
      const data = await res.json();
      if (data.ok && data.logs) {
        sim.notif.logs = data.logs.map(normalizeNotificationLog).filter(Boolean);
      }
    } catch (e) { /* ignore */ }
  }

  function connectSSE(deviceId) {
    if (sim._sseSource) {
      sim._sseSource.close();
    }
    const es = new EventSource(`/api/stream/events?device_id=${deviceId}`);
    sim._sseSource = es;

    es.onmessage = (e) => {
      try {
        const tick = JSON.parse(e.data);
        processSSETick(tick);
      } catch (err) {
        console.warn("[EdgeSim] SSE parse error:", err);
      }
    };

    es.onerror = () => {
      console.warn("[EdgeSim] SSE 断开，3s 后重连...");
      es.close();
      setTimeout(() => connectSSE(deviceId), 3000);
    };
  }

  function processSSETick(tick) {
    const t = nowSec();

    // 更新设备状态
    if (tick.device) {
      sim.device = normalizeDevice(tick.device);
      sim.cameraStatus = sim.device.camera_status;
      sim.videoOnline = sim.device.video_available || sim.device.camera_status === "online" || sim.device.camera_status === "mock";
      if (sim.device.last_seen_at) {
        sim._lastSeen = new Date(sim.device.last_seen_at).getTime() / 1000;
      }
    }

    // 实时通道：优先使用 device-agent 直接获取的传感器数据
    if (tick.realtime) {
      const rt = tick.realtime;
      const rtSensors = rt.sensors || {};
      if (rt.risk_score != null) push("risk", t, rt.risk_score);
      if (rtSensors.pir != null) push("pir", t, rtSensors.pir);
      if (rtSensors.flame != null) push("flame", t, rtSensors.flame);
      if (rtSensors.mq2 != null) push("mq2", t, rtSensors.mq2);
      if (rtSensors.vibration_score != null) push("vib", t, rtSensors.vibration_score);
      if (rtSensors.temp_c != null) push("temp", t, rtSensors.temp_c);
      if (rtSensors.humidity_pct != null) push("hum", t, rtSensors.humidity_pct);
      if (rtSensors.light_lux != null) push("lux", t, rtSensors.light_lux);
      const rtDev = rt.device || {};
      if (rtDev.cpu_temp_c != null) push("cpu_temp", t, rtDev.cpu_temp_c);
      if (rtDev.mem_used_mb != null) push("mem_used", t, rtDev.mem_used_mb);
      if (rtDev.cpu_load_1m != null) push("load", t, rtDev.cpu_load_1m);
      if (rtDev.disk_used_pct != null) push("disk", t, rtDev.disk_used_pct);
      sim._realtimeTs = rt.ts || null;
      sim._realtimeAge = rt.fetch_latency_ms || 0;
    }

    // 更新最新事件（保留用于事件流展示）
    if (tick.latest_event) {
      const ev = {
        ts: t,
        level: tick.latest_event.risk_score >= 7 ? "danger" : tick.latest_event.risk_score >= 4 ? "warn" : "info",
        source: tick.latest_event.type || "event",
        msg: `${tick.latest_event.state || ""} · risk ${tick.latest_event.risk_score}`,
        id: tick.latest_event.id || Math.random().toString(36).slice(2),
      };
      if (!sim.events.length || sim.events[0].msg !== ev.msg) {
        sim.events.unshift(ev);
        if (sim.events.length > 250) sim.events.pop();
      }

      // 从事件中提取传感器数据（仅在 realtime 通道无数据时作为 fallback）
      if (!tick.realtime) {
        const s = tick.latest_event.sensors || {};
        if (s.pir != null) push("pir", t, s.pir);
        if (s.flame != null) push("flame", t, s.flame);
        if (s.mq2 != null) push("mq2", t, s.mq2);
        if (tick.latest_event.risk_score != null) push("risk", t, tick.latest_event.risk_score);
      }
    }

    // 更新 AI 观察
    if (tick.latest_ai_observation) {
      const obs = normalizeObservation(tick.latest_ai_observation);
      if (obs && (!sim.ai.length || sim.ai[0].id !== obs.id)) {
        sim.ai.unshift(obs);
        if (sim.ai.length > 24) sim.ai.pop();
      }
    }

    // 更新告警
    if (tick.alerts && tick.alerts.length) {
      tick.alerts.map(normalizeAlert).filter(Boolean).forEach((a) => {
        if (!sim.alerts.find((x) => x.id === a.id)) {
          sim.alerts.unshift(a);
        }
      });
      if (sim.alerts.length > 80) sim.alerts.length = 80;
    }

    emit();
  }

  function startPolling(deviceId) {
    // 历史数据补全：30 秒拉一次 telemetry series（实时数据走 SSE realtime 通道）
    sim._pollTimer = setInterval(async () => {
      const metrics = ["risk_score", "cpu_temp_c", "mpu6500.vibration_score"];
      for (const metric of metrics) {
        try {
          const res = await fetch(`/api/telemetry/series?device_id=${deviceId}&metric=${metric}&seconds=600`);
          const data = await res.json();
          if (data.ok && data.points) {
            const key = metric === "risk_score" ? "risk" : metric === "cpu_temp_c" ? "cpu_temp" : metric === "mpu6500.vibration_score" ? "vib" : metric;
            data.points.map(normalizePoint).filter(Boolean).forEach((p) => push(key, p.t, p.v));
          }
        } catch (e) { /* ignore */ }
      }

      // 视频状态从设备心跳获取，不再单独 HEAD snapshot 探测
      sim.videoError = null;

      emit();
    }, 30000);
  }

  /* ============================================================
     Mock 模式：当无真实设备时启动
     ============================================================ */

  function startMock() {
    sim.mode = "mock";
    console.log("[EdgeSim] 无真实设备，启动 mock 模式");
    pushEvent("info", "system", "未检测到真实设备，使用 mock 数据预览", nowSec());

    // 种子历史数据
    seedHistory();

    // 启动 mock tick
    sim._timer = setInterval(tick, 1000);
    sim.ready = true;
    emit();
  }

  /* ---------------- per-second mock sample ---------------- */
  function sample(t, tick, scen, seeding) {
    const off = scen === "offline";
    if (off) return; // 离线：不再产生新数据，lastSeen 冻结

    const alarm = scen === "alarm";
    const ph = tick * 1.0;

    // safety sensors
    let flame = 0, mq2 = 0, pir = 0;
    if (alarm) {
      flame = 1;
      mq2 = Math.sin(ph * 0.7) > -0.45 ? 1 : 0;
      pir = Math.random() < 0.25 ? 1 : 0;
    } else {
      if (sim._pirHold > 0) { pir = 1; sim._pirHold--; }
      else if (Math.random() < 0.012) { pir = 1; sim._pirHold = Math.floor(rnd(2, 6)); if (!seeding) pushEvent("info", "sensor", "PIR 检测到移动目标", t); }
    }

    // risk
    let risk;
    if (alarm) risk = clamp(7.4 + 0.7 * Math.sin(ph * 0.11) + g(0.25) + (mq2 ? 0.8 : 0), 6.8, 9.6);
    else risk = clamp(1.3 + 0.55 * Math.sin(ph * 0.03) + (pir ? 0.9 : 0) + g(0.18), 0.2, 3.4);
    push("risk", t, r1(risk));

    // mpu6500
    let ax = 0.01 * Math.sin(ph * 0.5) + g(0.005);
    const ay = 0.02 * Math.cos(ph * 0.3) + g(0.005);
    const az = 0.98 + 0.01 * Math.sin(ph * 0.1) + g(0.003);
    let vib = Math.abs(ax) + Math.abs(ay) + Math.abs(az - 0.98) + 0.4 + g(0.12);
    if (Math.random() < 0.015) { ax += rnd(0.3, 0.8); vib += rnd(1.5, 2.8); }
    if (alarm) vib += 0.6;
    push("ax", t, r2(ax)); push("ay", t, r2(ay)); push("az", t, r2(az));
    push("gx", t, r2(0.1 * Math.sin(ph * 0.7) + g(0.05)));
    push("gy", t, r2(0.05 * Math.cos(ph * 0.4) + g(0.03)));
    push("gz", t, r2(0.2 * Math.sin(ph * 0.2) + g(0.04)));
    push("vib", t, r2(clamp(vib, 0, 10)));

    // env
    push("temp", t, r1(25 + 2 * Math.sin(ph * 0.01) + (alarm ? 3.5 : 0) + g(0.3)));
    push("hum", t, r1(50 + 5 * Math.sin(ph * 0.008) + g(1)));
    push("lux", t, Math.round(300 + 100 * Math.sin(ph * 0.02) + (alarm ? 80 : 0) + g(20)));

    // device health
    let cpuT = sim._cpuBase + 2.5 * Math.sin(ph * 0.005) + g(0.4);
    if (alarm) cpuT += Math.min(14, (tick - sim._alarmEntered) * 0.12); // 告警时缓慢升温
    push("cpu_temp", t, r1(clamp(cpuT, 40, 88)));
    push("mem_used", t, Math.round(5450 + 260 * Math.sin(ph * 0.004) + g(60) + (scen === "degraded" ? -2900 : 0))); // 降级=模型未驻留
    push("load", t, r2(clamp(1.25 + 0.5 * Math.sin(ph * 0.02) + g(0.18) + (tick % AI_EVERY < 4 && scen !== "degraded" ? 1.6 : 0), 0.1, 8)));
    push("disk", t, r1(41.2 + tick * 0.00002));

    push("pir", t, pir);
    push("flame", t, flame);
    push("mq2", t, mq2);
    push("smoke_score", t, r2(mq2 ? rnd(3.6, 4.6) : Math.abs(g(0.2))));

    sim._lastSeen = t;

    // threshold alerts（带冷却）
    if (!seeding) {
      if (risk >= THRESHOLDS.risk_score.danger && tick - (sim._lastRiskAlert || -999) > 45) {
        sim._lastRiskAlert = tick;
        pushAlert("danger", "risk_score", `风险分数 ${r1(risk)} 超过 danger 阈值 (≥${THRESHOLDS.risk_score.danger})`, t);
      }
      const ct = latest("cpu_temp");
      if (ct >= THRESHOLDS.cpu_temp_c.warn && tick - (sim._lastCpuAlert || -999) > 60) {
        sim._lastCpuAlert = tick;
        pushAlert("warn", "cpu_temp_c", `CPU 温度 ${ct}°C 超过 warn 阈值 (≥${THRESHOLDS.cpu_temp_c.warn}°C)`, t);
      }
      if (flame && tick - (sim._lastFlameAlert || -999) > 40) {
        sim._lastFlameAlert = tick;
        pushAlert("danger", "flame", "火焰传感器触发（设备端本地告警闭环已激活）", t);
      }
    }
  }

  /* ---------------- AI observation ---------------- */
  function makeObservation(t, scen, seeding) {
    sim._aiSeq++;
    const id = "obs_" + sim._aiSeq;

    if (scen === "degraded") {
      sim._degradedCount++;
      if (sim._degradedCount === 1) {
        const obs = {
          id, timestamp: t, ok: false, status: "refused",
          error: "ConnectionRefusedError: [Errno 111] connection refused (AI service :8080)",
          model: { name: "qwen3-vl-2b", backend: "rknn-llm", mode: "worker", model_ready: false },
          window_sec: 30,
        };
        sim.ai.unshift(obs);
        pushEvent("danger", "ai", "AI 推理失败：端口拒绝连接 (:8080)，准备降级", t);
        pushAlert("warn", "ai_service", "AI 服务不可达，已切换 mock 降级模式", t);
        return;
      }
      const obs = {
        id, timestamp: t, ok: true, status: "mock", window_sec: 30,
        model: { name: "mock-detector", backend: "cpu", mode: "mock", model_ready: true },
        risk_hint: 2,
        summary: "Mock 降级：基于传感器摘要的占位观察，person 检测为模拟结果。",
        full_text: "AI 服务处于 mock 降级模式：真实 Qwen3-VL 模型不可达。当前输出由占位检测器生成，仅用于链路联通性验证，不代表真实画面理解。传感器摘要未见异常。",
        labels: ["person"],
        run_metrics: { latency_ms: Math.round(rnd(6, 14)), total_ms: Math.round(rnd(6, 14)), tokens_out: 0, mem_rss_mb: 142 },
        control_allowed: false,
      };
      sim.ai.unshift(obs);
      if (!seeding) pushEvent("warn", "ai", "AI 观察完成（mock 降级）", t);
      return;
    }

    const alarm = scen === "alarm";
    // 偶发超时（正常场景 6%）
    if (!alarm && !seeding && Math.random() < 0.06) {
      sim.ai.unshift({
        id, timestamp: t, ok: false, status: "timeout",
        error: "ReadTimeout: inference exceeded 60s budget",
        model: { name: "qwen3-vl-2b", backend: "rknn-llm", mode: "worker", model_ready: true },
        window_sec: 30,
      });
      pushEvent("warn", "ai", "AI 推理超时（>60s），本轮观察丢弃", t);
      return;
    }

    const imgenc = Math.round(rnd(2280, 2520));
    const prefill = Math.round(rnd(1450, 1900));
    const tokens = Math.round(alarm ? rnd(110, 160) : rnd(55, 120));
    const tps = rnd(10.4, 11.6);
    const decode = Math.round((tokens / tps) * 1000);
    const total = imgenc + prefill + decode + Math.round(rnd(80, 200));
    const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];

    const obs = {
      id, timestamp: t, ok: true, status: "ok", window_sec: 30,
      model: { name: "qwen3-vl-2b", backend: "rknn-llm", mode: "worker", model_ready: true },
      risk_hint: alarm ? Math.round(rnd(7.6, 9.2)) : Math.round(rnd(1, 3)),
      summary: pick(alarm ? ALARM_SUMMARIES : NORMAL_SUMMARIES),
      full_text: pick(alarm ? ALARM_FULL : NORMAL_FULL),
      labels: pick(alarm ? ALARM_LABELS : NORMAL_LABELS),
      run_metrics: {
        ttft_ms: imgenc + prefill,
        imgenc_ms: imgenc,
        llm_prefill_ms: prefill,
        decode_ms: decode,
        total_ms: total,
        tokens_out: tokens,
        tokens_per_s: r1(tps),
        mem_rss_mb: Math.round(rnd(3060, 3240)),
        npu: "RK3588 NPU ×3",
      },
      control_allowed: false,
    };
    sim.ai.unshift(obs);
    if (sim.ai.length > 24) sim.ai.pop();
    if (!seeding) {
      pushEvent(alarm ? "danger" : "info", "ai",
        alarm ? `AI 观察：risk_hint ${obs.risk_hint}/10 — ${obs.summary.slice(0, 26)}…` : `AI 观察完成 · ${total}ms · ${tokens} tokens`, t);
    }
  }

  /* ---------------- heartbeat / device ---------------- */
  function makeDevice(t, scen) {
    const off = scen === "offline";
    const deg = scen === "degraded";
    return {
      device_id: "edge-opi5-001",
      ip: "—（配置接入）",
      agent_version: "0.2.0",
      uptime_s: 86400 * 3 + 7240 + sim._tick,
      last_seen_at: sim._lastSeen,
      online: !off,
      services: {
        device_agent: off ? "unknown" : "running",
        "opi5-ai-qwen3vl": off ? "unknown" : deg ? "failed" : "running",
        "opi5-safetyd": off ? "unknown" : "running",
      },
      health: {
        cpu_temp_c: latest("cpu_temp"),
        mem_used_mb: latest("mem_used"),
        mem_total_mb: 15876,
        cpu_load_1m: latest("load"),
        disk_used_pct: latest("disk"),
      },
      ai: off
        ? { ok: false, error: "unreachable" }
        : deg
          ? { ok: false, mode: "qwen3vl", model_ready: false, error: "connection refused (:8080)" }
          : { ok: true, mode: "qwen3vl", model_ready: true, contract_version: "1.1", models: [{ name: "qwen3-vl-2b", backend: "rknn-llm", version: "local" }] },
      agent_url: null,
      agent_port: 8090,
      camera: { status: off ? "offline" : "mock", mode: off ? "offline" : "mock", available: !off, device: null, width: 1280, height: 720, fps: 12, mock: true },
      camera_status: off ? "offline" : "mock",
      video_mode: off ? "offline" : "mock",
      video_available: !off,
      backend: "ok",
      contract_version: "1.1",
    };
  }

  /* ---------------- tick loop ---------------- */
  function tick() {
    sim._tick++;
    const t = nowSec();
    sample(t, sim._tick, sim.scenario, false);
    if (sim._tick % AI_EVERY === 0 && sim.scenario !== "offline") makeObservation(t, sim.scenario, false);
    if (sim._tick % HB_EVERY === 0) sim.device = makeDevice(t, sim.scenario);
    emit();
  }
  function emit() { sim.listeners.forEach((fn) => fn(sim)); }

  /* ---------------- public API ---------------- */
  sim.subscribe = function (fn) { sim.listeners.add(fn); return () => sim.listeners.delete(fn); };
  sim.getSeries = function (key, seconds) {
    const b = sim.buf[key] || [];
    if (!seconds) return b;
    const cut = nowSec() - seconds;
    let i = 0;
    while (i < b.length && b[i].t < cut) i++;
    return b.slice(i);
  };
  sim.latest = latest;
  sim.realtimeAge = function () {
    if (!sim._realtimeTs) return Infinity;
    return nowSec() - new Date(sim._realtimeTs).getTime() / 1000;
  };

  /* ---------------- seed mock history ---------------- */
  function seedHistory() {
    const t0 = nowSec() - HIST_SEC;
    for (let i = 0; i < HIST_SEC; i++) {
      sim._tick++;
      sample(t0 + i, sim._tick, "normal", true);
      if (sim._tick % AI_EVERY === 0 && i > 60) makeObservation(t0 + i, "normal", true);
    }
    const t = nowSec();
    pushEvent("info", "device", "device-agent 0.2.0 启动，开始 5s 心跳上报", t - 700);
    pushEvent("info", "ai", "Qwen3-VL 2B 模型加载完成 (vision encoder + RKLLM)", t - 688);
    pushEvent("info", "system", "遥测批量上报通道就绪（30s/批）", t - 686);
    pushEvent("warn", "ai", "AI 推理超时（>60s），本轮观察丢弃", t - 421);
    pushEvent("info", "sensor", "PIR 检测到移动目标", t - 245);
    sim.notif.logs.push(
      { id: "ntf_9031", ts: t - 5210, alert_id: "alr_1276", channel: "email", recipient: sim.notif.settings.alert_email_to, subject: "[EdgeSafety] 告警 · cpu_temp_c · edge-opi5-001", status: "sent", error: null },
      { id: "ntf_9030", ts: t - 5980, alert_id: "alr_1275", channel: "email", recipient: sim.notif.settings.alert_email_to, subject: "[EdgeSafety] 告警 · flame · edge-opi5-001", status: "failed", error: "SMTPException: connection to host timed out (30s)" },
      { id: "ntf_9029", ts: t - 6010, alert_id: "alr_1275", channel: "email", recipient: sim.notif.settings.alert_email_to, subject: "[EdgeSafety] 告警 · risk_score · edge-opi5-001", status: "sent", error: null },
    );
    sim.notif.last_result = { status: "sent", ts: t - 5210, error: null };
    sim.notif.last_sent_at = t - 5210;
    sim.device = makeDevice(t, "normal");
  }

  /* ============================================================
     启动：先检测真实设备，再决定模式
     ============================================================ */
  (async function init() {
    const hasReal = await connectReal();
    if (!hasReal) {
      startMock();
    }
  })();

  /* ---------------- public API ---------------- */
  sim.subscribe = function (fn) { sim.listeners.add(fn); return () => sim.listeners.delete(fn); };
  sim.getSeries = function (key, seconds) {
    const b = sim.buf[key] || [];
    if (!seconds) return b;
    const cut = nowSec() - seconds;
    let i = 0;
    while (i < b.length && b[i].t < cut) i++;
    return b.slice(i);
  };
  sim.latest = latest;
  sim.setScenario = function (s) {
    if (sim.mode === "real") return; // 真实模式下不允许切换场景
    if (s === sim.scenario) return;
    const prev = sim.scenario;
    sim.scenario = s;
    const t = nowSec();
    sim._degradedCount = 0;
    if (s === "alarm") { sim._alarmEntered = sim._tick; pushEvent("danger", "system", "状态切换 NORMAL → ALARM：火焰/烟雾传感器触发", t); }
    if (s === "normal" && prev !== "normal") pushEvent("info", "system", "状态恢复 → NORMAL，传感器信号清除", t);
    if (s === "degraded") pushEvent("warn", "system", "AI 服务异常，进入降级监控模式", t);
    if (s === "offline") { pushAlert("danger", "device", "设备心跳丢失，edge-opi5-001 离线", t); }
    if (prev === "offline" && s !== "offline") pushEvent("info", "device", "设备心跳恢复，重新上线", t);
    sim.device = makeDevice(t, s);
    emit();
  };
  sim.updateNotifSettings = async function (patch) {
    if (sim.mode === "real") {
      try {
        const res = await fetch('/api/notification/settings', {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(patch),
        });
        const data = await res.json();
        if (data.ok) {
          Object.assign(sim.notif.settings, data.settings);
          pushEvent("info", "notify", "通知设置已更新 (PUT /api/notification/settings)", nowSec());
        }
      } catch (e) {
        pushEvent("warn", "notify", "通知设置更新失败: " + e.message, nowSec());
      }
    } else {
      Object.assign(sim.notif.settings, patch);
      const s = sim.notif.settings;
      s.configured = !!(s.alert_email_to && s.smtp_host && s.smtp_port && s.smtp_user);
      pushEvent("info", "notify", "通知设置已更新 (PUT /api/notification/settings)", nowSec());
    }
    emit();
  };
  sim.sendTestEmail = async function (cb) {
    if (sim.mode === "real") {
      try {
        const res = await fetch('/api/notification/test-email', { method: 'POST' });
        const data = await res.json();
        const entry = {
          id: "ntf_" + (++sim._notifSeq), ts: nowSec(), alert_id: "—", channel: "email",
          recipient: sim.notif.settings.alert_email_to || "(未配置)",
          subject: "[EdgeSafety] 测试邮件 · 配置验证",
          status: data.status || (data.ok ? "sent" : "failed"),
          error: data.error || null,
        };
        sim.notif.logs.unshift(entry);
        sim.notif.last_result = { status: entry.status, ts: entry.ts, error: entry.error, test: true };
        pushEvent(entry.status === "sent" ? "info" : "warn", "notify",
          entry.status === "sent" ? "测试邮件发送成功" : "测试邮件发送失败", entry.ts);
        emit();
        cb && cb(entry);
      } catch (e) {
        pushEvent("warn", "notify", "测试邮件发送失败: " + e.message, nowSec());
        cb && cb({ status: "failed", error: e.message });
      }
    } else {
      // mock 模式
      const s = sim.notif.settings;
      setTimeout(() => {
        const fail = !s.configured || Math.random() < 0.18;
        const t = nowSec();
        const entry = {
          id: "ntf_" + (++sim._notifSeq), ts: t, alert_id: "—", channel: "email",
          recipient: s.alert_email_to || "(未配置)", subject: "[EdgeSafety] 测试邮件 · 配置验证",
          status: fail ? "failed" : "sent",
          error: fail ? (s.configured ? "SMTPAuthenticationError: 535 invalid credentials" : "配置不完整：缺少必填字段") : null,
        };
        sim.notif.logs.unshift(entry);
        sim.notif.last_result = { status: entry.status, ts: t, error: entry.error, test: true };
        pushEvent(fail ? "warn" : "info", "notify", fail ? "测试邮件发送失败" : "测试邮件发送成功 → " + s.alert_email_to, t);
        emit();
        cb && cb(entry);
      }, 1400 + Math.random() * 900);
    }
  };
  sim.setSpeed = function (x) {
    if (sim.mode === "real") return; // 真实模式下不控制速度
    sim.speed = x;
    clearInterval(sim._timer);
    sim._timer = setInterval(tick, 1000 / x);
  };

  window.EdgeSim = sim;
  return sim;
})();

export default window.EdgeSim;
