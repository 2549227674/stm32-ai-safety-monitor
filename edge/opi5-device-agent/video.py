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
        self.width = int(os.environ.get("VIDEO_WIDTH", "1280"))
        self.height = int(os.environ.get("VIDEO_HEIGHT", "720"))
        self.fps = int(os.environ.get("VIDEO_FPS", "12"))
        self.quality = int(os.environ.get("VIDEO_JPEG_QUALITY", "82"))
        self._frame = None
        self._lock = threading.Lock()
        self._available = False
        self._real_opened = False
        self._cap = None

        if not self.mock:
            self._try_open_camera()
        if not self._available:
            self.mock = True
            print("[video] using mock video stream")

    def _try_open_camera(self):
        try:
            import cv2
            self._cap = cv2.VideoCapture(self.device)
            if self._cap.isOpened():
                self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
                self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
                self._cap.set(cv2.CAP_PROP_FPS, self.fps)
                self._available = True
                self._real_opened = True
                threading.Thread(target=self._capture_loop, daemon=True).start()
                print(f"[video] opened {self.device} {self.width}x{self.height}@{self.fps}fps")
            else:
                print(f"[video] failed to open {self.device}")
        except ImportError:
            print("[video] opencv not available")
        except Exception as e:
            print(f"[video] camera error: {e}")

    def _capture_loop(self):
        import cv2
        while self._cap and self._cap.isOpened():
            ret, frame = self._cap.read()
            if ret:
                _, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, self.quality])
                with self._lock:
                    self._frame = buf.tobytes()
            time.sleep(1.0 / self.fps)

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
        """Return video capture status dict."""
        if self._real_opened and self._available:
            return {
                "status": "online",
                "mode": "real",
                "available": True,
                "device": self.device,
                "width": self.width,
                "height": self.height,
                "fps": self.fps,
                "mock": False,
            }
        if self._mock_env:
            return {
                "status": "mock",
                "mode": "mock",
                "available": True,
                "device": None,
                "width": self.width,
                "height": self.height,
                "fps": self.fps,
                "mock": True,
            }
        # Real camera was tried but failed, fell back to mock
        return {
            "status": "mock",
            "mode": "mock",
            "available": True,
            "device": self.device,
            "width": self.width,
            "height": self.height,
            "fps": self.fps,
            "mock": True,
        }
