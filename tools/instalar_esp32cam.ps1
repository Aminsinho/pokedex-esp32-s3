param(
    [string]$Port,
    [string]$Ssid,
    [string]$Password,
    [string]$BackendHost,
    [switch]$SkipUpload,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot ".tools"
$sketch = Join-Path $projectRoot "experimental\esp32-cam"
$privateConfig = Join-Path $sketch "camera_config.h"
$arduino = Join-Path $toolsRoot "arduino-cli\arduino-cli.exe"
$arduinoConfig = Join-Path $toolsRoot "arduino-cli.yaml"
$fqbn = "esp32:esp32:esp32cam"

function Escape-CString([string]$Value) {
    return $Value.Replace('\', '\\').Replace('"', '\"')
}

function Run([string]$File, [string[]]$Arguments, [string]$Description) {
    Write-Host "  $Description..." -NoNewline
    if ($DryRun) {
        Write-Host " OMITIDO (simulacion)" -ForegroundColor Yellow
        return
    }
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Description fallo con codigo $LASTEXITCODE." }
    Write-Host " OK" -ForegroundColor Green
}

function Suggested-IPv4 {
    return Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object {
            $_.IPAddress -notlike "127.*" -and
            $_.IPAddress -notlike "169.254.*" -and
            $_.InterfaceAlias -notmatch "Loopback|vEthernet|Virtual|VPN"
        } |
        Sort-Object InterfaceMetric |
        Select-Object -First 1 -ExpandProperty IPAddress
}

function Configure-Camera {
    if ($DryRun) { return }
    if ([string]::IsNullOrWhiteSpace($Ssid)) { $script:Ssid = Read-Host "Nombre de la red Wi-Fi (SSID)" }
    $suggested = Suggested-IPv4
    if ([string]::IsNullOrWhiteSpace($BackendHost)) {
        $script:BackendHost = Read-Host "IPv4 de este PC, donde se ejecutara el backend [$suggested]"
        if ([string]::IsNullOrWhiteSpace($BackendHost)) { $script:BackendHost = $suggested }
    }
    if ([string]::IsNullOrEmpty($Password)) {
        $secure = Read-Host "Contrasena Wi-Fi" -AsSecureString
        $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        try { $script:Password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
    }
    if ([string]::IsNullOrWhiteSpace($Ssid)) { throw "El SSID no puede estar vacio." }
    if ($BackendHost -notmatch '^\d{1,3}(\.\d{1,3}){3}$') { throw "La direccion del backend debe ser una IPv4." }

    $template = Get-Content -Raw (Join-Path $sketch "camera_config.example.h")
    $template = $template.Replace('TU_RED_WIFI', (Escape-CString $Ssid))
    $template = $template.Replace('TU_CONTRASENA_WIFI', (Escape-CString $Password))
    $template = $template.Replace('192.168.1.100', (Escape-CString $BackendHost))
    Set-Content -LiteralPath $privateConfig -Value $template -Encoding utf8
    $script:Password = $null
}

function Find-Port {
    if ($Port) { return $Port.ToUpperInvariant() }
    if ($DryRun) { return "COM_TEST" }
    $ports = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
        Where-Object { $_.DeviceID -ne "COM1" } |
        Sort-Object DeviceID
    if ($ports.Count -eq 1) { return $ports[0].DeviceID }
    if ($ports.Count -gt 0) { $ports | ForEach-Object { Write-Host "  $($_.DeviceID) - $($_.Name)" } }
    $chosen = Read-Host "Puerto de la ESP32-CAM, por ejemplo COM5"
    if ($chosen -notmatch '^COM\d+$') { throw "Puerto no valido." }
    return $chosen.ToUpperInvariant()
}

try {
    Write-Host "ESP32-CAM EXPERIMENTAL (GC2145)" -ForegroundColor White -BackgroundColor DarkRed
    if (-not $DryRun -and (-not (Test-Path $arduino) -or -not (Test-Path $arduinoConfig))) {
        throw "Ejecuta primero INSTALAR POKEDEX.bat para preparar Arduino y vuelve a intentarlo."
    }
    Configure-Camera
    $serialPort = Find-Port
    $build = Join-Path $toolsRoot "esp32cam-build"
    Run $arduino @("compile", $sketch, "--fqbn", $fqbn, "--build-path", $build, "--config-file", $arduinoConfig) "Compilar ESP32-CAM"
    if (-not $SkipUpload) {
        Run $arduino @("upload", $sketch, "--port", $serialPort, "--fqbn", $fqbn, "--input-dir", $build, "--config-file", $arduinoConfig) "Cargar firmware en $serialPort"
    }
    Write-Host "`nListo. Inicia el backend y pulsa SCAN en la Pokedex." -ForegroundColor Green
    Write-Host "La configuracion privada queda solo en experimental\esp32-cam\camera_config.h."
    exit 0
} catch {
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
