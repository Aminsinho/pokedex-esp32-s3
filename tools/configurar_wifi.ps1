param(
    [string]$Ssid,
    [string]$Password,
    [string]$BackendHost
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$outputPath = Join-Path $projectRoot "src\pokedex\wifi_config.h"

Write-Host "Configuracion privada de red para la Pokedex" -ForegroundColor Cyan
if ([string]::IsNullOrWhiteSpace($Ssid)) {
    $Ssid = Read-Host "Nombre de la red Wi-Fi (SSID)"
}

$suggestedIp = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object {
        $_.IPAddress -notlike "127.*" -and
        $_.IPAddress -notlike "169.254.*" -and
        $_.InterfaceAlias -notmatch "Loopback|vEthernet|Virtual|VPN"
    } |
    Sort-Object InterfaceMetric |
    Select-Object -First 1 -ExpandProperty IPAddress

$ipPrompt = if ($suggestedIp) {
    "IPv4 de este PC, donde se ejecutara el backend [$suggestedIp]"
} else {
    "IPv4 de este PC, donde se ejecutara el backend"
}
if ([string]::IsNullOrWhiteSpace($BackendHost)) {
    $BackendHost = Read-Host $ipPrompt
}
if ([string]::IsNullOrWhiteSpace($BackendHost)) { $BackendHost = $suggestedIp }

if ([string]::IsNullOrWhiteSpace($Ssid)) { throw "El SSID no puede estar vacio." }
if ([string]::IsNullOrWhiteSpace($BackendHost)) { throw "No se pudo determinar la IPv4 del PC." }
if ($BackendHost -notmatch '^\d{1,3}(\.\d{1,3}){3}$') {
    throw "La direccion del backend debe ser una IPv4, por ejemplo 192.168.1.100."
}

if ([string]::IsNullOrEmpty($Password)) {
    $securePassword = Read-Host "Contrasena Wi-Fi" -AsSecureString
    $passwordPtr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
    try {
        $Password = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPtr)
    } finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPtr)
    }
}

function Escape-CString([string]$value) {
    return $value.Replace('\', '\\').Replace('"', '\"')
}

$content = @"
#pragma once

// Generado localmente por Preparar WiFi.bat. No subir a Git.
#define WIFI_SSID      "$(Escape-CString $Ssid)"
#define WIFI_PASSWORD  "$(Escape-CString $Password)"
#define BACKEND_HOST   "$(Escape-CString $BackendHost)"
#define BACKEND_PORT   8000
"@

Set-Content -LiteralPath $outputPath -Value $content -Encoding utf8
$Password = $null
Write-Host "`nConfiguracion creada en src\pokedex\wifi_config.h" -ForegroundColor Green
Write-Host "La Pokedex y este PC deben estar conectados a la misma red." -ForegroundColor Green
