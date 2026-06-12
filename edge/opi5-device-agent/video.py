"""Video capture - real camera or mock JPEG stream."""

import os
import time
import threading
from datetime import datetime


class VideoCapture:
    def __init__(self):
        self._mock_env = os.environ.get("VIDEO_MOCK", "0") == "1"
        self.mock = self._mock_env
        self.device = os.environ.get("CAMERA_DEVICE", "/dev/video0")
        self.width = int(os.environ.get("VIDEO_WIDTH", "640"))
        self.height = int(os.environ.get("VIDEO_HEIGHT", "480"))
        self.fps = int(os.environ.get("VIDEO_FPS", "20"))
        self.quality = int(os.environ.get("VIDEO_JPEG_QUALITY", "60"))
        self._frame = None
        self._frame_version = 0
        self._lock = threading.Lock()
        self._cond = threading.Condition(self._lock)
        self._available = False
        self._real_opened = False
        self._cap = None
        self._cv2 = None

        # Live diagnostics
        self.frames_captured = 0
        self.last_frame_ts = 0.0
        self.measured_fps = 0.0
        self.last_error = ""
        self.actual_width = 0
        self.actual_height = 0
        self.actual_fps = 0.0
        self.actual_fourcc = ""
        self._fps_window = []  # list of timestamps for rolling FPS calc

        if not self.mock:
            self._try_open_camera()
        if not self._available:
            self.mock = True
            print("[video] using mock video stream")

    def _try_open_camera(self):
        try:
            import cv2
            self._cv2 = cv2
            device = self._resolve_device()
            if not device:
                return
            self.device = device

            self._cap = cv2.VideoCapture(device)
            if self._cap.isOpened():
                # Force MJPG fourcc first, then geometry/fps, then buffer=1
                fourcc = cv2.VideoWriter_fourcc(*"MJPG")
                self._cap.set(cv2.CAP_PROP_FOURCC, fourcc)
                self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
                self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
                self._cap.set(cv2.CAP_PROP_FPS, self.fps)
                self._cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
                # Disable RGB conversion so cap.read() returns raw MJPEG bytes
                self._cap.set(cv2.CAP_PROP_CONVERT_RGB, 0)

                # Read back actual negotiated values
                self.actual_width = int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                self.actual_height = int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                self.actual_fps = self._cap.get(cv2.CAP_PROP_FPS)
                raw_fourcc = int(self._cap.get(cv2.CAP_PROP_FOURCC))
                self.actual_fourcc = "".join([chr((raw_fourcc >> 8 * i) & 0xFF) for i in range(4)])

                self._available = True
                self._real_opened = True
                threading.Thread(target=self._capture_loop, daemon=True).start()
                print(f"[video] opened {self.device} fourcc={self.actual_fourcc} "
                      f"{self.actual_width}x{self.actual_height}@{self.actual_fps:.1f}fps "
                      f"(requested {self.width}x{self.height}@{self.fps}fps)")
            else:
                self.last_error = "failed to open device"
                print(f"[video] failed to open {self.device}")
        except ImportError:
            self.last_error = "opencv not available"
            print("[video] opencv not available")
        except Exception as e:
            self.last_error = str(e)
            print(f"[video] camera error: {e}")

    def _resolve_device(self):
        """Auto-detect /dev/videoX if configured device doesn't exist."""
        if os.path.exists(self.device):
            return self.device
        # Try common alternatives
        for candidate in ("/dev/video0", "/dev/video1", "/dev/video2"):
            if os.path.exists(candidate):
                print(f"[video] {self.device} not found, using {candidate}")
                return candidate
        self.last_error = f"no video device found (tried {self.device} and /dev/video0-2)"
        print(f"[video] {self.last_error}")
        return None

    def _capture_loop(self):
        while self._cap and self._cap.isOpened():
            ret, frame = self._cap.read()
            if not ret or frame is None:
                time.sleep(0.03)
                continue

            jpeg_bytes = self._frame_to_jpeg(frame)
            if jpeg_bytes is None:
                time.sleep(0.03)
                continue

            now = time.monotonic()
            with self._cond:
                self._frame = jpeg_bytes
                self._frame_version += 1
                self._cond.notify_all()
            self.frames_captured += 1
            self.last_frame_ts = now

            # Rolling FPS over last 5 seconds
            self._fps_window.append(now)
            cutoff = now - 5.0
            self._fps_window = [t for t in self._fps_window if t > cutoff]
            if len(self._fps_window) >= 2:
                span = self._fps_window[-1] - self._fps_window[0]
                if span > 0:
                    self.measured_fps = (len(self._fps_window) - 1) / span

    def _frame_to_jpeg(self, frame):
        """Convert a cv2 frame (ndarray or raw bytes) to JPEG bytes."""
        # Case 1: raw MJPEG bytes from camera (CONVERT_RGB=0 or MJPG fourcc)
        if hasattr(frame, 'tobytes'):
            raw = frame.tobytes()
            if len(raw) > 2 and raw[:2] == b'\xff\xd8':
                return raw

        # Case 2: decoded ndarray — re-encode to JPEG
        if self._cv2 is not None and hasattr(frame, 'shape'):
            encode_param = [int(self._cv2.IMWRITE_JPEG_QUALITY), self.quality]
            ok, buf = self._cv2.imencode('.jpg', frame, encode_param)
            if ok:
                return buf.tobytes()
            self.last_error = f"cv2.imencode failed (shape={frame.shape}, dtype={frame.dtype})"

        return None

    def _generate_mock_frame(self):
        try:
            from PIL import Image, ImageDraw, ImageFont
            img = Image.new("RGB", (self.width, self.height), (30, 30, 50))
            draw = ImageDraw.Draw(img)
            now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            draw.text((20, 20), f"MOCK VIDEO - {now}", fill=(0, 255, 0))
            draw.text((20, 50), f"Device: {os.environ.get('DEVICE_ID', 'unknown')}", fill=(200, 200, 200))
            draw.text((20, 80), "SIMULATED - NOT REAL CAMERA", fill=(255, 80, 80))
            import io
            buf = io.BytesIO()
            img.save(buf, format="JPEG", quality=self.quality)
            return buf.getvalue()
        except ImportError:
            return self._minimal_jpeg()

    def _minimal_jpeg(self):
        """Return a minimal valid JPEG (grey 2x2 image)."""
        # Minimal JFIF JPEG: SOI + APP0 + DQT + SOF0 + DHT + SOS + image data + EOI
        return (
            b'\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00'
            b'\xff\xdb\x00C\x00\x08\x06\x06\x07\x06\x05\x08\x07\x07\x07\t\t'
            b'\x08\n\x0c\x14\r\x0c\x0b\x0b\x0c\x19\x12\x13\x0f\x14\x1d\x1a'
            b'\x1f\x1e\x1d\x1a\x1c\x1c $.\' ",#\x1c\x1c(7),01444\x1f\'9=82<.342'
            b'\xff\xc0\x00\x0b\x08\x00\x02\x00\x02\x01\x01\x11\x00'
            b'\xff\xc4\x00\x1f\x00\x00\x01\x05\x01\x01\x01\x01\x01\x01\x00'
            b'\x00\x00\x00\x00\x00\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\t\n\x0b'
            b'\xff\xda\x00\x08\x01\x01\x00\x00?\x00T\xdb\x9e\xa7\x95'
            b'\xff\xd9'
        )

    def wait_frame(self, timeout=1.0):
        """Block until a new frame arrives, then return it. Returns None on timeout."""
        if self.mock:
            time.sleep(1.0 / self.fps)
            return self._generate_mock_frame()
        with self._cond:
            v = self._frame_version
            while self._frame_version == v:
                if not self._cond.wait(timeout):
                    return None
            return self._frame

    def get_frame(self):
        if self.mock:
            return self._generate_mock_frame()
        with self._lock:
            return self._frame

    def get_snapshot(self):
        return self.get_frame()

    def is_available(self):
        return self._available or self.mock

    def get_status(self):
        """Return video capture status dict with live diagnostics."""
        base = {
            "width": self.actual_width or self.width,
            "height": self.actual_height or self.height,
            "fps": self.actual_fps or self.fps,
            "frames_captured": self.frames_captured,
            "measured_fps": round(self.measured_fps, 1),
            "last_error": self.last_error or None,
            "actual_fourcc": self.actual_fourcc or None,
        }
        if self._real_opened and self._available:
            return {**base, "status": "online", "mode": "real", "available": True,
                    "device": self.device, "mock": False}
        if self._mock_env:
            return {**base, "status": "mock", "mode": "mock", "available": True,
                    "device": None, "mock": True}
        return {**base, "status": "mock", "mode": "mock", "available": True,
                "device": self.device, "mock": True}
