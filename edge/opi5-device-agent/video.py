"""Video capture - real camera or mock JPEG stream."""

import os
import time
import threading
from datetime import datetime


class VideoCapture:
    def __init__(self):
        self.mock = os.environ.get("VIDEO_MOCK", "0") == "1"
        self.device = os.environ.get("CAMERA_DEVICE", "/dev/video0")
        self.width = int(os.environ.get("VIDEO_WIDTH", "1280"))
        self.height = int(os.environ.get("VIDEO_HEIGHT", "720"))
        self.fps = int(os.environ.get("VIDEO_FPS", "12"))
        self.quality = int(os.environ.get("VIDEO_JPEG_QUALITY", "82"))
        self._frame = None
        self._lock = threading.Lock()
        self._available = False
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
            import io
            buf = io.BytesIO()
            img.save(buf, format="JPEG", quality=self.quality)
            return buf.getvalue()
        except ImportError:
            return self._minimal_jpeg()

    def _minimal_jpeg(self):
        import struct
        now = datetime.now().strftime("%H:%M:%S")
        data = f"MOCK {now}".encode()
        return data

    def get_frame(self):
        if self.mock:
            return self._generate_mock_frame()
        with self._lock:
            return self._frame

    def get_snapshot(self):
        return self.get_frame()

    def is_available(self):
        return self._available or self.mock
