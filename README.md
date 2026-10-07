# Pokédex física con ESP32-S3

Pokédex táctil para la pantalla **Freenove ESP32-S3 de 2,8 pulgadas (320×240)**. El catálogo, las fichas, las evoluciones, los ataques, los sprites y el audio se leen desde microSD. La red solo se utiliza para el escaneo: una cámara del PC toma la foto y un modelo visual local de Ollama identifica el Pokémon.

Esta es la versión estable del proyecto. No incluye los modos experimentales Compañero/Pokelike.

## Qué incluye

- Interfaz táctil inspirada en la Pokédex de Sinnoh.
- Registro nacional y fichas en español.
- Evoluciones, estadísticas y ataques por generación, leídos desde la SD.
- Reconocimiento local mediante cámara del PC + Ollama; las imágenes no salen del ordenador.
- Los 2050 sprites procesados necesarios (pequeño y grande para #001–#1025).
- Sonidos, cries y narraciones desde la SD. El instalador puede descargar las 1025 narraciones españolas como paquete opcional.
- Backend FastAPI y herramientas para generar/provisionar los datos.
- Firmware experimental para la ESP32-CAM GC2145, separado de la versión estable con webcam.

## Material necesario

- Freenove ESP32-S3 Display FNK0104B, 2,8 pulgadas, ILI9341 táctil.
- Tarjeta microSD en FAT32 o exFAT.
- Cable USB de datos.
- PC con Windows 10/11, Python 3.11 o posterior, Git, Arduino CLI y Ollama.
- Una webcam compatible con Windows (recomendado) o, opcionalmente, la ESP32-CAM GC2145 probada en el proyecto.

El pinout y la configuración de pantalla están en [`docs/HARDWARE_PINOUT.md`](docs/HARDWARE_PINOUT.md).

## Puesta en marcha rápida

### Instalación automática recomendada

1. Descarga el repositorio y descomprímelo.
2. Inserta la microSD en la Pokédex y conecta la placa ESP32-S3 por USB.
3. Haz doble clic en **`INSTALAR POKEDEX.bat`**.
4. Elige instalación completa o Kanto y escribe los datos de tu Wi-Fi.

El asistente descarga una copia aislada de Arduino CLI, instala el core ESP32 y las versiones compatibles de las bibliotecas, configura la pantalla, prepara Python, compila y carga el firmware, genera y copia los datos a la microSD, instala Ollama y su modelo visual, y crea el acceso directo **Iniciar Pokédex** en el escritorio. También ofrece descargar e instalar las narraciones españolas. El toolchain se guarda en `%LOCALAPPDATA%\PokedexESP32` para evitar los límites de rutas largas de Windows. El controlador táctil FT6336U se incluye con su licencia MIT en `third_party/`. No necesitas modificar código ni elegir el puerto si solo hay una placa conectada.

La instalación completa puede descargar varios gigabytes y tardar bastante, sobre todo al generar las mecánicas de 1025 Pokémon. Si algo se interrumpe, vuelve a ejecutar el instalador: los pasos descargados se reutilizan. El registro queda en `.installer/instalacion.log`.

Los pasos manuales siguientes se conservan para diagnóstico o instalaciones personalizadas.

### 1. Descargar el proyecto

```powershell
git clone https://github.com/Aminsinho/pokedex-esp32-s3.git
cd pokedex-esp32-s3
```

### 2. Configurar la red sin editar código

Haz doble clic en **`Preparar WiFi.bat`**. El asistente pide:

1. el nombre de tu Wi‑Fi;
2. la contraseña;
3. la IPv4 del PC que ejecutará el backend (el asistente propone una automáticamente).

Se crea `src/pokedex/wifi_config.h`. Este archivo está ignorado por Git, por lo que la contraseña no se publica. El ESP32-S3 y el PC deben estar en la misma red y la dirección del PC debe mantenerse estable; lo más cómodo es reservarla en el router.

Si prefieres hacerlo manualmente, copia `src/pokedex/wifi_config.example.h` como `src/pokedex/wifi_config.h` y cambia sus tres valores.

### 3. Instalar Arduino y compilar el firmware

Instala Arduino CLI y el core `esp32:esp32`. También hacen falta las bibliotecas **LVGL**, **TFT_eSPI**, **ArduinoJson** y **FT6336U**.

La placa usa PSRAM OPI y una tabla de particiones propia. Desde la raíz del proyecto:

```powershell
arduino-cli compile src/pokedex `
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,USBMode=hwcdc,CDCOnBoot=cdc" `
  --build-path build-release
```

Para cargarlo, sustituye `COM4` por el puerto que aparezca en el Administrador de dispositivos:

```powershell
arduino-cli upload src/pokedex `
  --port COM4 `
  --fqbn "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,USBMode=hwcdc,CDCOnBoot=cdc" `
  --input-dir build-release
```

La configuración específica de TFT_eSPI suministrada para la placa está en `docs/tft_setups/`.

### 4. Preparar la microSD

Los sprites procesados necesarios para la interfaz y la comparación visual sí están en el repositorio. Los cientos de megabytes de binarios finales de la SD, cachés, imágenes originales y audio no se guardan en Git. Los scripts generan ese contenido en `sd_dataset/`, una carpeta ignorada por Git.

Instala las dependencias de herramientas:

```powershell
python -m venv .venv-tools
.\.venv-tools\Scripts\python.exe -m pip install requests Pillow pyserial httpx soundfile
```

Genera el catálogo binario, las mecánicas y los iconos de evolución. Para una prueba rápida puede usarse Kanto:

```powershell
.\.venv-tools\Scripts\python.exe tools\generate_local_db.py
.\.venv-tools\Scripts\python.exe tools\generate_mechanics.py --first 1 --last 151
.\.venv-tools\Scripts\python.exe tools\prepare_evolution_stones.py
```

Con el firmware encendido y el puerto correcto, provisiona los datos por USB:

```powershell
.\.venv-tools\Scripts\python.exe tools\send_to_sd.py --port COM4
.\.venv-tools\Scripts\python.exe tools\provision_sprites.py --port COM4 --first 1 --last 151
.\.venv-tools\Scripts\python.exe tools\provision_data.py --port COM4 --mechanics --stones
```

Para 1–1025, cambia el rango de provisión de sprites y ejecuta el generador de mecánicas con `--first 1 --last 1025`. `tools/sync_pokemon_data.py --all` permite actualizar voluntariamente datos y sprites desde PokéAPI. La documentación del formato y de la carga por rangos está en [`docs/DATOS_MECANICAS_SD.md`](docs/DATOS_MECANICAS_SD.md).

Las 1025 narraciones españolas están disponibles como descarga opcional de la release y el instalador comprueba su SHA-256 antes de copiarlas. Para instalarlas manualmente por rangos:

```powershell
.\.venv-tools\Scripts\python.exe tools\provision_data.py --port COM4 --narration-first 1 --narration-last 151
```

También se puede generar o instalar una narración individual; los cries siguen siendo opcionales:

```powershell
.\.venv-tools\Scripts\python.exe tools\prepare_cry.py 25
powershell -ExecutionPolicy Bypass -File tools\prepare_narration.ps1 -PokemonId 25
.\.venv-tools\Scripts\python.exe tools\provision_data.py --port COM4 --narration 25
```

### 5. Iniciar backend, cámara e IA

Instala [Ollama para Windows](https://ollama.com/download/windows). Después haz doble clic en **`Iniciar Pokedex - PCCam.bat`**. En la primera ejecución el script:

- crea el entorno virtual del backend;
- instala las dependencias;
- descarga `qwen3-vl:8b-instruct` si falta;
- inicia Ollama, FastAPI y la cámara del PC.

La descarga del modelo requiere varios gigabytes y puede tardar. Cuando todo esté iniciado, la API de diagnóstico estará en `http://127.0.0.1:8000/docs`. El puerto 8000 debe permitirse en el firewall **solo para redes privadas**.

Para usar otra webcam, abre `Iniciar Pokedex - PCCam.bat` y cambia `--index 0` por `--index 1`, `2`, etc.

### Cámara ESP32-CAM experimental

Después de la instalación principal, desconecta la Pokédex, conecta la ESP32-CAM y ejecuta **`INSTALAR ESP32-CAM EXPERIMENTAL.bat`**. El asistente pide la red, compila con el perfil `esp32:esp32:esp32cam` y carga el firmware sin guardar las credenciales en Git. Después inicia los servicios con **`Iniciar Pokedex - ESP32Cam.bat`**. La guía y los límites están en [`experimental/esp32-cam/README.md`](experimental/esp32-cam/README.md).

## Uso

1. Enciende la Pokédex y espera a que indique sistema/cámara disponibles.
2. Abre **SCAN**.
3. Coloca una figura, carta o imagen clara de un único Pokémon delante de la webcam.
4. Pulsa el botón de captura. Ollama analizará la imagen y la Pokédex abrirá la ficha local correspondiente.

El catálogo sigue funcionando sin Wi‑Fi una vez que la SD está preparada. Solo SCAN necesita el PC, el backend y Ollama.

## Comprobaciones

Backend:

```powershell
cd backend
.\.venv\Scripts\python.exe -m pytest tests -q
```

Pruebas del firmware y los datos:

```powershell
python tests\test_local_db.py
python -m pytest tests\test_mechanics.py -q
```

Estas dos comprobaciones se ejecutan después de generar `sd_dataset/`; antes de ese paso fallarán porque los binarios todavía no existen.

## Privacidad y seguridad

- No subas `src/pokedex/wifi_config.h`, `experimental/esp32-cam/camera_config.h` ni archivos `.env`.
- Ollama escucha en el propio PC y las imágenes no se envían a un servicio cloud.
- El backend está pensado para una red local de confianza: no abras el puerto 8000 a Internet.
- `POKEDEX_RETAIN_IMAGES=0` evita conservar las capturas por defecto.

## Recursos y marcas

Pokémon y sus nombres son marcas de Nintendo, Game Freak y The Pokémon Company. Este proyecto es educativo, no oficial y no está afiliado con dichas compañías. Los sprites y datos obtenidos de fuentes externas conservan las condiciones de sus respectivos autores. No se incluyen imágenes originales de alta resolución, cries ni paquetes binarios completos de la SD en Git; las narraciones generadas se distribuyen por separado como recurso de la release.

Los archivos de referencia de Freenove mantienen su licencia original indicada dentro de `docs/`.
