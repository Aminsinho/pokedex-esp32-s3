# Cámara PC y reconocimiento local

Flujo: SCAN en ESP32 → POST scan/request → agente webcam reclama petición →
foto JPEG → backend/Ollama → ID validado → polling del ESP32 → ficha desde SD.

Modelo predeterminado: `qwen3-vl:8b-instruct` (ya instalado en este ordenador).
Ollama debe ejecutarse localmente en `127.0.0.1:11434`. No se usa un servicio cloud.
La foto se captura solo cuando hay un SCAN pendiente y no se escribe en disco
salvo que se configure explícitamente `POKEDEX_RETAIN_IMAGES=1`.

## Arranque manual (PowerShell)

Desde `C:\Users\aminj\Desktop\pokedexProject\backend`, en terminales separadas:

```powershell
ollama serve
.\.venv\Scripts\python.exe -m uvicorn app.main:app --host 0.0.0.0 --port 8000
.\.venv\Scripts\python.exe camera_pc.py --index 0
```

No ejecutar duplicados si el puerto/servicio ya está activo. Ctrl+C detiene cada
servicio lanzado en terminal. El agente libera la cámara después de capturar;
no tiene vista previa ni grabación continua. El LED puede encenderse brevemente
al pulsar SCAN. No configurar inicio automático de Windows sin pedirlo.

Instalación reproducible si falta el entorno:

```powershell
python -m venv --system-site-packages .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-camera.txt
```

Variables opcionales: `POKEDEX_OLLAMA_MODEL`, `POKEDEX_OLLAMA_URL`,
`POKEDEX_RETAIN_IMAGES`. `POKEDEX_VISION_PROVIDER=mock` es solo para pruebas
históricas y NO identifica imágenes. Por defecto es `ollama`, sin fallback mock.
`POKEDEX_CAMERA_DEVICE` selecciona qué cámara puede reclamar los escaneos:
`pokedex-camera-01` (webcam PC, predeterminada y compatible con el display) o
`pokedex-camera-esp32-01`.

## Uso y límites

1. Colocar una figura, carta o imagen de un solo Pokémon delante de la C920.
2. En Pokédex entrar en SCAN y pulsar SCAN. Esperar CAPTURING/ANALYZING.
3. Al reconocerlo aparece MATCH y se abre su ficha local. BACK sigue operativo.
4. Si no se reconoce, mejorar iluminación/encuadre y reintentar.

Validación estricta de ID y nombre contra catálogo 1–1025; rechazo si la
autoevaluación del modelo es inferior a 0.80. Esta cifra no es una probabilidad
calibrada ni garantiza exactitud. Un solo trabajo simultáneo, inferencia hasta
120 s y caducidad del scan a 150 s. Inicio en frío del modelo puede tardar más.
Backend mantiene consultas de estado responsivas durante inferencia.

El agente publica heartbeat cada 10 s; después de 35 s sin heartbeat aparece
offline. Online indica agente disponible, no garantiza permisos/imagen válida.
Cancelar en el display impide abrir una ficha tardía, pero la petición ya
capturada puede terminar de procesarse en backend.

Servicio MVP para LAN de confianza: API existente sin autenticación, NO abrir
puerto 8000 a Internet. Ollama permanece en loopback. No se cambió firewall.

## Pruebas focalizadas

```powershell
.\.venv\Scripts\python.exe -m pytest tests/test_camera_ollama.py -q
```

Estos tests usan dobles de modelo/cámara; no sustituyen probar una webcam real.
Fuentes oficiales de integración:
- https://docs.ollama.com/capabilities/vision
- https://docs.ollama.com/capabilities/structured-outputs
