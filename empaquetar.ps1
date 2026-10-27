# Arma el paquete para publicar: Brizna-<versión>-x64.zip con el programa, sus traducciones y las
# licencias de terceros, más SHA256SUMS.txt con su huella. Se usa después de compilar.ps1.
#
# Uso: powershell -ExecutionPolicy Bypass -File empaquetar.ps1 -Salida C:\carpeta\de\salida
param(
    [Parameter(Mandatory = $true)][string]$Salida,
    [string]$Build
)

$ErrorActionPreference = 'Stop'
if (-not $Build) { $Build = Join-Path $PSScriptRoot 'build-x64' }
$version = (Get-Content (Join-Path $PSScriptRoot 'version.txt') -TotalCount 1).Trim()
$release = Join-Path $Build 'Release'
$archivos = 'Brizna.exe', 'coredb.sqlite', 'AVISOS-DE-TERCEROS.txt' | ForEach-Object { Join-Path $release $_ }
foreach ($f in $archivos) { if (-not (Test-Path $f)) { throw "Falta $f (primero hay que compilar)" } }

New-Item -ItemType Directory -Force $Salida | Out-Null
$zip = Join-Path $Salida "Brizna-$version-x64.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $archivos -DestinationPath $zip

$huella = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
"$huella  $(Split-Path $zip -Leaf)" | Set-Content -Encoding ascii (Join-Path $Salida 'SHA256SUMS.txt')
Write-Host "Paquete: $zip"
Write-Host "SHA256:  $huella"
