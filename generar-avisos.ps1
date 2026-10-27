# Arma AVISOS-DE-TERCEROS.txt con la licencia de cada pieza de software que va dentro de Brizna.exe.
# Los textos salen de lo que instaló vcpkg (share\<paquete>\copyright) y de los submódulos (vendor\...),
# así que siempre corresponden a las versiones compiladas. Lo llama compilar.ps1 después de compilar.
param(
    [string]$Build = (Join-Path $PSScriptRoot 'build-x64'),
    [Parameter(Mandatory = $true)][string]$Salida,
    [string]$Version = ''
)

$ErrorActionPreference = 'Stop'
$codigo = $PSScriptRoot
$share = Join-Path $Build 'vcpkg_installed\x64-windows-static-md-rel\share'
$wx = Join-Path $codigo 'vendor\wx'

function Leer([string]$ruta) {
    if (-not (Test-Path $ruta)) { throw "Falta el archivo de licencia: $ruta" }
    $t = [IO.File]::ReadAllText($ruta, [Text.Encoding]::UTF8)
    return ($t -replace "`r`n", "`n" -replace "`r", "`n").Trim("`n", ' ')
}

function Version-Vcpkg([string]$port) {
    $j = Get-Content -Raw -Encoding UTF8 (Join-Path $share "$port\vcpkg.spdx.json") | ConvertFrom-Json
    $p = $j.packages | Where-Object { $_.SPDXID -eq 'SPDXRef-port' } | Select-Object -First 1
    return ($p.versionInfo -replace '#\d+$', '')
}

function Version-Wx {
    $h = Leer (Join-Path $wx 'include\wx\version.h')
    $v = foreach ($n in 'wxMAJOR_VERSION', 'wxMINOR_VERSION', 'wxRELEASE_NUMBER') { [regex]::Match($h, "#define $n\s+(\d+)").Groups[1].Value }
    return ($v -join '.')
}

# Sección "LEGAL ISSUES" del README de libjpeg (IJG): es su licencia
function Licencia-Ijg {
    $lineas = (Leer (Join-Path $wx 'src\jpeg\README')) -split "`n"
    $inicio = -1
    for ($i = 0; $i -lt $lineas.Count - 1; $i++) {
        if ($lineas[$i].Trim() -eq 'LEGAL ISSUES' -and $lineas[$i + 1] -match '^=+\s*$') { $inicio = $i; break }
    }
    if ($inicio -lt 0) { throw 'No encontré LEGAL ISSUES en el README de libjpeg' }
    $fin = $lineas.Count
    for ($i = $inicio + 2; $i -lt $lineas.Count - 1; $i++) {
        if ($lineas[$i].Trim() -and $lineas[$i + 1] -match '^=+\s*$') { $fin = $i; break }
    }
    return (($lineas[$inicio..($fin - 1)]) -join "`n").Trim()
}

$boost = @(Get-ChildItem $share -Directory | Where-Object { $_.Name -like 'boost-*' -and (Test-Path (Join-Path $_.FullName 'copyright')) } | ForEach-Object { $_.Name })
$vWx = Version-Wx

$piezas = @(
    @{ Nombre = 'PicoTorrent'; Version = ''; Web = 'https://github.com/picotorrent/picotorrent'; Licencia = 'MIT'
       Nota = 'Brizna es una versión modificada de PicoTorrent.'; Texto = Leer (Join-Path $codigo 'LICENSE') },
    @{ Nombre = 'libtorrent (Rasterbar)'; Version = Version-Vcpkg 'libtorrent'; Web = 'https://libtorrent.org'; Licencia = 'BSD'
       Nota = 'Incluye try_signal (BSD-3-Clause) y el código de terceros que se menciona en su licencia.'; Texto = Leer (Join-Path $share 'libtorrent\copyright') },
    @{ Nombre = 'Boost'; Version = Version-Vcpkg 'boost-log'; Web = 'https://www.boost.org'; Licencia = 'Boost Software License 1.0'
       Nota = ("Bibliotecas usadas ({0}): {1}." -f $boost.Count, ($boost -join ', ')); Texto = Leer (Join-Path $share 'boost-log\copyright') },
    @{ Nombre = 'OpenSSL'; Version = Version-Vcpkg 'openssl'; Web = 'https://www.openssl.org'; Licencia = 'Apache License 2.0'
       Nota = 'Se usa sin modificaciones. OpenSSL 3.x no trae archivo NOTICE.'; Texto = Leer (Join-Path $share 'openssl\copyright') },
    @{ Nombre = '{fmt}'; Version = Version-Vcpkg 'fmt'; Web = 'https://fmt.dev'; Licencia = 'MIT'
       Nota = ''; Texto = Leer (Join-Path $share 'fmt\copyright') },
    @{ Nombre = 'JSON for Modern C++ (nlohmann/json)'; Version = Version-Vcpkg 'nlohmann-json'; Web = 'https://github.com/nlohmann/json'; Licencia = 'MIT'
       Nota = ''; Texto = Leer (Join-Path $share 'nlohmann-json\copyright') },
    @{ Nombre = 'SQLite'; Version = Version-Vcpkg 'sqlite3'; Web = 'https://www.sqlite.org'; Licencia = 'Dominio público'
       Nota = ''; Texto = Leer (Join-Path $share 'sqlite3\copyright') },
    @{ Nombre = 'wxWidgets'; Version = $vWx; Web = 'https://www.wxwidgets.org'; Licencia = 'wxWindows Library Licence 3.1'
       Nota = 'Se usa sin modificaciones, enlazado de forma estática según la excepción de su licencia.'; Texto = Leer (Join-Path $wx 'docs\licence.txt') },
    @{ Nombre = 'zlib (incluida en wxWidgets)'; Version = ''; Web = 'https://zlib.net'; Licencia = 'zlib'
       Nota = ''; Texto = Leer (Join-Path $wx 'src\zlib\LICENSE') },
    @{ Nombre = 'libpng (incluida en wxWidgets)'; Version = ''; Web = 'http://www.libpng.org'; Licencia = 'PNG Reference Library License version 2'
       Nota = ''; Texto = Leer (Join-Path $wx 'src\png\LICENSE') },
    @{ Nombre = 'libjpeg del Independent JPEG Group (incluida en wxWidgets)'; Version = ''; Web = 'https://ijg.org'; Licencia = 'licencia del IJG'
       Nota = 'This software is based in part on the work of the Independent JPEG Group.'; Texto = Licencia-Ijg },
    @{ Nombre = 'LibTIFF (incluida en wxWidgets)'; Version = ''; Web = 'https://libtiff.gitlab.io/libtiff/'; Licencia = 'licencia de LibTIFF'
       Nota = ''; Texto = Leer (Join-Path $wx 'src\tiff\LICENSE.md') },
    @{ Nombre = 'PCRE2 (incluida en wxWidgets)'; Version = ''; Web = 'https://github.com/PCRE2Project/pcre2'; Licencia = 'BSD-3-Clause WITH PCRE2-exception'
       Nota = ''; Texto = Leer (Join-Path $wx '3rdparty\pcre\LICENCE.md') },
    @{ Nombre = 'NanoSVG (incluida en wxWidgets)'; Version = ''; Web = 'https://github.com/memononen/nanosvg'; Licencia = 'tipo zlib'
       Nota = ''; Texto = Leer (Join-Path $wx '3rdparty\nanosvg\LICENSE.txt') },
    @{ Nombre = 'libwebp (incluida en wxWidgets)'; Version = ''; Web = 'https://chromium.googlesource.com/webm/libwebp'; Licencia = 'BSD-3-Clause, con concesión de patentes de Google'
       Nota = ''; Texto = (Leer (Join-Path $wx '3rdparty\libwebp\COPYING')) + "`n`n" + (Leer (Join-Path $wx '3rdparty\libwebp\PATENTS')) },
    @{ Nombre = 'ANTLR 4 (runtime de C++)'; Version = ''; Web = 'https://www.antlr.org'; Licencia = 'BSD-3-Clause'
       Nota = 'Se usa para el filtro de búsqueda de torrents (PQL).'; Texto = Leer (Join-Path $codigo 'vendor\antlr4\LICENSE.txt') }
)

$sb = New-Object System.Text.StringBuilder
$titulo = 'Brizna' + $(if ($Version) { " $Version" } else { '' }) + ' - Avisos de terceros (licencias)'
[void]$sb.AppendLine($titulo)
[void]$sb.AppendLine('=' * $titulo.Length)
[void]$sb.AppendLine()
[void]$sb.AppendLine('Brizna es un cliente de BitTorrent basado en PicoTorrent (https://github.com/picotorrent/picotorrent).')
[void]$sb.AppendLine('Incluye el software de terceros que se lista abajo. Cada pieza conserva su propia licencia,')
[void]$sb.AppendLine('cuyo texto se reproduce sin cambios (en inglés, que es el texto original y el que vale).')
[void]$sb.AppendLine()
[void]$sb.AppendLine('Contenido')
[void]$sb.AppendLine('---------')
$n = 0
foreach ($p in $piezas) {
    $n++
    $nombre = $p.Nombre + $(if ($p.Version) { ' ' + $p.Version } else { '' })
    [void]$sb.AppendLine(('{0,2}. {1}  ({2})' -f $n, $nombre, $p.Licencia))
}
$n = 0
foreach ($p in $piezas) {
    $n++
    [void]$sb.AppendLine()
    [void]$sb.AppendLine()
    [void]$sb.AppendLine('=' * 78)
    [void]$sb.AppendLine(('{0}. {1}{2}' -f $n, $p.Nombre, $(if ($p.Version) { ' ' + $p.Version } else { '' })))
    [void]$sb.AppendLine("   $($p.Web)")
    [void]$sb.AppendLine("   Licencia: $($p.Licencia)")
    if ($p.Nota) { [void]$sb.AppendLine("   $($p.Nota)") }
    [void]$sb.AppendLine('=' * 78)
    [void]$sb.AppendLine()
    [void]$sb.AppendLine($p.Texto)
}

$texto = $sb.ToString() -replace "`r?`n", "`r`n"
New-Item -ItemType Directory -Force (Split-Path $Salida) | Out-Null
[IO.File]::WriteAllText($Salida, $texto, (New-Object System.Text.UTF8Encoding $true))
Write-Host ("Avisos de terceros: {0} piezas -> {1}" -f $piezas.Count, $Salida)
