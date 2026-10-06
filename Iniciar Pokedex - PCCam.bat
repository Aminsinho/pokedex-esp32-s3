@echo off
setlocal
cd /d "%~dp0"
title Pokedex - Backend y camara del PC

set "BACKEND=%CD%\backend"
set "PYTHON=%BACKEND%\.venv\Scripts\python.exe"

where python >nul 2>nul
if errorlevel 1 (
  echo ERROR: Python 3 no esta instalado o no aparece en PATH.
  pause
  exit /b 1
)

if not exist "%PYTHON%" (
  echo Primera ejecucion: creando el entorno e instalando dependencias...
  python -m venv "%BACKEND%\.venv"
  if errorlevel 1 goto :error
  "%PYTHON%" -m pip install --upgrade pip
  if errorlevel 1 goto :error
  "%PYTHON%" -m pip install -r "%BACKEND%\requirements-camera.txt"
  if errorlevel 1 goto :error
)

where ollama >nul 2>nul
if errorlevel 1 (
  echo ERROR: instala Ollama desde https://ollama.com/download/windows
  pause
  exit /b 1
)

ollama show qwen3-vl:8b-instruct >nul 2>nul
if errorlevel 1 (
  echo Falta el modelo de vision. Ejecutando: ollama pull qwen3-vl:8b-instruct
  ollama pull qwen3-vl:8b-instruct
  if errorlevel 1 goto :error
)

powershell -NoProfile -Command "if (-not (Get-NetTCPConnection -LocalPort 11434 -State Listen -ErrorAction SilentlyContinue)) { Start-Process -FilePath 'ollama' -ArgumentList 'serve' -WindowStyle Hidden }"

set "POKEDEX_CAMERA_DEVICE=pokedex-camera-01"
set "POKEDEX_RETAIN_IMAGES=0"
start "Pokedex Backend" /min /d "%BACKEND%" "%PYTHON%" -m uvicorn app.main:app --host 0.0.0.0 --port 8000
timeout /t 3 /nobreak >nul
start "Pokedex Camara PC" /min /d "%BACKEND%" "%PYTHON%" camera_pc.py --index 0

echo.
echo Pokedex iniciada. Mantén este PC y el dispositivo en la misma red.
echo Backend: http://127.0.0.1:8000/docs
exit /b 0

:error
echo ERROR: no se pudo completar la instalacion o el arranque.
pause
exit /b 1
