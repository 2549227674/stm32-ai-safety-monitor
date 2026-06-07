#!/bin/sh
# imx_vertical_slice_once.sh — Task07-A: one-shot end-edge vertical slice
# Runs on i.MX6ULL: capture → POST OPi5 AI → assemble event → POST Flask
set -e

: "${OPI5_URL:?Set OPI5_URL e.g. http://10.0.1.120:8080}"
: "${BACKEND_URL:?Set BACKEND_URL e.g. http://192.168.137.1:5000}"
: "${DEVICE_ID:=labbox_001}"
: "${FRAME_ID:=7001}"
: "${CAPTURE_DEVICE:=/dev/video1}"
: "${CAPTURE_DIR:=/opt/edge-ai-safety-monitor/captures}"
: "${SPOOL_DIR:=/opt/edge-ai-safety-monitor/spool}"

CAPTURE_FILE="$CAPTURE_DIR/task07_latest.jpg"
AI_RESPONSE_FILE="$SPOOL_DIR/task07_ai_response.json"
EVENT_JSON_FILE="$SPOOL_DIR/task07_event.json"
BACKEND_RESPONSE_FILE="$SPOOL_DIR/task07_backend_response.json"

mkdir -p "$CAPTURE_DIR" "$SPOOL_DIR"

now_ms() {
  # millisecond timestamp; fallback to seconds if %3N not supported
  local t
  t=$(date +%s%3N 2>/dev/null) || t=$(date +%s)000
  echo "$t"
}

TS_START=$(now_ms)

# --- Step 1: Capture ---
echo "[1/5] Capturing from $CAPTURE_DEVICE ..."
T_CAP_START=$(now_ms)

if v4l2-ctl -d "$CAPTURE_DEVICE" \
    --set-fmt-video=width=640,height=480,pixelformat=MJPG \
    --stream-mmap=3 --stream-count=1 \
    --stream-to="$CAPTURE_FILE" 2>/dev/null; then
  echo "  capture OK: $CAPTURE_FILE"
  ls -lh "$CAPTURE_FILE"
else
  echo "  capture FAILED, falling back to task04 image"
  FALLBACK="/opt/edge-ai-safety-monitor/captures/task04_latest.jpg"
  if [ -f "$FALLBACK" ]; then
    cp "$FALLBACK" "$CAPTURE_FILE"
    echo "  fallback OK: $CAPTURE_FILE"
  else
    echo "  ERROR: no fallback image available"
    exit 1
  fi
fi

T_CAP_END=$(now_ms)
CAPTURE_MS=$((T_CAP_END - T_CAP_START))

# --- Step 2: Build metadata ---
echo "[2/5] Building metadata ..."
TS_ISO=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

cat > "$SPOOL_DIR/task07_metadata.json" <<METAEOF
{
  "contract_version": "1.0",
  "device_id": "$DEVICE_ID",
  "frame_id": $FRAME_ID,
  "timestamp": "$TS_ISO",
  "source": "imx6ull_vertical_slice",
  "image_format": "jpeg",
  "resolution": {"width": 640, "height": 480},
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "state_before_ai": "VERIFY",
  "risk_score_before_ai": 3,
  "pan_tilt": {"pan_deg": 90, "tilt_deg": 90}
}
METAEOF

# --- Step 3: POST to OPi5 ---
echo "[3/5] POSTing to OPi5 $OPI5_URL/api/infer/vision ..."
T_AI_START=$(now_ms)

AI_HTTP_CODE=$(curl -sS -X POST \
  -F "image=@$CAPTURE_FILE" \
  -F "metadata=$(cat $SPOOL_DIR/task07_metadata.json)" \
  "$OPI5_URL/api/infer/vision" \
  -o "$AI_RESPONSE_FILE" \
  -w "%{http_code}" --connect-timeout 5 --max-time 10) || AI_HTTP_CODE="000"

T_AI_END=$(now_ms)
AI_MS=$((T_AI_END - T_AI_START))

echo "  OPi5 HTTP: $AI_HTTP_CODE"
echo "  AI response:"
cat "$AI_RESPONSE_FILE" 2>/dev/null || echo "  (no response file)"
echo ""

# Check control_allowed
if grep -q '"control_allowed"[[:space:]]*:[[:space:]]*false' "$AI_RESPONSE_FILE" 2>/dev/null; then
  echo "  control_allowed=false: OK"
  CONTROL_OK="true"
else
  echo "  WARNING: control_allowed is not false or missing"
  CONTROL_OK="false"
fi

# Extract risk_hint from AI response (grep fallback since no jq)
RISK_HINT=2
RH_RAW=$(grep -o '"risk_hint"[[:space:]]*:[[:space:]]*[0-9]*' "$AI_RESPONSE_FILE" 2>/dev/null | grep -o '[0-9]*$') && RISK_HINT="$RH_RAW" || true
echo "  risk_hint=$RISK_HINT"

# --- Step 4: Assemble event JSON ---
echo "[4/5] Assembling event JSON ..."

# Simplified risk_score fusion: local_base(3) + risk_hint
RISK_SCORE=$((3 + RISK_HINT))
if [ "$RISK_SCORE" -gt 10 ]; then RISK_SCORE=10; fi

# Read AI response as single line for embedding
AI_RESULT_LINE=$(cat "$AI_RESPONSE_FILE" | tr '\n' ' ' | sed 's/  */ /g')

T_POST_START=$(now_ms)

cat > "$EVENT_JSON_FILE" <<EVENTEOF
{
  "type": "event",
  "contract_version": "1.0",
  "device_id": "$DEVICE_ID",
  "seq": $FRAME_ID,
  "state": "VERIFY",
  "risk_score": $RISK_SCORE,
  "need_snap": true,
  "sensors": {"door": 0, "pir": 1, "flame": 0, "mq2": 0},
  "actuators": {"relay": 0, "fan": 0, "pump": 0, "buzzer": 0, "rgb_r": 0, "rgb_g": 0, "rgb_b": 1},
  "vision": {
    "frame_id": $FRAME_ID,
    "image_url": null,
    "pan_tilt": {"pan_deg": 90, "tilt_deg": 90}
  },
  "ai_result": $AI_RESULT_LINE,
  "latency_ms": {"capture": $CAPTURE_MS, "ai": $AI_MS, "post": 0}
}
EVENTEOF

echo "  event JSON:"
cat "$EVENT_JSON_FILE"
echo ""

# --- Step 5: POST to Flask ---
echo "[5/5] POSTing to Flask $BACKEND_URL/api/events ..."

BACKEND_HTTP_CODE=$(curl -sS -X POST "$BACKEND_URL/api/events" \
  -H "Content-Type: application/json" \
  -d @"$EVENT_JSON_FILE" \
  -o "$BACKEND_RESPONSE_FILE" \
  -w "%{http_code}" --connect-timeout 5 --max-time 10) || BACKEND_HTTP_CODE="000"

T_POST_END=$(now_ms)
POST_MS=$((T_POST_END - T_POST_START))
TOTAL_MS=$((T_POST_END - TS_START))

# Update latency_ms in event JSON with actual post time
# (not re-saving to keep it simple; recorded below)

echo "  Flask HTTP: $BACKEND_HTTP_CODE"
echo "  Backend response:"
cat "$BACKEND_RESPONSE_FILE" 2>/dev/null || echo "  (no response file)"
echo ""

# --- Summary ---
echo "========================================"
echo "Task07-A Vertical Slice Summary"
echo "========================================"
echo "capture_ms : $CAPTURE_MS"
echo "ai_ms      : $AI_MS"
echo "post_ms    : $POST_MS"
echo "total_ms   : $TOTAL_MS"
echo "OPi5 HTTP  : $AI_HTTP_CODE"
echo "Flask HTTP : $BACKEND_HTTP_CODE"
echo "control_allowed_ok: $CONTROL_OK"
echo "risk_hint  : $RISK_HINT"
echo "risk_score : $RISK_SCORE"
echo "========================================"
