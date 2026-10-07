@echo off
setlocal
cd /d "%~dp0"
title Pokedex - Backend y ESP32-CAM

set "BACKEND=%CD%\backend"
set "PYTHON=%BACKEND%\.venv\Scripts\python.exe"
if not exist "%PYTHON%" (
  echo ERROR: ejecuta primero INSTALAR POKEDEX.bat.
  pause
  exit /b 1
)

where ollama >nul 2>nul
if errorlevel 1 (
  echo ERROR: Ollama no esta instalado.
  pause
  exit /b 1
)

powershell -NoProfile -Command "if (-not (Get-NetTCPConnection -LocalPort 11434 -State Listen -ErrorAction SilentlyContinue)) { Start-Process -FilePath 'ollama' -ArgumentList 'serve' -WindowStyle Hidden }"
set "POKEDEX_CAMERA_DEVICE=pokedex-camera-esp32-01"
set "POKEDEX_RETAIN_IMAGES=0"
start "Pokedex Backend" /min /d "%BACKEND%" "%PYTHON%" -m uvicorn app.main:app --host 0.0.0.0 --port 8000

echo.
echo Backend e IA iniciados para ESP32-CAM.
echo No se inicia la webcam del PC en este modo.
echo Diagnostico: http://127.0.0.1:8000/docs
exit /b 0
