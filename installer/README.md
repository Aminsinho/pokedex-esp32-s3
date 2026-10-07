# Instalador gráfico

`INSTALAR POKEDEX.exe` es una aplicación WinForms pequeña que acompaña al ZIP y
reutiliza `tools/instalar_pokedex.ps1` como motor. No contiene firmware ni
credenciales embebidas.

Funciones:

- detecta puertos serie y prioriza VID/PID habituales de Espressif, CH340 y CP210x;
- permite confirmar manualmente el puerto si el dispositivo no se identifica;
- configura Wi-Fi y backend sin exponer la contraseña en argumentos de procesos;
- selecciona Kanto o catálogo completo, narraciones y limpieza final;
- muestra salida y errores en tiempo real;
- ofrece el instalador ESP32-CAM al completar la instalación principal.

## Reconstruir

En Windows:

```powershell
powershell -ExecutionPolicy Bypass -File installer\build_installer_exe.ps1
.\INSTALAR` POKEDEX.exe --self-test
```

Se utiliza el compilador .NET Framework incluido en Windows. El binario no está
firmado; una distribución pública puede activar SmartScreen hasta que el proyecto
disponga de un certificado de firma de código.

## Limpieza

La limpieza voluntaria solo elimina:

- `%LOCALAPPDATA%\PokedexESP32\downloads`;
- builds bajo `.tools`;
- el ZIP temporal de narraciones.

Conserva el core instalado para reparaciones, el backend, Python, Ollama, el
modelo, los accesos directos y todos los datos necesarios para el funcionamiento.
