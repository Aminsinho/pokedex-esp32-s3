from fastapi import APIRouter

from app.services.device_service import device_service

router = APIRouter(tags=["devices"])


@router.post("/devices/status")
def device_status(payload: dict):
    if "device_id" not in payload:
        return {"ok": False, "error": "device_id required"}
    device = device_service.report(
        device_id=payload["device_id"],
        device_type=payload.get("device_type", "unknown"),
        status=payload.get("status", "online"),
        ip=payload.get("ip"),
    )
    return {"ok": True, "device": device}


@router.get("/devices")
def list_devices():
    return {"devices": device_service.list_all()}
