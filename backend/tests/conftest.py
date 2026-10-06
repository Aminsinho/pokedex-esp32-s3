import base64
import io
import sys
from pathlib import Path

# make `app` importable from backend/
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import pytest
from fastapi.testclient import TestClient
from app.main import app

# Minimal valid 1x1 JPEG for upload tests.
TINY_JPEG = base64.b64decode(
    "/9j/4AAQSkZJRgABAQEASABIAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRof"
    "Hh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/wAALCAABAAEBAREA/8QAFAA"
    "BAAAAAAAAAAAAAAAAAAAACf/EABQQAQAAAAAAAAAAAAAAAAAAAAD/2gAIAQEAAD8AVN//2Q=="
)


@pytest.fixture()
def client(monkeypatch):
    # Legacy pipeline tests explicitly exercise the mock, never a live webcam/model.
    from app.services import scan_service
    from app.services.vision_service import MockVisionService
    monkeypatch.setattr(scan_service, "vision_service", MockVisionService())
    with TestClient(app) as c:
        yield c
