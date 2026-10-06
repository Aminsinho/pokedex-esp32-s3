from dataclasses import dataclass
from datetime import datetime, timezone


@dataclass
class Device:
    device_id: str
    device_type: str = "unknown"   # display | camera
    status: str = "offline"        # online | offline
    ip: str | None = None
    last_seen: str | None = None


class DeviceService:
    """In-memory device registry (M1). No complex persistence yet."""

    def __init__(self):
        self._devices: dict[str, Device] = {}

    def report(self, device_id: str, device_type: str | None = None,
               status: str | None = None, ip: str | None = None) -> dict:
        d = self._devices.get(device_id) or Device(device_id=device_id)
        if device_type:
            d.device_type = device_type
        if status:
            d.status = status
        if ip:
            d.ip = ip
        d.last_seen = datetime.now(timezone.utc).isoformat()
        self._devices[device_id] = d
        return self._to_dict(d)

    def get(self, device_id: str) -> dict | None:
        return self._to_dict(self._devices.get(device_id))

    def list_all(self) -> list[dict]:
        return [self._to_dict(d) for d in self._devices.values()]

    @staticmethod
    def _to_dict(d: Device | None) -> dict | None:
        if d is None:
            return None
        status = d.status
        if d.device_type == "camera" and d.last_seen:
            age = (datetime.now(timezone.utc) - datetime.fromisoformat(d.last_seen)).total_seconds()
            if age > 35:
                status = "offline"
        return {
            "device_id": d.device_id,
            "device_type": d.device_type,
            "status": status,
            "ip": d.ip,
            "last_seen": d.last_seen,
        }


device_service = DeviceService()
