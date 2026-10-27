# Pasa vcpkg (de donde salen Boost, OpenSSL, fmt, SQLite y nlohmann-json) a su última versión publicada.
#
# Uso:
#   powershell -ExecutionPolicy Bypass -File actualizar-vcpkg.ps1                 (busca la última versión y la aplica)
#   powershell -ExecutionPolicy Bypass -File actualizar-vcpkg.ps1 -SoloRevisar    (solo dice si hay una nueva)
#   powershell -ExecutionPolicy Bypass -File actualizar-vcpkg.ps1 -Version 2026.10.16
#
# libtorrent no cambia con esto: la elige nuestro port propio (actualizar-libtorrent.ps1).
# No compila: después hay que ejecutar compilar.ps1.
param(
    [string]$Version,
    [switch]$SoloRevisar
)

$ErrorActionPreference = 'Stop'
$codigo = $PSScriptRoot
$git = Join-Path (Split-Path $codigo -Parent) 'herramientas\git\cmd\git.exe'
if (-not (Test-Path $git)) { $git = 'git' }   # en GitHub Actions se usa el git del sistema
$vcpkg = Join-Path $codigo 'vendor\vcpkg'
$api = @{ 'User-Agent' = 'picotorrent-propio' }
if ($env:GITHUB_TOKEN) { $api['Authorization'] = "Bearer $env:GITHUB_TOKEN" }

if (-not $Version) {
    $ultima = Invoke-RestMethod -Uri 'https://api.github.com/repos/microsoft/vcpkg/releases/latest' -Headers $api
    $Version = $ultima.tag_name
    Write-Host ("Última versión publicada de vcpkg: {0} ({1:yyyy-MM-dd})" -f $Version, [datetime]$ultima.published_at)
}

# Se compara por commit, así funciona aunque la copia de vcpkg no tenga las etiquetas (como en GitHub Actions)
$commitNuevo = (Invoke-RestMethod -Uri "https://api.github.com/repos/microsoft/vcpkg/commits/$Version" -Headers $api).sha
$commitActual = (& $git -C $vcpkg rev-parse HEAD).Trim()
# Sin etiquetas, git avisa por stderr; en PowerShell 5.1 eso sería un error si no se relaja la preferencia
$eap = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
$actual = (& $git -C $vcpkg describe --tags --exact-match 2>$null)
$ErrorActionPreference = $eap
if (-not $actual) { $actual = $commitActual.Substring(0, 10) }
Write-Host "Versión que usa nuestra compilación: $actual"
if ($commitActual -eq $commitNuevo) { Write-Host 'No hay nada que actualizar.' -ForegroundColor Green; return }
if ($SoloRevisar) { Write-Host "Hay una versión nueva: $Version" -ForegroundColor Yellow; return }

& $git -C $vcpkg fetch --depth 1 origin tag $Version
if ($LASTEXITCODE -ne 0) { throw "No pude bajar la versión $Version de vcpkg" }
& $git -C $vcpkg -c advice.detachedHead=false checkout -q $Version
if ($LASTEXITCODE -ne 0) { throw "No pude cambiar vcpkg a $Version" }

Write-Host "`nListo: vcpkg $actual -> $Version" -ForegroundColor Green
Write-Host 'Ahora ejecuta compilar.ps1 (va a recompilar las bibliotecas que cambiaron) y prueba el programa.'
