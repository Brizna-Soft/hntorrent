# Arma el paquete para publicar: Brizna-<versión>-x64.zip con el programa, sus traducciones y las
# licencias de terceros, más SHA256SUMS.txt con su huella. Se usa después de compilar.ps1.
#
# Con -ClavePrivada además firma la versión y escribe actualizacion.json, que es lo que lee el
# actualizador de Brizna. La firma (Ed25519) cubre a la vez el número de versión y la huella del .zip.
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
$archivos = 'Brizna.exe', 'coredb.sqlite', 'AVISOS-DE-TERCEROS.txt' | ForEach-Object { Join-Path $release $_ }
foreach ($f in $archivos) { if (-not (Test-Path $f)) { throw "Falta $f (primero hay que compilar)" } }

New-Item -ItemType Directory -Force $Salida | Out-Null
$nombreZip = "Brizna-$version-x64.zip"
$zip = Join-Path $Salida $nombreZip
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $archivos -DestinationPath $zip

$huella = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
"$huella  $nombreZip" | Set-Content -Encoding ascii (Join-Path $Salida 'SHA256SUMS.txt')
Copy-Item (Join-Path $release 'AVISOS-DE-TERCEROS.txt') $Salida -Force
Write-Host "Paquete: $zip"
Write-Host "SHA256:  $huella"

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
    $mensaje = Join-Path $tmp 'mensaje.txt'
    $firma = Join-Path $tmp 'firma.bin'
    $publica = Join-Path $tmp 'publica.pem'
    # Mismo texto que arma Brizna en updatechecker.cpp, byte por byte
    [IO.File]::WriteAllBytes($mensaje, [Text.Encoding]::ASCII.GetBytes("brizna-actualizacion-v1`nversion=$version`nsha256=$huella`n"))
    & $openssl pkeyutl -sign -inkey $ClavePrivada -rawin -in $mensaje -out $firma
    if ($LASTEXITCODE -ne 0) { throw 'openssl no pudo firmar' }
    & $openssl pkey -in $ClavePrivada -pubout -out $publica
    & $openssl pkeyutl -verify -pubin -inkey $publica -rawin -in $mensaje -sigfile $firma | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'La firma recién hecha no se pudo comprobar' }
    $firmaHex = (([IO.File]::ReadAllBytes($firma)) | ForEach-Object { $_.ToString('x2') }) -join ''
} finally {
    Remove-Item -Recurse -Force $tmp
}

$manifiesto = [ordered]@{
    formato = 1
    version = $version
    archivo = $nombreZip
    sha256  = $huella
    firma   = $firmaHex
}
[IO.File]::WriteAllText((Join-Path $Salida 'actualizacion.json'), ($manifiesto | ConvertTo-Json), (New-Object System.Text.UTF8Encoding $false))
Write-Host "Firmado: actualizacion.json (Ed25519, $($firmaHex.Length / 2) bytes)"
