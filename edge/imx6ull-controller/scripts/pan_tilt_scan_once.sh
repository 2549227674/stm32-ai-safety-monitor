#!/bin/sh
# pan_tilt_scan_once.sh — Task08-A: time-sliced pan-tilt scan with capture + mock AI
# Runs on i.MX6ULL: move gimbal to 3 pan angles → capture each → POST OPi5 → select best → POST Flask
set -e

: "${OPI5_URL:?Set OPI5_URL}"
: "${BACKEND_URL:?Set BACKEND_URL}"
: "${DEVICE_ID:=labbox_001}"
: "${SCAN_SEQ_BASE:=8001}"
: "${CAPTURE_DEVICE:=/dev/video1}"
: "${CAPTURE_DIR:=/opt/edge-ai-safety-monitor/captures/task08}"
: "${SPOOL_DIR:=/opt/edge-ai-safety-monitor/spool/task08}"
: "${POSE_BIN:=/opt/edge-ai-safety-monitor/pca9685_set_pose}"
: "${SETTLE_MS:=800}"

mkdir -p "$CAPTURE_DIR" "$SPOOL_DIR"

now_ms() {
  date +%s%3N 2>/dev/null || echo $(($(date +%s) * 1000))
}

TS_START=$(now_ms)
CAPTURE_TOTAL=0
AI_TOTAL=0

# Scan points: pan_deg pan_us tilt_deg tilt_us
PAN_DEGS="60 90 120"
PAN_USS="1330 1500 1670"
TILT_DEG=90
TILT_US=1500

best_risk_hint=-1
best_score=0
best_idx=0
best_pan_deg=90
best_pan_us=1500
best_frame_id=$SCAN_SEQ_BASE
idx=0

echo "========================================"
echo "Task08-A pan-tilt scan"
echo "Points: pan 60/90/120 deg, tilt 90 deg fixed"
echo "========================================"

# --- Scan loop ---
set -- $PAN_USS
for pan_deg in $PAN_DEGS; do
  idx=$((idx + 1))
  pan_us=$1; shift
  frame_id=$((SCAN_SEQ_BASE + idx - 1))

  echo ""
  echo "--- Point $idx: pan=${pan_deg}deg (${pan_us}us) ---"

  # 1. Move gimbal
  echo "[move] pan_us=$pan_us tilt_us=$TILT_US"
  T_MOVE_START=$(now_ms)
  $POSE_BIN --pan-us "$pan_us" --tilt-us "$TILT_US" --hold-ms "$SETTLE_MS"
  T_MOVE_END=$(now_ms)
  move_ms=$((T_MOVE_END - T_MOVE_START))

  # 2. Capture
  capture_file="$CAPTURE_DIR/task08_frame_${frame_id}.jpg"
  echo "[capture] $capture_file"
  T_CAP_START=$(now_ms)
  v4l2-ctl -d "$CAPTURE_DEVICE" \
    --set-fmt-video=width=640,height=480,pixelformat=MJPG \
    --stream-mmap=3 --stream-count=1 \
    --stream-to="$capture_file" 2>/dev/null || {
    echo "  capture failed, using task04 fallback"
    cp /opt/edge-ai-safety-monitor/captures/task04_latest.jpg "$capture_file"
  }
  T_CAP_END=$(now_ms)
  cap_ms=$((T_CAP_END - T_CAP_START))
  CAPTURE_TOTAL=$((CAPTURE_TOTAL + cap_ms))
  ls -lh "$capture_file"

  # 3. Build metadata
  TS_ISO=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
  metadata_file="$SPOOL_DIR/task08_meta_${frame_id}.json"
  cat > "$metadata_file" <<METAEOF
{
  "contract_version": "1.0",
  "device_id": "$DEVICE_ID",
  "frame_id": $frame_id,
  "timestamp": "$TS_ISO",
  "source": "imx6ull_pan_tilt_scan",
  "image_format": "jpeg",
  "resolution": {"width": 640, "height": 480},
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "state_before_ai": "VERIFY",
  "risk_score_before_ai": 3,
  "pan_tilt": {"pan_deg": $pan_deg, "tilt_deg": $TILT_DEG}
}
METAEOF

  # 4. POST to OPi5
  ai_file="$SPOOL_DIR/task08_ai_${frame_id}.json"
  echo "[ai] POST to OPi5"
  T_AI_START=$(now_ms)
  ai_code=$(curl -sS -X POST \
    -F "image=@$capture_file" \
    -F "metadata=$(cat $metadata_file)" \
    "$OPI5_URL/api/infer/vision" \
    -o "$ai_file" \
    -w "%{http_code}" --connect-timeout 5 --max-time 10) || ai_code="000"
  T_AI_END=$(now_ms)
  ai_ms=$((T_AI_END - T_AI_START))
  AI_TOTAL=$((AI_TOTAL + ai_ms))
  echo "  HTTP $ai_code, ${ai_ms}ms"

  # 5. Extract risk_hint
  risk_hint=0
  rh=$(grep -o '"risk_hint"[[:space:]]*:[[:space:]]*[0-9]*' "$ai_file" 2>/dev/null | grep -o '[0-9]*$') && risk_hint="$rh" || true
  echo "  risk_hint=$risk_hint"

  # 6. Track best (highest risk_hint, then center tie-break)
  if [ "$risk_hint" -gt "$best_risk_hint" ] 2>/dev/null; then
    best_risk_hint=$risk_hint
    best_idx=$idx
    best_pan_deg=$pan_deg
    best_pan_us=$pan_us
    best_frame_id=$frame_id
    best_score=0
    # Try to extract first object score
    sc=$(grep -o '"score"[[:space:]]*:[[:space:]]*[0-9.]*' "$ai_file" 2>/dev/null | head -1 | grep -o '[0-9.]*$') && best_score="$sc" || true
  elif [ "$risk_hint" -eq "$best_risk_hint" ] 2>/dev/null; then
    # Tie: prefer center (pan=90)
    if [ "$pan_deg" -eq 90 ]; then
      best_idx=$idx
      best_pan_deg=$pan_deg
      best_pan_us=$pan_us
      best_frame_id=$frame_id
    fi
  fi

  echo "  best so far: pan=${best_pan_deg}deg (point $best_idx, risk_hint=$best_risk_hint)"
done

echo ""
echo "========================================"
echo "Scan complete. Best: pan=${best_pan_deg}deg (point $best_idx)"
echo "========================================"

# --- Hold best angle ---
echo "[hold] moving to best angle: pan_us=$best_pan_us tilt_us=$TILT_US"
$POSE_BIN --pan-us "$best_pan_us" --tilt-us "$TILT_US" --hold-ms 1000

# --- Assemble event JSON ---
echo "[event] assembling final event JSON"
T_POST_START=$(now_ms)

# Read best AI response as single line
AI_RESULT_LINE=$(cat "$SPOOL_DIR/task08_ai_${best_frame_id}.json" | tr '\n' ' ' | sed 's/  */ /g')

# Build scan array JSON
SCAN_JSON="["
first=1
for pan_deg in $PAN_DEGS; do
  fid=$((SCAN_SEQ_BASE + first - 1))
  rh=0
  r=$(grep -o '"risk_hint"[[:space:]]*:[[:space:]]*[0-9]*' "$SPOOL_DIR/task08_ai_${fid}.json" 2>/dev/null | grep -o '[0-9]*$') && rh="$r" || true
  if [ "$first" -ne 1 ]; then SCAN_JSON="$SCAN_JSON,"; fi
  SCAN_JSON="$SCAN_JSON{\"frame_id\":$fid,\"pan_deg\":$pan_deg,\"tilt_deg\":$TILT_DEG,\"risk_hint\":$rh}"
  first=$((first + 1))
done
SCAN_JSON="$SCAN_JSON]"

RISK_SCORE=$((3 + best_risk_hint))
[ "$RISK_SCORE" -gt 10 ] && RISK_SCORE=10

event_file="$SPOOL_DIR/task08_event.json"
cat > "$event_file" <<EVENTEOF
{
  "type": "event",
  "contract_version": "1.0",
  "device_id": "$DEVICE_ID",
  "seq": $SCAN_SEQ_BASE,
  "state": "VERIFY",
  "risk_score": $RISK_SCORE,
  "need_snap": true,
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 1},
  "vision": {
    "frame_id": $best_frame_id,
    "image_url": null,
    "pan_tilt": {"pan_deg": $best_pan_deg, "tilt_deg": $TILT_DEG},
    "scan": $SCAN_JSON,
    "selected": {"frame_id": $best_frame_id, "pan_deg": $best_pan_deg, "tilt_deg": $TILT_DEG, "reason": "highest_risk_hint"}
  },
  "ai_result": $AI_RESULT_LINE,
  "latency_ms": {"scan_total": 0, "capture_total": $CAPTURE_TOTAL, "ai_total": $AI_TOTAL, "post": 0}
}
EVENTEOF

# --- POST to Flask ---
echo "[post] POSTing to Flask"
BACKEND_CODE=$(curl -sS -X POST "$BACKEND_URL/api/events" \
  -H "Content-Type: application/json" \
  -d @"$event_file" \
  -o "$SPOOL_DIR/task08_backend_response.json" \
  -w "%{http_code}" --connect-timeout 5 --max-time 10) || BACKEND_CODE="000"
T_POST_END=$(now_ms)
POST_MS=$((T_POST_END - T_POST_START))
TOTAL_MS=$((T_POST_END - TS_START))

SCAN_TOTAL=$((TOTAL_MS - POST_MS))

# Update latency in event (best effort)
echo ""
echo "========================================"
echo "Task08-A Summary"
echo "========================================"
echo "points scanned  : 3"
echo "selected        : pan=${best_pan_deg}deg (point $best_idx)"
echo "risk_hint best  : $best_risk_hint"
echo "Flask HTTP      : $BACKEND_CODE"
echo "capture_total_ms: $CAPTURE_TOTAL"
echo "ai_total_ms     : $AI_TOTAL"
echo "post_ms         : $POST_MS"
echo "scan_total_ms   : $SCAN_TOTAL"
echo "========================================"

# Print backend response
echo "Backend response:"
cat "$SPOOL_DIR/task08_backend_response.json" 2>/dev/null || echo "(none)"
echo ""
