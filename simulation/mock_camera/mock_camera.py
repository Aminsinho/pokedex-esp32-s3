#!/usr/bin/env python3
"""Mock ESP32-CAM for the Pokédex scan pipeline.

Emulates the camera controller firmware:
  1. registers as pokedex-camera-01
  2. polls GET /api/v1/camera/pending (750 ms)
  3. on pending scan: reads a local JPEG and uploads it
     POST /api/v1/scan/{scan_id}/image  (multipart: device_id, image)

Stdlib only (urllib) so it runs anywhere Python runs.

Usage:
    python mock_camera.py                          # keep polling
    python mock_camera.py --once                   # exit after one scan (or if none)
    python mock_camera.py --backend http://10.0.0.5:8000
"""
import argparse
import base64
import io
import json
import os
import time
import urllib.request
import uuid

DEVICE_ID = "pokedex-camera-01"
POLL_INTERVAL = 0.75  # seconds — no aggressive polling

# Minimal valid 1x1 JPEG, used if no test image exists.
TINY_JPEG_B64 = (
    "/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRof"
    "Hh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/wAALCAABAAEBAREA/8QAFAA"
    "BAAAAAAAAAAAAAAAAAAAACf/EABQQAQAAAAAAAAAAAAAAAAAAAAD/2gAIAQEAAD8AVN//2Q=="
)


def http_json(url: str, payload: dict | None = None) -> dict:
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(url, data=data, method="POST" if data else "GET")
    if data:
        req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read().decode())


def upload_multipart(url: str, device_id: str, image_path: str) -> dict:
    boundary = uuid.uuid4().hex
    with open(image_path, "rb") as f:
        img = f.read()
    body = io.BytesIO()
    body.write(f'--{boundary}\r\nContent-Disposition: form-data; name="device_id"\r\n\r\n{device_id}\r\n'.encode())
    body.write(
        f'--{boundary}\r\nContent-Disposition: form-data; name="image"; '
        f'filename="{os.path.basename(image_path)}"\r\nContent-Type: image/jpeg\r\n\r\n'.encode())
    body.write(img)
    body.write(f"\r\n--{boundary}--\r\n".encode())
    req = urllib.request.Request(url, data=body.getvalue(), method="POST")
    req.add_header("Content-Type", f"multipart/form-data; boundary={boundary}")
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read().decode())


def main() -> None:
    ap = argparse.ArgumentParser(description="Mock ESP32-CAM for Pokédex pipeline")
    ap.add_argument("--backend", default="http://127.0.0.1:8000")
    ap.add_argument("--image", default=os.path.join(os.path.dirname(__file__), "test.jpg"))
    ap.add_argument("--once", action="store_true",
                    help="exit after handling one scan (or immediately if none pending)")
    args = ap.parse_args()
    base = args.backend.rstrip("/")

    if not os.path.exists(args.image):
        with open(args.image, "wb") as f:
            f.write(base64.b64decode(TINY_JPEG_B64))
        print(f"[mock-camera] created test image: {args.image}")

    print(f"[mock-camera] {DEVICE_ID} online -> {base}")
    http_json(f"{base}/api/v1/devices/status",
              {"device_id": DEVICE_ID, "device_type": "camera", "status": "online"})

    while True:
        try:
            pending = http_json(f"{base}/api/v1/camera/pending?device_id={DEVICE_ID}")
        except Exception as e:
            print(f"[mock-camera] backend unreachable: {e}")
            if args.once:
                return
            time.sleep(POLL_INTERVAL)
            continue

        if not pending.get("pending"):
            if args.once:
                print("[mock-camera] no pending scan, exiting")
                return
            time.sleep(POLL_INTERVAL)
            continue

        scan_id = pending["scan_id"]
        print(f"[mock-camera] scan {scan_id}: CAPTURING -> UPLOADING ({args.image})")
        result = upload_multipart(f"{base}/api/v1/scan/{scan_id}/image",
                                  DEVICE_ID, args.image)
        print(f"[mock-camera] upload result: {json.dumps(result)}")
        if args.once:
            return


if __name__ == "__main__":
    main()
