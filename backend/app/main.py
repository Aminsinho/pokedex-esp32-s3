from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.api import api_router
from app.config import settings


@asynccontextmanager
async def lifespan(app: FastAPI):
    print("POKÉDEX BACKEND ONLINE")
    print(f"  version: {settings.version}")
    print(f"  data:    {settings.data_file}")
    yield


app = FastAPI(title="Pokédex Backend", version=settings.version, lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(api_router)


@app.get("/")
def root():
    return {"service": settings.service, "status": "online"}
