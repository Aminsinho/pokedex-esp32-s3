@echo off
setlocal
cd /d "%~dp0"
title Instalador ESP32-CAM experimental

echo ============================================================
echo          ESP32-CAM EXPERIMENTAL - POKEDEX
echo ============================================================
echo.
echo Ejecuta primero INSTALAR POKEDEX.bat al menos una vez.
echo Desconecta la Pokedex, conecta la ESP32-CAM por USB y continua.
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\instalar_esp32cam.ps1"
set "RESULT=%ERRORLEVEL%"
echo.
if "%RESULT%"=="0" (
  echo ESP32-CAM instalada correctamente.
) else (
  echo La instalacion no termino. Revisa el mensaje anterior.
)
pause
exit /b %RESULT%
