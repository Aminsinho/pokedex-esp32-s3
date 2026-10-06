"""Focused new tests; real model/camera acceptance is a separate manual check."""
import io
import json
import threading
import time
from concurrent.futures import ThreadPoolExecutor

import httpx
import pytest
from PIL import Image
from fastapi.testclient import TestClient

from app.main import app
from app.config import settings
from app.services import scan_service as scan_module
from app.services.scan_service import ScanService, ScanState
from app.services.vision_service import OllamaVisionService, VisionResult


@pytest.fixture
def jpeg():
    output = io.BytesIO()
    Image.new("RGB", (32, 32), "white").save(output, "JPEG")
    return output.getvalue()


def reply(monkeypatch, answer=None, error=None):
    def post(self, url, **kwargs):
        assert url.endswith("/api/chat")
        assert kwargs["json"]["messages"][0]["images"]
        assert kwargs["json"]["stream"] is False
        if error:
            raise error
        if "same_species" in kwargs["json"]["format"].get("properties", {}):
            return httpx.Response(200, request=httpx.Request("POST", url),
                json={"message": {"content": '{"same_species": true}'}})
        return httpx.Response(200, request=httpx.Request("POST", url),
            json={"message": {"content": json.dumps(answer)}})
    monkeypatch.setattr(httpx.Client, "post", post)


def test_identification_validated_against_catalogue(monkeypatch, jpeg):
    reply(monkeypatch, {"found": True, "name": "Pikachu", "confidence": .95})
    assert OllamaVisionService().recognize(jpeg).pokemon_id == 25


@pytest.mark.parametrize("answer", [
    {"found": False, "pokemon_id": 0, "name": "", "confidence": 0.0},
    {"found": True, "pokemon_id": 25, "name": "Pikachu", "confidence": .4},
    {"found": True, "pokemon_id": 25, "name": "Charizard", "confidence": .95},
    {"found": True, "pokemon_id": 1026, "name": "Invented", "confidence": .99},
    {"found": True, "pokemon_id": "25", "name": "Pikachu", "confidence": .9},
    {"found": True, "pokemon_id": 25, "name": "Pikachu", "confidence": float("nan")},
])
def test_reject_ambiguous_or_invalid_model_output(monkeypatch, jpeg, answer):
    reply(monkeypatch, answer)
    with pytest.raises(ValueError):
        OllamaVisionService().recognize(jpeg)


def test_invalid_image_never_reaches_ollama(monkeypatch):
    def forbidden(*a, **kw):
        pytest.fail("must not send invalid image")
    monkeypatch.setattr(httpx.Client, "post", forbidden)
    with pytest.raises(ValueError, match="invalid_image"):
        OllamaVisionService().recognize(b"not an image")


def test_model_offline_reports_error_without_mock(monkeypatch, jpeg):
    reply(monkeypatch, error=httpx.ConnectError("offline"))
    monkeypatch.setattr(scan_module, "vision_service", OllamaVisionService())
    service = ScanService()
    scan = service.create_scan()
    result = service.upload_image(scan.scan_id, jpeg)
    assert result.state == ScanState.ERROR
    assert result.error == "vision_unavailable"
    assert result.pokemon_id is None
    assert result.image_path is None


def test_single_active_scan_and_expiry():
    service = ScanService()
    scan = service.create_scan()
    with pytest.raises(ValueError, match="busy"):
        service.create_scan()
    assert service.claim_pending("pokedex-camera-esp32-01") is None
    assert service.claim_pending("pokedex-camera-01").scan_id == scan.scan_id
    assert service.claim_pending("pokedex-camera-01") is None
    scan.started -= 151
    assert service.get_scan(scan.scan_id).state == ScanState.ERROR
    assert service.create_scan().scan_id != scan.scan_id


def test_poll_remains_responsive_during_inference(monkeypatch, jpeg):
    from app.api import scans
    service = ScanService()
    monkeypatch.setattr(scans, "scan_service", service)
    entered, release = threading.Event(), threading.Event()
    class SlowVision:
        def recognize(self, image):
            entered.set()
            assert release.wait(5)
            return VisionResult(25, .95)
    monkeypatch.setattr(scan_module, "vision_service", SlowVision())
    with TestClient(app) as client, ThreadPoolExecutor() as pool:
        scan_id = client.post("/api/v1/scan/request", json={}).json()["scan_id"]
        future = pool.submit(client.post, f"/api/v1/scan/{scan_id}/image",
            data={"device_id": "pokedex-camera-01"}, files={"image": ("test.jpg", jpeg, "image/jpeg")})
        try:
            assert entered.wait(2)
            started = time.monotonic()
            status = client.get(f"/api/v1/scan/{scan_id}").json()
            assert time.monotonic() - started < 1
            assert status["status"] == "processing"
        finally:
            release.set()
        assert future.result(timeout=3).status_code == 200
        assert client.get(f"/api/v1/scan/{scan_id}").json()["pokemon"]["id"] == 25
        duplicate = client.post(f"/api/v1/scan/{scan_id}/image",
            data={"device_id": "camera"}, files={"image": ("test.jpg", jpeg, "image/jpeg")})
        assert duplicate.status_code == 409


def test_camera_failure_is_terminal():
    service = ScanService()
    scan = service.create_scan()
    service.fail(scan.scan_id, "camera_capture_failed")
    assert service.to_response(scan)["error"] == "camera_capture_failed"


def test_camera_capture_releases_device_on_error(monkeypatch):
    import camera_pc
    class Camera:
        released = False
        def isOpened(self): return True
        def set(self, *args): pass
        def read(self): return False, None
        def release(self): self.released = True
    camera = Camera()
    monkeypatch.setattr(camera_pc.cv2, "VideoCapture", lambda *args: camera)
    with pytest.raises(RuntimeError, match="camera_read_failed"):
        camera_pc.capture(0)
    assert camera.released


def test_capture_driver_timeout_is_bounded(monkeypatch):
    import camera_pc
    import subprocess
    def timeout(*args, **kwargs):
        assert kwargs["timeout"] == 12
        raise subprocess.TimeoutExpired("camera", 12)
    monkeypatch.setattr(camera_pc.subprocess, "run", timeout)
    with pytest.raises(subprocess.TimeoutExpired):
        camera_pc.capture_bounded(0)
