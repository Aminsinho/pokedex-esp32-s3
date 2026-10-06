"""On-demand PC camera agent. No continuous video, no files, no cloud upload.

Run with .venv/Scripts/python.exe camera_pc.py. Stop with Ctrl+C.
"""
import argparse
import logging
import threading
import time
import subprocess
import sys
from pathlib import Path

import cv2
import httpx

LOG = logging.getLogger("camera_pc")
DEVICE = "pokedex-camera-01"


def capture(index: int) -> bytes:
    camera = cv2.VideoCapture(index, cv2.CAP_DSHOW)
    try:
        if not camera.isOpened():
            raise RuntimeError("camera_unavailable")
        camera.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
        camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
        frame = None
        # Brief exposure warm-up, only after an explicit scan request.
        started = time.monotonic()
        for i in range(60):
            ok, frame = camera.read()
            if not ok:
                raise RuntimeError("camera_read_failed")
            if i >= 11 and time.monotonic() - started >= 1.2:
                break
        ok, jpeg = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 92])
        if not ok:
            raise RuntimeError("camera_encode_failed")
        return jpeg.tobytes()
    finally:
        camera.release()


def heartbeat(base: str, stop: threading.Event):
    with httpx.Client(timeout=5, trust_env=False) as client:
        while not stop.is_set():
            try:
                client.post(base + "/devices/status", json={
                    "device_id": DEVICE, "device_type": "camera", "status": "online"}).raise_for_status()
            except httpx.HTTPError:
                LOG.warning("Backend unavailable for heartbeat")
            stop.wait(10)
        try:
            client.post(base + "/devices/status", json={
                "device_id": DEVICE, "device_type": "camera", "status": "offline"})
        except httpx.HTTPError:
            pass


def capture_bounded(index: int) -> bytes:
    # Native camera drivers can block read/open. Isolate them so the polling
    # service recovers and Windows releases the camera even after a timeout.
    result = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                             "--capture-once", str(index)],
                            capture_output=True, check=True, timeout=12,
                            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if not result.stdout:
        raise RuntimeError("empty_capture")
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", default="http://127.0.0.1:8000")
    parser.add_argument("--index", type=int, default=0)
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")
    logging.getLogger("httpx").setLevel(logging.WARNING)
    base = args.backend.rstrip("/") + "/api/v1"
    stop = threading.Event()
    thread = threading.Thread(target=heartbeat, args=(base, stop), daemon=True)
    thread.start()
    LOG.info("Camera agent ready (index %d); captures only on SCAN", args.index)
    try:
        with httpx.Client(timeout=httpx.Timeout(135, connect=5), trust_env=False) as client:
            while not stop.is_set():
                scan_id = None
                try:
                    response = client.get(base + "/camera/pending", params={"device_id": DEVICE}, timeout=5)
                    response.raise_for_status()
                    pending = response.json()
                    if pending.get("pending"):
                        scan_id = pending["scan_id"]
                        started = time.monotonic()
                        image = capture_bounded(args.index)
                        LOG.info("Scan %s captured (%d bytes)", scan_id, len(image))
                        response = client.post(base + f"/scan/{scan_id}/image",
                            data={"device_id": DEVICE}, files={"image": ("capture.jpg", image, "image/jpeg")})
                        response.raise_for_status()
                        LOG.info("Scan %s: %s in %.1fs", scan_id, response.json()["status"], time.monotonic()-started)
                except Exception as exc:
                    LOG.warning("Camera cycle failed: %s", type(exc).__name__)
                    if scan_id:
                        try:
                            client.post(base + f"/scan/{scan_id}/camera-error", timeout=5)
                        except httpx.HTTPError:
                            pass
                stop.wait(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        thread.join(timeout=6)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--capture-once":
        sys.stdout.buffer.write(capture(int(sys.argv[2])))
    else:
        main()
