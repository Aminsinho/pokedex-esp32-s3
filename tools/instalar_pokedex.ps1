param(
    [string]$Port,
    [switch]$Kanto,
    [switch]$Full,
    [switch]$SkipData,
    [switch]$SkipOllama,
    [switch]$SkipUpload,
    [switch]$ReuseWifi,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot ".tools"
$installerRoot = Join-Path $projectRoot ".installer"
$logPath = Join-Path $installerRoot "instalacion.log"
$fqbn = "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,USBMode=hwcdc,CDCOnBoot=cdc"
$arduinoVersion = "3.3.11"
$librarySpecs = @(
    "TFT_eSPI@2.5.43",
    "lvgl@8.4.0",
    "ArduinoJson@7.4.3"
)

New-Item -ItemType Directory -Force -Path $toolsRoot, $installerRoot | Out-Null
Start-Transcript -LiteralPath $logPath -Append | Out-Null

function Section([string]$Text) {
    Write-Host "`n== $Text ==" -ForegroundColor Cyan
}

function Run([string]$File, [string[]]$Arguments, [string]$Description) {
    Write-Host "  $Description..." -NoNewline
    if ($DryRun) {
        Write-Host " OMITIDO (simulacion)" -ForegroundColor Yellow
        return
    }
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description fallo con codigo $LASTEXITCODE."
    }
    Write-Host " OK" -ForegroundColor Green
}

function Find-Python {
    $candidates = @()
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        try {
            $resolved = & $py.Source -3 -c "import sys; print(sys.executable)" 2>$null
            if ($LASTEXITCODE -eq 0) { $candidates += $resolved }
        } catch {}
    }
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) { $candidates += $python.Source }
    $candidates += Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -ExpandProperty FullName
    return $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
}

function Ensure-Python {
    $python = Find-Python
    if ($python) { return $python }
    if ($DryRun) { return "python" }
    $winget = Get-Command winget -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "Falta Python 3 y Windows Package Manager. Instala Python 3.12 desde python.org y repite."
    }
    Run $winget.Source @("install", "--id", "Python.Python.3.12", "-e", "--scope", "user", "--accept-package-agreements", "--accept-source-agreements") "Instalar Python 3.12"
    $python = Find-Python
    if (-not $python) { throw "Python se instalo, pero Windows aun no lo encuentra. Cierra y vuelve a abrir el instalador." }
    return $python
}

function Ensure-ArduinoCli {
    $bundled = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    $local = Join-Path $toolsRoot "arduino-cli\arduino-cli.exe"
    $command = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if (Test-Path -LiteralPath $bundled) { return $bundled }
    if (Test-Path -LiteralPath $local) { return $local }
    if ($DryRun) { return "arduino-cli" }

    Section "Descargando Arduino CLI"
    $release = Invoke-RestMethod "https://api.github.com/repos/arduino/arduino-cli/releases/latest"
    $asset = $release.assets | Where-Object { $_.name -match 'Windows_64bit\.zip$' } | Select-Object -First 1
    if (-not $asset) { throw "No se encontro el paquete oficial de Arduino CLI para Windows." }
    $zip = Join-Path $toolsRoot "arduino-cli.zip"
    $destination = Split-Path -Parent $local
    Invoke-WebRequest $asset.browser_download_url -OutFile $zip
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Expand-Archive -LiteralPath $zip -DestinationPath $destination -Force
    if (-not (Test-Path -LiteralPath $local)) { throw "Arduino CLI no se extrajo correctamente." }
    return $local
}

function Write-ArduinoConfig {
    $config = Join-Path $toolsRoot "arduino-cli.yaml"
    $data = (Join-Path $toolsRoot "arduino-data").Replace('\', '/')
    $downloads = (Join-Path $toolsRoot "arduino-downloads").Replace('\', '/')
    $user = (Join-Path $toolsRoot "arduino-user").Replace('\', '/')
    $yaml = @"
board_manager:
  additional_urls:
    - https://espressif.github.io/arduino-esp32/package_esp32_index.json
directories:
  data: $data
  downloads: $downloads
  user: $user
"@
    Set-Content -LiteralPath $config -Value $yaml -Encoding utf8
    return $config
}

function Install-ArduinoToolchain([string]$Cli, [string]$Config) {
    Section "Preparando Arduino y ESP32"
    Run $Cli @("core", "update-index", "--config-file", $Config) "Actualizar indice Arduino"
    Run $Cli @("core", "install", "esp32:esp32@$arduinoVersion", "--config-file", $Config) "Instalar soporte ESP32 $arduinoVersion"
    foreach ($library in $librarySpecs) {
        Run $Cli @("lib", "install", $library, "--config-file", $Config) "Instalar $library"
    }

    if ($DryRun) { return }
    $libraryRoot = Join-Path $toolsRoot "arduino-user\libraries"
    $tft = Join-Path $libraryRoot "TFT_eSPI"
    $setups = Join-Path $libraryRoot "TFT_eSPI_Setups"
    $touch = Join-Path $libraryRoot "FT6336U_CTP_Controller"
    if (-not (Test-Path -LiteralPath $tft)) { throw "No se encontro TFT_eSPI despues de instalarla." }
    New-Item -ItemType Directory -Force -Path $setups | Out-Null
    Copy-Item -Path (Join-Path $projectRoot "docs\tft_setups\TFT_eSPI_Setups\*.h") -Destination $setups -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot "installer\TFT_User_Setup_Select.h") -Destination (Join-Path $tft "User_Setup_Select.h") -Force
    New-Item -ItemType Directory -Force -Path $touch | Out-Null
    Copy-Item -Path (Join-Path $projectRoot "third_party\FT6336U_CTP_Controller\*") -Destination $touch -Recurse -Force
}

function Find-SerialPort {
    if ($Port) { return $Port.ToUpperInvariant() }
    if ($DryRun) { return "COM_TEST" }

    Section "Buscando la Pokedex por USB"
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        $serial = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
            Where-Object { $_.DeviceID -ne "COM1" } |
            Sort-Object @{ Expression = { if ($_.Name -match 'USB|JTAG|ESP32|CP210|CH340') { 0 } else { 1 } } }, DeviceID
        if ($serial.Count -eq 1) {
            Write-Host "  Detectada: $($serial[0].Name)" -ForegroundColor Green
            return $serial[0].DeviceID
        }
        if ($serial.Count -gt 1) {
            Write-Host "Se encontraron varios puertos:"
            $serial | ForEach-Object { Write-Host "  $($_.DeviceID) - $($_.Name)" }
            $chosen = Read-Host "Escribe el puerto de la Pokedex, por ejemplo COM4"
            if ($chosen -match '^COM\d+$') { return $chosen.ToUpperInvariant() }
        }
        if ($attempt -eq 0) { Write-Host "  Conectala ahora; esperando hasta 60 segundos..." }
        Start-Sleep -Seconds 2
    }
    throw "No se detecto la Pokedex. Comprueba que el cable USB permite datos y vuelve a intentarlo."
}

function Wait-ForPort([string]$ExpectedPort) {
    if ($DryRun) { return }
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        if ([System.IO.Ports.SerialPort]::GetPortNames() -contains $ExpectedPort) { return }
        Start-Sleep -Seconds 2
    }
    throw "$ExpectedPort no reaparecio despues del reinicio del ESP32-S3."
}

function Configure-Wifi {
    Section "Configurando la red"
    if ($DryRun) {
        Write-Host "  Asistente Wi-Fi validado (simulacion)." -ForegroundColor Yellow
        return
    }
    $wifiConfig = Join-Path $projectRoot "src\pokedex\wifi_config.h"
    if (Test-Path -LiteralPath $wifiConfig) {
        if ($ReuseWifi) {
            Write-Host "  Se conserva la configuracion Wi-Fi existente." -ForegroundColor Green
            return
        }
        $keep = Read-Host "Ya existe una configuracion Wi-Fi. Conservarla? [S/n]"
        if ([string]::IsNullOrWhiteSpace($keep) -or $keep -match '^[sSyY]') {
            Write-Host "  Se conserva la configuracion Wi-Fi existente." -ForegroundColor Green
            return
        }
    }
    & (Join-Path $PSScriptRoot "configurar_wifi.ps1")
    if (-not (Test-Path -LiteralPath $wifiConfig)) {
        throw "No se creo la configuracion Wi-Fi."
    }
}

function Prepare-Python([string]$Python) {
    Section "Preparando Python"
    # El lanzador diario usa este mismo entorno; no se reinstala Python dos veces.
    $venv = Join-Path $projectRoot "backend\.venv"
    $venvPython = Join-Path $venv "Scripts\python.exe"
    if (-not (Test-Path -LiteralPath $venvPython)) {
        Run $Python @("-m", "venv", $venv) "Crear entorno Python"
    }
    if ($DryRun) { return "python" }
    Run $venvPython @("-m", "pip", "install", "--upgrade", "pip") "Actualizar pip"
    Run $venvPython @("-m", "pip", "install", "-r", (Join-Path $projectRoot "backend\requirements-camera.txt"), "pyserial", "soundfile") "Instalar dependencias Python"
    return $venvPython
}

function Build-And-Upload([string]$Cli, [string]$Config, [string]$SerialPort) {
    Section "Compilando e instalando el firmware"
    $build = Join-Path $toolsRoot "firmware-build"
    Run $Cli @("compile", (Join-Path $projectRoot "src\pokedex"), "--fqbn", $fqbn, "--build-path", $build, "--config-file", $Config) "Compilar firmware"
    if ($SkipUpload) {
        Write-Host "  Carga omitida por parametro; el firmware compilado queda en $build" -ForegroundColor Yellow
        return
    }
    Run $Cli @("upload", (Join-Path $projectRoot "src\pokedex"), "--port", $SerialPort, "--fqbn", $fqbn, "--input-dir", $build, "--config-file", $Config) "Cargar firmware en $SerialPort"
    Wait-ForPort $SerialPort
}

function Select-Scope {
    if ($Kanto) { return 151 }
    if ($Full) { return 1025 }
    Write-Host "`nDatos que se instalaran en la microSD:"
    Write-Host "  1. Completo: 1025 Pokemon (recomendado; puede tardar bastante)"
    Write-Host "  2. Rapido: Kanto, 151 Pokemon"
    $choice = Read-Host "Elige 1 o 2 [1]"
    if ($choice -eq "2") { return 151 }
    return 1025
}

function Prepare-And-SendData([string]$Python, [string]$SerialPort, [int]$LastId) {
    if ($SkipData) { return }
    Section "Preparando datos para la microSD"
    Run $Python @((Join-Path $projectRoot "tools\generate_local_db.py")) "Generar catalogo local"
    Run $Python @((Join-Path $projectRoot "tools\generate_mechanics.py"), "--first", "1", "--last", "$LastId") "Generar evoluciones y ataques"
    Run $Python @((Join-Path $projectRoot "tools\prepare_evolution_stones.py")) "Preparar iconos evolutivos"

    Section "Copiando datos a la microSD de la Pokedex"
    Wait-ForPort $SerialPort
    Run $Python @((Join-Path $projectRoot "tools\send_to_sd.py"), "--port", $SerialPort) "Copiar fichas"
    Run $Python @((Join-Path $projectRoot "tools\provision_sprites.py"), "--port", $SerialPort, "--first", "1", "--last", "$LastId") "Copiar sprites"
    Run $Python @((Join-Path $projectRoot "tools\provision_data.py"), "--port", $SerialPort, "--mechanics", "--stones") "Copiar mecanicas e iconos"
}

function Find-Ollama {
    $command = Get-Command ollama -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $local = Join-Path $env:LOCALAPPDATA "Programs\Ollama\ollama.exe"
    if (Test-Path -LiteralPath $local) { return $local }
    return $null
}

function Install-Ollama {
    if ($SkipOllama) { return }
    Section "Preparando la inteligencia artificial"
    $ollama = Find-Ollama
    if (-not $ollama) {
        if ($DryRun) { $ollama = "ollama" }
        else {
            $winget = Get-Command winget -ErrorAction SilentlyContinue
            if (-not $winget) { throw "Falta Ollama. Instalalo desde https://ollama.com/download/windows y repite." }
            Run $winget.Source @("install", "--id", "Ollama.Ollama", "-e", "--accept-package-agreements", "--accept-source-agreements") "Instalar Ollama"
            $ollama = Find-Ollama
            if (-not $ollama) { throw "Ollama se instalo, pero aun no aparece. Cierra y vuelve a abrir el instalador." }
        }
    }
    if (-not $DryRun) {
        $list = & $ollama list 2>$null
        if ($LASTEXITCODE -ne 0) {
            $ollamaLog = Join-Path $installerRoot "ollama.log"
            $ollamaError = Join-Path $installerRoot "ollama-error.log"
            Start-Process -FilePath $ollama -ArgumentList "serve" -WindowStyle Hidden -RedirectStandardOutput $ollamaLog -RedirectStandardError $ollamaError
            for ($attempt = 0; $attempt -lt 30; $attempt++) {
                Start-Sleep -Seconds 1
                try {
                    Invoke-RestMethod "http://127.0.0.1:11434/api/tags" -TimeoutSec 2 | Out-Null
                    break
                } catch {
                    if ($attempt -eq 29) { throw "Ollama se instalo, pero su servicio no pudo iniciarse." }
                }
            }
            $list = & $ollama list 2>$null
        }
        if ($list -notmatch 'qwen3-vl:8b-instruct') {
            Run $ollama @("pull", "qwen3-vl:8b-instruct") "Descargar modelo visual (varios GB)"
        } else {
            Write-Host "  Modelo visual ya instalado." -ForegroundColor Green
        }
    }
}

function Create-Shortcut {
    if ($DryRun) { return }
    $desktop = [Environment]::GetFolderPath("Desktop")
    $shortcutPath = Join-Path $desktop "Iniciar Pokedex.lnk"
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = Join-Path $projectRoot "Iniciar Pokedex - PCCam.bat"
    $shortcut.WorkingDirectory = $projectRoot
    $shortcut.Description = "Iniciar backend, camara e IA de la Pokedex"
    $shortcut.Save()
}

try {
    Write-Host "POKEDEX ESP32-S3 - INSTALACION AUTOMATICA" -ForegroundColor White -BackgroundColor DarkRed
    Write-Host "No cierres esta ventana. El modo completo descarga varios GB y puede tardar."

    foreach ($required in @(
        "src\pokedex\pokedex.ino",
        "src\pokedex\wifi_config.example.h",
        "backend\requirements-camera.txt",
        "installer\TFT_User_Setup_Select.h",
        "third_party\FT6336U_CTP_Controller\library.properties"
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $required))) {
            throw "Falta $required. Extrae o clona el proyecto completo antes de instalar."
        }
    }

    $lastId = Select-Scope
    $python = Ensure-Python
    $arduino = Ensure-ArduinoCli
    $arduinoConfig = Write-ArduinoConfig
    Install-ArduinoToolchain $arduino $arduinoConfig
    $venvPython = Prepare-Python $python
    Configure-Wifi
    $serialPort = if ($SkipUpload) { "SIN_PUERTO" } else { Find-SerialPort }
    Build-And-Upload $arduino $arduinoConfig $serialPort
    if ($SkipUpload -and -not $SkipData) { throw "-SkipUpload requiere tambien -SkipData." }
    Prepare-And-SendData $venvPython $serialPort $lastId
    Install-Ollama
    Create-Shortcut

    Section "Instalacion completada"
    if ($SkipUpload) { Write-Host "Firmware compilado; carga omitida." -ForegroundColor Yellow }
    else { Write-Host "Firmware instalado en $serialPort." -ForegroundColor Green }
    if (-not $SkipData) { Write-Host "Datos instalados: Pokemon #001-$LastId." -ForegroundColor Green }
    Write-Host "Usa el acceso directo 'Iniciar Pokedex' del escritorio para activar camara e IA."
    Write-Host "Registro completo: $logPath"
    Stop-Transcript | Out-Null
    exit 0
} catch {
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Registro: $logPath" -ForegroundColor Yellow
    try { Stop-Transcript | Out-Null } catch {}
    exit 1
}
