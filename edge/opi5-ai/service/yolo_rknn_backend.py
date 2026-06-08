"""YOLO RKNN backend for fire/smoke object detection.

Provides detect(image_bytes) -> dict with objects[].bbox and tracking_hint.
When no real RKNN model is available, returns mock data with model_ready=false.

Environment variables:
  YOLO_RKNN_MODEL   - path to .rknn model file
  YOLO_LABELS        - comma-separated label file (one per line)
  YOLO_INPUT_SIZE    - model input size (default 640)
  YOLO_SCORE_THR     - confidence threshold (default 0.5)
"""

import json
import os
import random
import struct
import time

DEFAULT_INPUT_SIZE = 640
DEFAULT_SCORE_THR = 0.5
DEFAULT_LABELS = ["fire", "smoke"]

_rknn_model = None
_labels = None
_model_loaded = False
_model_attempted = False


def _get_labels():
    global _labels
    if _labels is not None:
        return _labels
    labels_path = os.environ.get("YOLO_LABELS", "")
    if labels_path and os.path.isfile(labels_path):
        with open(labels_path, "r") as f:
            _labels = [line.strip() for line in f if line.strip()]
    else:
        _labels = list(DEFAULT_LABELS)
    return _labels


def _try_load_rknn():
    global _rknn_model, _model_loaded, _model_attempted
    if _model_attempted:
        return _model_loaded
    _model_attempted = True

    model_path = os.environ.get("YOLO_RKNN_MODEL", "")
    if not model_path or not os.path.isfile(model_path):
        return False

    try:
        from rknnlite.api import RKNNLite
        rknn = RKNNLite()
        ret = rknn.load_rknn(model_path)
        if ret != 0:
            return False
        ret = rknn.init_runtime()
        if ret != 0:
            rknn.release()
            return False
        _rknn_model = rknn
        _model_loaded = True
        return True
    except Exception:
        return False


def _postprocess_yolo(output_data, input_size, score_thr, labels):
    """Post-process YOLOv8 RKNN output to extract bounding boxes.

    Simplified: assumes single output tensor [1, num_classes+4, num_boxes].
    """
    objects = []
    if not output_data or len(output_data) == 0:
        return objects

    try:
        data = output_data[0]
        if len(data.shape) == 3:
            data = data[0]

        num_features = data.shape[0]
        num_boxes = data.shape[1] if len(data.shape) > 1 else 0
        num_classes = num_features - 4

        if num_classes <= 0 or num_boxes <= 0:
            return objects

        for i in range(num_boxes):
            class_scores = data[:num_classes, i]
            best_class_idx = int(class_scores.argmax())
            score = float(class_scores[best_class_idx])

            if score < score_thr:
                continue

            cx, cy, w, h = float(data[num_classes, i]), float(data[num_classes+1, i]), \
                           float(data[num_classes+2, i]), float(data[num_classes+3, i])

            x1 = max(0, int(cx - w / 2))
            y1 = max(0, int(cy - h / 2))
            x2 = min(input_size, int(cx + w / 2))
            y2 = min(input_size, int(cy + h / 2))

            label = labels[best_class_idx] if best_class_idx < len(labels) else f"class_{best_class_idx}"
            objects.append({
                "label": label,
                "score": round(score, 3),
                "bbox": [x1, y1, x2, y2],
            })
    except Exception:
        pass

    return objects


def detect(image_bytes, metadata=None):
    """Run YOLO detection on image bytes.

    Returns dict with:
      ok, backend, model_ready, objects[], best_fire, latency_ms
    """
    t0 = time.time()
    labels = _get_labels()
    input_size = int(os.environ.get("YOLO_INPUT_SIZE", DEFAULT_INPUT_SIZE))
    score_thr = float(os.environ.get("YOLO_SCORE_THR", DEFAULT_SCORE_THR))

    if _try_load_rknn():
        return _detect_real(image_bytes, labels, input_size, score_thr, t0)
    else:
        return _detect_mock(image_bytes, labels, input_size, score_thr, t0)


def _detect_real(image_bytes, labels, input_size, score_thr, t0):
    """Real RKNN inference."""
    try:
        import cv2
        import numpy as np

        nparr = np.frombuffer(image_bytes, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        if img is None:
            return {"ok": False, "backend": "yolo_rknn", "model_ready": True,
                    "error": "image decode failed", "objects": [], "best_fire": None,
                    "latency_ms": int((time.time() - t0) * 1000)}

        img_resized = cv2.resize(img, (input_size, input_size))
        img_rgb = cv2.cvtColor(img_resized, cv2.COLOR_BGR2RGB)

        outputs = _rknn_model.inference(inputs=[img_rgb])
        objects = _postprocess_yolo(outputs, input_size, score_thr, labels)

        latency_ms = int((time.time() - t0) * 1000)

        best_fire = None
        for obj in objects:
            if obj["label"] in ("fire", "smoke"):
                if best_fire is None or obj["score"] > best_fire["score"]:
                    best_fire = obj

        return {
            "ok": True,
            "backend": "yolo_rknn",
            "model_ready": True,
            "objects": objects,
            "best_fire": best_fire,
            "latency_ms": latency_ms,
        }
    except Exception as e:
        return {"ok": False, "backend": "yolo_rknn", "model_ready": True,
                "error": str(e), "objects": [], "best_fire": None,
                "latency_ms": int((time.time() - t0) * 1000)}


def _detect_mock(image_bytes, labels, input_size, score_thr, t0):
    """Mock detection when no RKNN model is available."""
    latency_ms = int((time.time() - t0) * 1000)

    cx = random.randint(200, 440)
    cy = random.randint(150, 350)
    bw = random.randint(80, 160)
    bh = random.randint(80, 160)
    x1 = max(0, cx - bw // 2)
    y1 = max(0, cy - bh // 2)
    x2 = min(input_size, cx + bw // 2)
    y2 = min(input_size, cy + bh // 2)

    objects = [{
        "label": "fire",
        "score": round(random.uniform(0.70, 0.92), 3),
        "bbox": [x1, y1, x2, y2],
    }]
    best_fire = objects[0]

    return {
        "ok": True,
        "backend": "yolo_rknn_mock",
        "model_ready": False,
        "objects": objects,
        "best_fire": best_fire,
        "latency_ms": latency_ms,
    }


def model_status():
    """Return current model status for /health endpoint."""
    if _model_loaded:
        return {"model_ready": True, "backend": "rknn"}
    model_path = os.environ.get("YOLO_RKNN_MODEL", "")
    if model_path:
        return {"model_ready": False, "backend": "rknn", "note": f"model not loaded: {model_path}"}
    return {"model_ready": False, "backend": "mock", "note": "no YOLO_RKNN_MODEL set"}
