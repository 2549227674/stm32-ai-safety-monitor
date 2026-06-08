# 2026-06-08 YOLO RKNN Backend Minimal Test

## Target
OPi5 YOLO RKNN backend integration - smoke test for API contract.

## Modified Files
- `edge/opi5-ai/service/yolo_rknn_backend.py` (new)
- `edge/opi5-ai/service/app.py` (modified: added yolo_rknn backend + /api/track/frame + /api/track/stream)

## What Was Implemented
1. `yolo_rknn_backend.py`: RKNN inference backend with mock fallback when no model available
   - `detect(image_bytes)` -> objects with bbox, best_fire, tracking_hint
   - Real RKNN path: loads model from `YOLO_RKNN_MODEL` env, runs YOLOv8n inference
   - Mock path: returns synthetic fire bbox with jitter (model_ready=false)
2. `app.py`: Three backends (`mock`, `qwen3vl`, `yolo_rknn`)
   - `POST /api/track/frame`: low-latency tracking endpoint with bbox + tracking_hint
   - `GET /api/track/stream`: MJPEG annotated stream (bbox overlay via PIL)
   - `POST /api/infer/vision`: existing endpoint, now supports yolo_rknn backend
   - `GET /health`: reports mode and model_ready for yolo_rknn

## Verification
- [x] Python syntax: `py_compile` passes for both files
- [ ] `AI_BACKEND=mock` regression (curl /health, /api/infer/vision)
- [ ] `AI_BACKEND=yolo_rknn` mock smoke test (service starts, returns objects)
- [ ] /api/track/frame returns tracking_hint with offset_px
- [ ] control_allowed=false in all responses

## Status
Code implemented, syntax verified. **Real RKNN model not yet integrated** (needs model conversion per design §2.2). Mock fallback returns `model_ready=false`.

## Real Hardware/Model
- Real RKNN: No (model not converted yet)
- Mock: Yes (synthetic bbox for contract validation)
