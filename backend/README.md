# Pokédex Backend (M1)

Cerebro central de la Pokédex. FastAPI + datos en memoria + JSON local.

## Arranque

```bash
pip install -r requirements.txt
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
```

Muestra: `POKÉDEX BACKEND ONLINE`

## Endpoints (v1)

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/api/v1/status` | Estado del backend |
| GET | `/api/v1/pokemon` | Lista (search, type, page, limit) |
| GET | `/api/v1/pokemon/{id}` | Detalle |
| GET | `/api/v1/assets/pokemon/{id}` | Placeholder sprite (SVG) |
| POST | `/api/v1/scan/request` | Display pide scan → `scan_id` |
| GET | `/api/v1/camera/pending?device_id=` | Cámara poll (750 ms) |
| POST | `/api/v1/scan/{scan_id}/image` | Upload JPEG multipart |
| GET | `/api/v1/scan/{scan_id}` | Estado / resultado |
| POST | `/api/v1/devices/status` | Registro de dispositivo |
| GET | `/api/v1/devices` | Lista dispositivos |

## Mock vision

`MockVisionService` devuelve siempre `#025 Pikachu @ 0.97`.
Sustituible por `RealVisionService` sin tocar API ni firmware.

## Tests

```bash
cd backend
python -m pytest tests/ -v
```

## Mock camera (pipeline E2E sin hardware)

```bash
python simulation/mock_camera/mock_camera.py
```
