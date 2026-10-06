import json
from pathlib import Path

from fastapi import APIRouter, HTTPException
from fastapi.responses import FileResponse, Response

from app.config import settings
from app.services.pokemon_service import pokemon_service

router = APIRouter(tags=["pokemon"])


@router.get("/status")
def status():
    return {
        "status": "online",
        "version": settings.version,
        "service": settings.service,
    }


@router.get("/pokemon")
def list_pokemon(search: str | None = None, type: str | None = None,
                 page: int = 1, limit: int = 20):
    return pokemon_service.list_pokemon(search=search, type=type,
                                        page=page, limit=limit)


@router.get("/pokemon/{pokemon_id}")
def get_pokemon(pokemon_id: int):
    detail = pokemon_service.get_pokemon(pokemon_id)
    if detail is None:
        raise HTTPException(status_code=404, detail=f"Pokemon {pokemon_id} not found")
    return detail


# ─── ASSETS ──────────────────────────────────────────────────────────────────


@router.get("/assets/manifest")
def asset_manifest():
    """Return the asset manifest for the ESP32 to sync."""
    manifest_file = settings.data_file.parent / "manifest.json"
    if not manifest_file.exists():
        raise HTTPException(status_code=404, detail="Manifest not found")
    with open(manifest_file) as f:
        return json.load(f)


@router.get("/assets/pokemon/{pokemon_id}/small")
def asset_small(pokemon_id: int):
    """Serve small sprite (48x48 RGB565 raw)."""
    if pokemon_service.get_pokemon(pokemon_id) is None:
        raise HTTPException(status_code=404, detail="not found")
    # Prefer .r565 raw format for ESP32
    r565_path = settings.assets_dir / "small" / f"{pokemon_id:04d}.r565"
    if r565_path.exists():
        return FileResponse(r565_path, media_type="application/octet-stream")
    png_path = settings.assets_dir / "small" / f"{pokemon_id:04d}.png"
    if png_path.exists():
        return FileResponse(png_path, media_type="image/png")
    raise HTTPException(status_code=404, detail="sprite not found")


@router.get("/assets/pokemon/{pokemon_id}/large")
def asset_large(pokemon_id: int):
    """Serve large sprite (96x96 RGB565 raw)."""
    if pokemon_service.get_pokemon(pokemon_id) is None:
        raise HTTPException(status_code=404, detail="not found")
    # Prefer .r565 raw format for ESP32
    r565_path = settings.assets_dir / "large" / f"{pokemon_id:04d}.r565"
    if r565_path.exists():
        return FileResponse(r565_path, media_type="application/octet-stream")
    png_path = settings.assets_dir / "large" / f"{pokemon_id:04d}.png"
    if png_path.exists():
        return FileResponse(png_path, media_type="image/png")
    raise HTTPException(status_code=404, detail="sprite not found")


# ─── DATA VERSION ────────────────────────────────────────────────────────────


@router.get("/data/version")
def data_version():
    """Return dataset version info."""
    version_file = settings.data_file.parent / "version.json"
    if version_file.exists():
        with open(version_file) as f:
            return json.load(f)
    return {
        "dataset_version": "unknown",
        "pokemon_count": pokemon_service.get_count(),
    }
