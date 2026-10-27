# Compila Brizna (nuestra versión de PicoTorrent) y la deja lista en ..\programa
#
# Uso (desde cualquier carpeta):
#   powershell -ExecutionPolicy Bypass -File compilar.ps1
#
# Opciones:
#   -Limpiar      borra la carpeta build-x64 antes de compilar (compilación desde cero)
#   -SinCopiar    compila pero no copia el resultado a ..\programa
#   -Publicacion  versión limpia (la de version.txt, sin "-local"); la usa GitHub Actions al publicar
#
# También funciona en GitHub Actions: si no está el Git portátil usa el git del sistema, y respeta
# VCPKG_DEFAULT_BINARY_CACHE y VCPKG_DOWNLOADS si ya vienen puestos.
param(
    [switch]$Limpiar,
    [switch]$SinCopiar,
    [switch]$Publicacion
)

$ErrorActionPreference = 'Stop'
$codigo = $PSScriptRoot
$raiz = Split-Path $codigo -Parent
$build = Join-Path $codigo 'build-x64'
$programa = Join-Path $raiz 'programa'

function Paso($texto) { Write-Host "`n=== $texto ===" -ForegroundColor Cyan }

# --- 1. Herramientas: Git portátil y Visual Studio 2022 Build Tools ---
Paso 'Buscando herramientas'
$gitDir = Join-Path $raiz 'herramientas\git\cmd'
if (Test-Path (Join-Path $gitDir 'git.exe')) { $env:PATH = "$gitDir;$env:PATH" }
elseif (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "No encuentro Git (ni el portátil en $gitDir ni uno del sistema)" }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'No encuentro Visual Studio 2022 Build Tools (falta vswhere.exe)' }
$vs = & $vswhere -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath
if (-not $vs) { throw 'No encuentro Visual Studio 2022 con las herramientas de C++' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { throw "No encuentro CMake de Visual Studio en $cmake" }
Write-Host "Visual Studio: $vs"
& $cmake --version | Select-Object -First 1

# --- 2. vcpkg: cachés dentro de nuestra carpeta y sin telemetría ---
if (-not $env:VCPKG_DEFAULT_BINARY_CACHE) { $env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $raiz 'cache\vcpkg-binarios' }
if (-not $env:VCPKG_DOWNLOADS) { $env:VCPKG_DOWNLOADS = Join-Path $raiz 'cache\vcpkg-descargas' }
New-Item -ItemType Directory -Force $env:VCPKG_DEFAULT_BINARY_CACHE, $env:VCPKG_DOWNLOADS | Out-Null
$env:VCPKG_DISABLE_METRICS = '1'

# --- 3. Versión: la de version.txt + commit actual ---
Paso 'Calculando la versión'
$sha = (& git -C $codigo rev-parse --short=7 HEAD).Trim()
$rama = (& git -C $codigo rev-parse --abbrev-ref HEAD).Trim()
$version = (Get-Content (Join-Path $codigo 'version.txt') -TotalCount 1).Trim()
if ($version -notmatch '^(\d+)\.(\d+)\.(\d+)$') { throw "version.txt debe tener el formato 1.2.3 (tiene '$version')" }
$mayor = [int]$Matches[1]; $menor = [int]$Matches[2]; $parche = [int]$Matches[3]
$semver = if ($Publicacion) { $version } else { "$version-local+$sha" }
Write-Host "Versión: $semver (rama $rama)"

# --- 4. Configurar con CMake (aquí vcpkg baja y compila las bibliotecas) ---
if ($Limpiar -and (Test-Path $build)) {
    Paso 'Borrando la compilación anterior'
    Remove-Item -Recurse -Force $build
}

Paso 'Configurando (la primera vez tarda: vcpkg compila Boost, OpenSSL y libtorrent)'
& $cmake -S $codigo -B $build -G 'Visual Studio 17 2022' -A x64 -T v143 `
    "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md-rel" `
    "-DVCPKG_INSTALL_OPTIONS=--clean-buildtrees-after-build;--clean-packages-after-build" `
    "-DGITVERSION_VAR_BRANCHNAME=$rama" `
    "-DGITVERSION_VAR_SEMVER=$semver" `
    "-DGITVERSION_VAR_SHORTSHA=$sha" `
    "-DGITVERSION_VAR_VERSION_MAJOR=$mayor" `
    "-DGITVERSION_VAR_VERSION_MINOR=$menor" `
    "-DGITVERSION_VAR_VERSION_PATCH=$parche" `
    "-DGITVERSION_VAR_VERSION=$version"
if ($LASTEXITCODE -ne 0) { throw "Falló la configuración con CMake (código $LASTEXITCODE)" }

# --- 5. Compilar ---
Paso 'Compilando Brizna'
& $cmake --build $build --config Release --target PicoTorrent --parallel
if ($LASTEXITCODE -ne 0) { throw "Falló la compilación (código $LASTEXITCODE)" }

$exe = Join-Path $build 'Release\Brizna.exe'
$coredb = Join-Path $build 'Release\coredb.sqlite'
$avisos = Join-Path $build 'Release\AVISOS-DE-TERCEROS.txt'
foreach ($f in $exe, $coredb) { if (-not (Test-Path $f)) { throw "No se generó $f" } }

Paso 'Armando los avisos de licencias de terceros'
& (Join-Path $codigo 'generar-avisos.ps1') -Build $build -Salida $avisos -Version $semver

# --- 6. Copiar a ..\programa ---
if (-not $SinCopiar) {
    Paso "Copiando a $programa"
    if (Get-Process Brizna -ErrorAction SilentlyContinue) { throw 'Cierra Brizna antes de copiar la versión nueva' }
    New-Item -ItemType Directory -Force $programa | Out-Null
    # Guardar la versión que estaba antes, por si la nueva falla (tus torrents y ajustes no se tocan)
    $anterior = Join-Path $raiz 'programa-anterior'
    if (Test-Path (Join-Path $programa 'Brizna.exe')) {
        New-Item -ItemType Directory -Force $anterior | Out-Null
        Copy-Item (Join-Path $programa 'Brizna.exe'), (Join-Path $programa 'coredb.sqlite') $anterior -Force -ErrorAction SilentlyContinue
        Write-Host "Versión anterior guardada en $anterior"
    }
    Copy-Item $exe, $coredb, $avisos $programa -Force
}

Paso 'Listo'
Write-Host "Brizna $semver compilado." -ForegroundColor Green
if (-not $SinCopiar) { Write-Host "Programa: $(Join-Path $programa 'Brizna.exe')" -ForegroundColor Green }
