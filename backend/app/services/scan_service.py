import uuid
import time
import logging
from threading import RLock
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from app.config import settings
from app.services.pokemon_service import pokemon_service
from app.services.vision_service import vision_service


class ScanState(str, Enum):
    WAITING_CAMERA = "waiting_camera"
    CAPTURING = "capturing"
    UPLOADING = "uploading"
    PROCESSING = "processing"
    COMPLETE = "complete"
    ERROR = "error"


@dataclass
class Scan:
    scan_id: str
    state: ScanState = ScanState.WAITING_CAMERA
    display_id: str | None = None
    created_at: str = field(
        default_factory=lambda: datetime.now(timezone.utc).isoformat())
    image_path: str | None = None
    pokemon_id: int | None = None
    confidence: float | None = None
    error: str | None = None
    started: float = field(default_factory=time.monotonic)


class ScanService:
    """Scan pipeline state machine (in-memory for M1).

    waiting_camera → capturing → uploading → processing → complete | error
    """

    def __init__(self):
        self._scans: dict[str, Scan] = {}
        self._lock = RLock()

    def _expire(self):
        for scan in self._scans.values():
            if scan.state not in (ScanState.COMPLETE, ScanState.ERROR) and time.monotonic() - scan.started > 150:
                scan.state, scan.error = ScanState.ERROR, "scan_timeout"

    def create_scan(self, display_id: str | None = None) -> Scan:
        with self._lock:
            self._expire()
            if any(s.state not in (ScanState.COMPLETE, ScanState.ERROR) for s in self._scans.values()):
                raise ValueError("scan_busy")
            while len(self._scans) >= 64:
                self._scans.pop(next(iter(self._scans)))
            scan = Scan(scan_id=uuid.uuid4().hex[:12], display_id=display_id)
            self._scans[scan.scan_id] = scan
            return scan

    def claim_pending(self, device_id: str) -> Scan | None:
        """Camera polls: claim first scan waiting for the camera."""
        if device_id != settings.camera_device_id:
            return None
        with self._lock:
            self._expire()
            for scan in self._scans.values():
                if scan.state is ScanState.WAITING_CAMERA:
                    scan.state = ScanState.CAPTURING
                    return scan
        return None

    def upload_image(self, scan_id: str, data: bytes) -> Scan | None:
        with self._lock:
            self._expire()
            scan = self._scans.get(scan_id)
            if scan is None:
                return None
            if scan.state not in (ScanState.CAPTURING, ScanState.WAITING_CAMERA):
                raise ValueError("scan_not_uploadable")
            scan.state = ScanState.PROCESSING
        try:
            if settings.retain_scan_images:
                path = settings.upload_dir / f"{scan_id}.jpg"
                path.write_bytes(data)
                scan.image_path = str(path)
            result = vision_service.recognize(data)
            with self._lock:
                self._expire()
                if scan.state is ScanState.PROCESSING:
                    scan.pokemon_id = result.pokemon_id
                    scan.confidence = result.confidence
                    scan.state = ScanState.COMPLETE
        except Exception as e:  # keep pipeline alive, report error
            safe_errors = {"pokemon_not_recognized", "pokemon_identity_mismatch", "pokemon_reference_mismatch", "invalid_image", "invalid_image_size", "image_too_large"}
            logging.getLogger(__name__).warning("Scan %s failed: %s (%s)", scan_id,
                type(e).__name__, str(e) if str(e) in safe_errors else "vision_unavailable")
            self.fail(scan_id, str(e) if str(e) in safe_errors else "vision_unavailable")
        return scan

    def fail(self, scan_id: str, error: str) -> Scan | None:
        with self._lock:
            scan = self._scans.get(scan_id)
            if scan and scan.state not in (ScanState.COMPLETE, ScanState.ERROR):
                scan.state, scan.error = ScanState.ERROR, error
            return scan

    def get_scan(self, scan_id: str) -> Scan | None:
        with self._lock:
            self._expire()
            return self._scans.get(scan_id)

    def to_response(self, scan: Scan) -> dict:
        resp: dict = {"scan_id": scan.scan_id, "status": scan.state.value}
        if scan.state is ScanState.COMPLETE:
            p = pokemon_service.get_pokemon(scan.pokemon_id)
            resp["pokemon"] = {
                "id": scan.pokemon_id,
                "name": p["name"] if p else "Unknown",
            }
            resp["confidence"] = scan.confidence
        if scan.error:
            resp["error"] = scan.error
        return resp


scan_service = ScanService()
