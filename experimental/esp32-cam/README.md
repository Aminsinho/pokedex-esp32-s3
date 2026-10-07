# ESP32-CAM experimental (GC2145)

Este firmware sustituye opcionalmente la webcam del PC. Está validado con la
placa ESP32-CAM-MB del proyecto, pinout AI-Thinker y sensor GC2145. Mantiene una
vista MJPEG en la red local, captura una imagen VGA y la envía al mismo backend
que usa la Pokédex.

## Instalación sencilla en Windows

1. Ejecuta primero `INSTALAR POKEDEX.bat` para preparar Arduino y el backend.
2. Desconecta la Pokédex y conecta únicamente la ESP32-CAM por USB.
3. Haz doble clic en `INSTALAR ESP32-CAM EXPERIMENTAL.bat`.
4. Introduce la misma Wi-Fi y la IPv4 del PC que ejecutará el backend.
5. Inicia el backend con `Iniciar Pokedex - ESP32Cam.bat` y pulsa SCAN.

El asistente crea `camera_config.h`, que está ignorado por Git. No publiques ese
archivo porque contiene la contraseña de red. El ejemplo seguro está en
`camera_config.example.h`.

La cámara expone `http://IP-DE-LA-CAMARA/` para ajustar y revisar la imagen y
`http://IP-DE-LA-CAMARA:81/stream` para el directo. Es un componente
experimental: la webcam del PC continúa siendo la opción recomendada.

## Compilación manual

```powershell
arduino-cli compile experimental/esp32-cam --fqbn esp32:esp32:esp32cam
arduino-cli upload experimental/esp32-cam --port COM5 --fqbn esp32:esp32:esp32cam
```

El firmware y el backend son para una red local de confianza. No publiques los
puertos 80, 81 u 8000 en Internet.
