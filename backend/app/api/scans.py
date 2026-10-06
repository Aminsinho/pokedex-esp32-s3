from fastapi import APIRouter, File, Form, HTTPException, Query, UploadFile
from starlette.concurrency import run_in_threadpool
from app.config import settings

from app.services.scan_service import scan_service

router = APIRouter(tags=["scans"])


@router.post("/scan/request")
def request_scan(payload: dict):
    device_id = payload.get("device_id", "unknown")
    try:
        scan = scan_service.create_scan(display_id=device_id)
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    return {"scan_id": scan.scan_id, "status": scan.state.value}


@router.get("/camera/pending")
def camera_pending(device_id: str = Query("pokedex-camera-01")):
    claimed = scan_service.claim_pending(device_id)
    if claimed is None:
        return {"pending": False}
    return {"pending": True, "scan_id": claimed.scan_id}


@router.post("/scan/{scan_id}/image")
async def upload_image(scan_id: str,
                       device_id: str = Form(...),
                       image: UploadFile = File(...)):
    data = await image.read(settings.max_image_bytes + 1)
    await image.close()
    if len(data) > settings.max_image_bytes:
        scan_service.fail(scan_id, "image_too_large")
        raise HTTPException(status_code=413, detail="image_too_large")
    try:
        # Ollama is blocking I/O: keep FastAPI event loop free for display polling.
        scan = await run_in_threadpool(scan_service.upload_image, scan_id, data)
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    if scan is None:
        raise HTTPException(status_code=404, detail="unknown scan_id")
    return {"scan_id": scan.scan_id, "status": scan.state.value}


@router.post("/scan/{scan_id}/camera-error")
def camera_error(scan_id: str):
    scan = scan_service.fail(scan_id, "camera_capture_failed")
    if scan is None:
        raise HTTPException(status_code=404, detail="unknown scan_id")
    return scan_service.to_response(scan)


@router.get("/scan/{scan_id}")
def get_scan(scan_id: str):
    scan = scan_service.get_scan(scan_id)
    if scan is None:
        raise HTTPException(status_code=404, detail="unknown scan_id")
    return scan_service.to_response(scan)


