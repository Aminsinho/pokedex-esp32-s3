param(
    [ValidateRange(1, 1025)][int]$PokemonId = 25,
    [string]$Voice = "Microsoft Helena Desktop"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$records = Get-Content -LiteralPath (Join-Path $root "backend/app/data/pokemon.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$pokemon = $records | Where-Object { $_.id -eq $PokemonId } | Select-Object -First 1
if (-not $pokemon) { throw "Pokemon $PokemonId not found" }
$description = $pokemon.description_es
if ([string]::IsNullOrWhiteSpace($description)) { throw "No Spanish description for Pokemon $PokemonId" }
$outDir = Join-Path $root "sd_dataset/audio/narration"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$outFile = Join-Path $outDir ("{0:D4}.wav" -f $PokemonId)
Add-Type -AssemblyName System.Speech
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
$synth.SelectVoice($Voice)
$synth.Rate = 1
$synth.Volume = 100
$format = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(44100, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
$synth.SetOutputToWaveFile($outFile, $format)
$synth.Speak(("{0}. {1}" -f $pokemon.name_es, $description))
$synth.Dispose()
& python (Join-Path $root "tools/canonicalize_wav.py") $outFile
if ($LASTEXITCODE -ne 0) { throw "WAV canonicalization failed" }
Write-Output ("{0}: {1} bytes; voice={2}" -f $outFile, (Get-Item -LiteralPath $outFile).Length, $Voice)
