# Arma los paquetes para publicar, con el programa, sus traducciones y las licencias de terceros,
# más SHA256SUMS.txt con sus huellas. Se usa después de compilar.ps1.
#
#   HNTorrent-<versión>-x64.zip   el paquete normal (HNTorrent.exe)
#   Brizna-<versión>-x64.zip      el mismo programa con el nombre de antes (Brizna.exe), para que las
#                                 copias con Brizna 0.26 o anterior se actualicen solas. Al abrirse,
#                                 el programa se cambia el nombre a HNTorrent.exe y pasa sus datos.
#
# Con -ClavePrivada además firma cada paquete y escribe hntorrent-actualizacion.json (lo lee
# HNTorrent) y actualizacion.json (lo lee Brizna 0.26). La firma (Ed25519) cubre a la vez el número
# de versión y la huella del .zip.
#
# Uso: powershell -ExecutionPolicy Bypass -File empaquetar.ps1 -Salida C:\carpeta [-ClavePrivada clave.pem]
param(
    [Parameter(Mandatory = $true)][string]$Salida,
    [string]$Build,
    [string]$ClavePrivada
)

$ErrorActionPreference = 'Stop'
if (-not $Build) { $Build = Join-Path $PSScriptRoot 'build-x64' }
$version = (Get-Content (Join-Path $PSScriptRoot 'version.txt') -TotalCount 1).Trim()
$release = Join-Path $Build 'Release'
$exe = Join-Path $release 'HNTorrent.exe'
$otros = 'coredb.sqlite', 'AVISOS-DE-TERCEROS.txt' | ForEach-Object { Join-Path $release $_ }
foreach ($f in @($exe) + $otros) { if (-not (Test-Path $f)) { throw "Falta $f (primero hay que compilar)" } }

New-Item -ItemType Directory -Force $Salida | Out-Null
$tmp = Join-Path ([IO.Path]::GetTempPath()) ('paquete-' + [guid]::NewGuid())
New-Item -ItemType Directory $tmp | Out-Null

# Nombre del paquete -> nombre del programa dentro de él, y el manifiesto que lo anuncia
$paquetes = @(
    [ordered]@{ Zip = "HNTorrent-$version-x64.zip"; Exe = 'HNTorrent.exe'; Manifiesto = 'hntorrent-actualizacion.json' },
    [ordered]@{ Zip = "Brizna-$version-x64.zip"; Exe = 'Brizna.exe'; Manifiesto = 'actualizacion.json' }
)

try {
    foreach ($p in $paquetes) {
        $carpeta = Join-Path $tmp $p.Exe
        New-Item -ItemType Directory $carpeta | Out-Null
        Copy-Item $exe (Join-Path $carpeta $p.Exe)
        Copy-Item $otros $carpeta
        $zip = Join-Path $Salida $p.Zip
        if (Test-Path $zip) { Remove-Item $zip }
        Compress-Archive -Path (Join-Path $carpeta '*') -DestinationPath $zip
        $p.Huella = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
        Write-Host "Paquete: $zip"
        Write-Host "SHA256:  $($p.Huella)"
    }
} finally {
    Remove-Item -Recurse -Force $tmp
}

($paquetes | ForEach-Object { "$($_.Huella)  $($_.Zip)" }) | Set-Content -Encoding ascii (Join-Path $Salida 'SHA256SUMS.txt')
Copy-Item (Join-Path $release 'AVISOS-DE-TERCEROS.txt') $Salida -Force

if (-not $ClavePrivada) { return }

# openssl 3 (hace falta para Ed25519 con -rawin): el del sistema, el del Git portátil o el de Git para Windows
$openssl = @(
    (Get-Command openssl -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source),
    (Join-Path (Split-Path $PSScriptRoot -Parent) 'herramientas\git\ucrt64\bin\openssl.exe'),
    'C:\Program Files\Git\usr\bin\openssl.exe'
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $openssl) { throw 'No encuentro openssl para firmar' }

$tmp = Join-Path ([IO.Path]::GetTempPath()) ('firma-' + [guid]::NewGuid())
New-Item -ItemType Directory $tmp | Out-Null
try {
    $publica = Join-Path $tmp 'publica.pem'
    & $openssl pkey -in $ClavePrivada -pubout -out $publica
    if ($LASTEXITCODE -ne 0) { throw 'openssl no pudo leer la clave' }

    foreach ($p in $paquetes) {
        $mensaje = Join-Path $tmp 'mensaje.txt'
        $firma = Join-Path $tmp 'firma.bin'
        # Mismo texto que arma el programa en updatechecker.cpp, byte por byte (el prefijo "brizna"
        # es el del formato original y no cambia)
        [IO.File]::WriteAllBytes($mensaje, [Text.Encoding]::ASCII.GetBytes("brizna-actualizacion-v1`nversion=$version`nsha256=$($p.Huella)`n"))
        & $openssl pkeyutl -sign -inkey $ClavePrivada -rawin -in $mensaje -out $firma
        if ($LASTEXITCODE -ne 0) { throw 'openssl no pudo firmar' }
        & $openssl pkeyutl -verify -pubin -inkey $publica -rawin -in $mensaje -sigfile $firma | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'La firma recién hecha no se pudo comprobar' }
        $firmaHex = (([IO.File]::ReadAllBytes($firma)) | ForEach-Object { $_.ToString('x2') }) -join ''

        $manifiesto = [ordered]@{
            formato = 1
            version = $version
            archivo = $p.Zip
            sha256  = $p.Huella
            firma   = $firmaHex
        }
        [IO.File]::WriteAllText((Join-Path $Salida $p.Manifiesto), ($manifiesto | ConvertTo-Json), (New-Object System.Text.UTF8Encoding $false))
        Write-Host "Firmado: $($p.Manifiesto) (Ed25519, $($firmaHex.Length / 2) bytes)"
    }
} finally {
    Remove-Item -Recurse -Force $tmp
}
