#!/usr/bin/env bash
# diagnose_video_latency.sh — Quick latency diagnostics for MJPEG video chain.
# Usage: bash scripts/diagnose_video_latency.sh [device_id]
set -euo pipefail

DEVICE_ID="${1:-edge-opi5-001}"
AGENT_URL="${OPI5_DEVICE_AGENT_URL:-http://127.0.0.1:8090}"
BACKEND_URL="http://127.0.0.1:5000"

echo "=== Video Latency Diagnostics ==="
echo "device_id: $DEVICE_ID"
echo "agent:     $AGENT_URL"
echo "backend:   $BACKEND_URL"
echo ""

# 1. v4l2 camera formats
echo "--- v4l2 camera info ---"
for dev in /dev/video*; do
    [ -e "$dev" ] || continue
    echo "  $dev:"
    v4l2-ctl --list-formats-ext -d "$dev" 2>/dev/null | head -20 || echo "    (v4l2-ctl not available)"
    echo ""
done

# 2. Agent status / camera diagnostics
echo "--- agent /api/status camera ---"
curl -sS "$AGENT_URL/api/status" 2>/dev/null | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    cam = d.get('camera', {})
    print(f\"  status:         {cam.get('status')}\")
    print(f\"  mode:           {cam.get('mode')}\")
    print(f\"  device:         {cam.get('device')}\")
    print(f\"  actual_fourcc:  {cam.get('actual_fourcc')}\")
    print(f\"  actual res:     {cam.get('width')}x{cam.get('height')}\")
    print(f\"  actual_fps:     {cam.get('fps')}\")
    print(f\"  measured_fps:   {cam.get('measured_fps')}\")
    print(f\"  frames_captured:{cam.get('frames_captured')}\")
    print(f\"  last_error:     {cam.get('last_error')}\")
except Exception as e:
    print(f'  parse error: {e}')
" || echo "  (agent unreachable)"

echo ""

# 3. Snapshot latency
echo "--- snapshot latency ---"
curl -o /tmp/_diag_snap.jpg -w "  agent:   time=%{time_total}s size=%{size_download} bytes\n" \
    -sS "$AGENT_URL/api/video/snapshot.jpg" 2>/dev/null || echo "  agent snapshot failed"
file /tmp/_diag_snap.jpg 2>/dev/null | sed 's/^/  /' || true

curl -o /tmp/_diag_snap_be.jpg -w "  backend: time=%{time_total}s size=%{size_download} bytes\n" \
    -sS "$BACKEND_URL/api/video/snapshot.jpg?device_id=$DEVICE_ID" 2>/dev/null || echo "  backend snapshot failed"
file /tmp/_diag_snap_be.jpg 2>/dev/null | sed 's/^/  /' || true
echo ""

# 4. Agent stream 5-second frame count
echo "--- agent stream 5s sample ---"
timeout 5 curl -sS "$AGENT_URL/api/video/stream" -o /tmp/_diag_agent.mjpg 2>/dev/null || true
AGENT_BYTES=$(stat -c%s /tmp/_diag_agent.mjpg 2>/dev/null || echo 0)
AGENT_FRAMES=$(grep -ac -- "Content-Type: image/jpeg" /tmp/_diag_agent.mjpg 2>/dev/null || echo 0)
echo "  bytes: $AGENT_BYTES"
echo "  frames (boundary count): $AGENT_FRAMES"
if [ "$AGENT_FRAMES" -gt 0 ] 2>/dev/null; then
    echo "  estimated fps: $(echo "scale=1; $AGENT_FRAMES / 5" | bc)"
fi
echo ""

# 5. Backend proxy stream 5-second frame count
echo "--- backend proxy stream 5s sample ---"
timeout 5 curl -sS "$BACKEND_URL/api/video/stream?device_id=$DEVICE_ID" -o /tmp/_diag_backend.mjpg 2>/dev/null || true
BACKEND_BYTES=$(stat -c%s /tmp/_diag_backend.mjpg 2>/dev/null || echo 0)
BACKEND_FRAMES=$(grep -ac -- "Content-Type: image/jpeg" /tmp/_diag_backend.mjpg 2>/dev/null || echo 0)
echo "  bytes: $BACKEND_BYTES"
echo "  frames (boundary count): $BACKEND_FRAMES"
if [ "$BACKEND_FRAMES" -gt 0 ] 2>/dev/null; then
    echo "  estimated fps: $(echo "scale=1; $BACKEND_FRAMES / 5" | bc)"
fi
echo ""

# 6. Analysis
echo "--- analysis ---"
if [ "$AGENT_FRAMES" -gt 0 ] 2>/dev/null && [ "$BACKEND_FRAMES" -gt 0 ] 2>/dev/null; then
    if [ "$AGENT_FRAMES" -gt $((BACKEND_FRAMES * 2)) ]; then
        echo "  PROBLEM: agent fps >> backend fps => Flask proxy is buffering/dropping frames"
        echo "  FIX: check backend iter_content chunk_size and response headers"
    elif [ "$AGENT_FRAMES" -lt 5 ]; then
        echo "  PROBLEM: agent stream fps is very low (<1fps) => camera capture issue"
        echo "  FIX: check FOURCC, camera format, cap.read() blocking, CONVERT_RGB setting"
    else
        echo "  OK: agent and backend fps are comparable"
    fi
else
    echo "  Could not compare — one or both streams returned 0 frames"
fi

# Cleanup
rm -f /tmp/_diag_snap.jpg /tmp/_diag_snap_be.jpg /tmp/_diag_agent.mjpg /tmp/_diag_backend.mjpg
