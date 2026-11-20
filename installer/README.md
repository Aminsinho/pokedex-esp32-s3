# Instalador gráfico

`INSTALAR POKEDEX.exe` es una aplicación WinForms pequeña que acompaña al ZIP y
reutiliza `tools/instalar_pokedex.ps1` como motor. No contiene firmware ni
credenciales embebidas.

Funciones:

- asistente paso a paso: bienvenida, conexión de la placa, Wi-Fi, contenido e instalación;
- comprueba el espacio libre en disco antes de empezar;
- detecta la Pokédex automáticamente al conectarla (prioriza VID/PID de Espressif, CH340 y
  CP210x) y da consejos si no aparece (cable de datos, puerto, programas que usan la placa);
- propone la red Wi-Fi actual del PC, avisa de que la placa solo admite 2,4 GHz y detecta la IP
  del PC (editable en «Opciones avanzadas»); reutiliza `wifi_config.h` en reinstalaciones;
- configura Wi-Fi y backend sin exponer la contraseña en argumentos de procesos;
- muestra el progreso por fases con una barra general; el registro técnico es opcional;
- si algo falla, explica el problema en lenguaje sencillo y qué hacer, y permite reintentar,
  copiar los detalles o abrir el registro (`.installer/instalador-gui.log`);
- al terminar ofrece iniciar la Pokédex y el instalador ESP32-CAM.

Opciones para pruebas: `--self-test`, `--capture <carpeta>` (capturas de cada pantalla) y
`--auto COMx` (instalación Kanto desatendida con la configuración Wi-Fi existente).

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
