# Cambia la versión de libtorrent que usa nuestra versión de PicoTorrent.
#
# Uso:
#   powershell -ExecutionPolicy Bypass -File actualizar-libtorrent.ps1                 (busca la última 2.0.x y la aplica)
#   powershell -ExecutionPolicy Bypass -File actualizar-libtorrent.ps1 -SoloRevisar    (solo dice si hay una nueva)
#   powershell -ExecutionPolicy Bypass -File actualizar-libtorrent.ps1 -Version 2.0.16 (una versión concreta)
#
# Qué hace: baja el código de libtorrent desde GitHub (arvidn/libtorrent), calcula su huella SHA512
# y la escribe en vendor\vcpkg-overlays\ports\libtorrent. vcpkg comprueba esa huella al compilar,
# así que si alguien cambiara el archivo en GitHub, la compilación se detendría.
# No compila: después hay que ejecutar compilar.ps1.
param(
    [string]$Version,
    [switch]$SoloRevisar
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$codigo = $PSScriptRoot
$puerto = Join-Path $codigo 'vendor\vcpkg-overlays\ports\libtorrent'
$manifiesto = Join-Path $puerto 'vcpkg.json'
$portfile = Join-Path $puerto 'portfile.cmake'
$api = @{ 'User-Agent' = 'picotorrent-propio' }

$actual = (Get-Content -Raw $manifiesto | ConvertFrom-Json).version

if (-not $Version) {
    $releases = Invoke-RestMethod -Uri 'https://api.github.com/repos/arvidn/libtorrent/releases?per_page=40' -Headers $api
    $ultima = $releases |
        Where-Object { -not $_.prerelease -and -not $_.draft -and $_.tag_name -match '^v2\.0\.\d+$' } |
        Sort-Object { [version]$_.tag_name.TrimStart('v') } -Descending |
        Select-Object -First 1
    if (-not $ultima) { throw 'No encontré ninguna versión 2.0.x publicada en GitHub' }
    $Version = $ultima.tag_name.TrimStart('v')
    Write-Host ("Última libtorrent 2.0.x publicada: {0} ({1:yyyy-MM-dd})" -f $Version, [datetime]$ultima.published_at)
}

Write-Host "Versión que usa nuestra compilación: $actual"
if ($actual -eq $Version) { Write-Host 'No hay nada que actualizar.' -ForegroundColor Green; return }
if ($SoloRevisar) { Write-Host "Hay una versión nueva: $Version" -ForegroundColor Yellow; return }

function Get-HuellaGithub([string]$repo, [string]$ref) {
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ('pico-' + [guid]::NewGuid())
    New-Item -ItemType Directory $tmp | Out-Null
    try {
        $archivo = Join-Path $tmp 'codigo.tar.gz'
        Invoke-WebRequest -Uri "https://github.com/$repo/archive/$ref.tar.gz" -OutFile $archivo -UseBasicParsing
        return (Get-FileHash -Algorithm SHA512 $archivo).Hash.ToLower()
    } finally {
        Remove-Item -Recurse -Force $tmp
    }
}

$texto = Get-Content -Raw $portfile

# 1. Código de libtorrent (REF "v${VERSION}")
Write-Host "Bajando libtorrent v$Version para calcular su huella..."
$huella = Get-HuellaGithub 'arvidn/libtorrent' "v$Version"
$patron = '(REPO\s+arvidn/libtorrent\s+REF\s+"v\$\{VERSION\}"\s+SHA512\s+)[0-9a-f]+'
if ($texto -notmatch $patron) { throw 'No encontré el bloque de libtorrent en portfile.cmake' }
$texto = [regex]::Replace($texto, $patron, { param($m) $m.Groups[1].Value + $huella })

# 2. Partes que libtorrent trae como submódulos: si la versión nueva usa otro commit, también se actualizan
$submodulos = @(
    @{ Repo = 'arvidn/try_signal';                   Carpeta = 'deps';       Nombre = 'try_signal' },
    @{ Repo = 'paullouisageneau/boost-asio-gnutls';  Carpeta = 'deps';       Nombre = 'asio-gnutls' },
    @{ Repo = 'arvidn/libsimulator';                 Carpeta = 'simulation'; Nombre = 'libsimulator' }
)
foreach ($s in $submodulos) {
    $lista = Invoke-RestMethod -Uri ("https://api.github.com/repos/arvidn/libtorrent/contents/{0}?ref=v{1}" -f $s.Carpeta, $Version) -Headers $api
    $nuevoRef = ($lista | Where-Object { $_.name -eq $s.Nombre }).sha
    if (-not $nuevoRef) { throw "No encontré $($s.Carpeta)/$($s.Nombre) en libtorrent v$Version" }
    $patronSub = '(REPO\s+' + [regex]::Escape($s.Repo) + '\s+REF\s+)([0-9a-f]{40})([^\r\n]*)(\r?\n\s+SHA512\s+)([0-9a-f]+)'
    $m = [regex]::Match($texto, $patronSub)
    if (-not $m.Success) { throw "No encontré el bloque de $($s.Repo) en portfile.cmake" }
    if ($m.Groups[2].Value -ne $nuevoRef) {
        Write-Host "  $($s.Nombre) cambió a $nuevoRef, calculando su huella..."
        $huellaSub = Get-HuellaGithub $s.Repo $nuevoRef
        $texto = $texto.Replace($m.Value, $m.Groups[1].Value + $nuevoRef + ' # libtorrent v' + $Version + $m.Groups[4].Value + $huellaSub)
    } else {
        Write-Host "  $($s.Nombre) sin cambios"
    }
}

# 3. Guardar
[IO.File]::WriteAllText($portfile, $texto, (New-Object System.Text.UTF8Encoding $false))
$json = Get-Content -Raw $manifiesto
$json = [regex]::Replace($json, '"version":\s*"[^"]+"', ('"version": "' + $Version + '"'))
[IO.File]::WriteAllText($manifiesto, $json, (New-Object System.Text.UTF8Encoding $false))

Write-Host "`nListo: libtorrent $actual -> $Version" -ForegroundColor Green
Write-Host 'Ahora ejecuta compilar.ps1 y prueba el programa antes de guardar el cambio con git.'
