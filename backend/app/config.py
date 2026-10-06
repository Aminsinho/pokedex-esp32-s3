from pathlib import Path
import os

BASE_DIR = Path(__file__).resolve().parent
BACKEND_DIR = BASE_DIR.parent


class Settings:
    vision_provider = os.getenv("POKEDEX_VISION_PROVIDER", "ollama")
    ollama_url = os.getenv("POKEDEX_OLLAMA_URL", "http://127.0.0.1:11434")
    ollama_model = os.getenv("POKEDEX_OLLAMA_MODEL", "qwen3-vl:8b-instruct")
    vision_timeout = 120.0
    min_confidence = 0.80
    retain_scan_images = os.getenv("POKEDEX_RETAIN_IMAGES", "0") == "1"
    max_image_bytes = 5 * 1024 * 1024
    camera_device_id = os.getenv("POKEDEX_CAMERA_DEVICE", "pokedex-camera-01")
    version = "0.2.0"
    service = "pokedex-backend"
    data_file = BASE_DIR / "data" / "pokemon.json"
    upload_dir = BACKEND_DIR / "storage" / "uploads"
    assets_dir = BACKEND_DIR / "storage" / "assets"
    data_version_file = BASE_DIR / "data" / "version.json"


settings = Settings()
settings.upload_dir.mkdir(parents=True, exist_ok=True)
settings.assets_dir.mkdir(parents=True, exist_ok=True)
