from fastapi import APIRouter

from . import devices, pokemon, scans

api_router = APIRouter(prefix="/api/v1")
api_router.include_router(pokemon.router)
api_router.include_router(scans.router)
api_router.include_router(devices.router)
