@echo off
setlocal
cd /d "%~dp0"
title Instalador de Pokedex ESP32-S3

echo ============================================================
echo          INSTALADOR POKEDEX ESP32-S3
echo ============================================================
echo.
echo Conecta la Pokedex por USB e inserta su tarjeta microSD.
echo El asistente instalara las herramientas, el firmware y los datos.
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\instalar_pokedex.ps1"
set "RESULT=%ERRORLEVEL%"
echo.
if "%RESULT%"=="0" (
  echo Instalacion terminada correctamente.
) else (
  echo La instalacion no termino. El mensaje anterior indica que corregir.
)
pause
exit /b %RESULT%
