# Brizna

Brizna es un cliente de BitTorrent liviano para Windows 10 y 11 (64 bits). Está basado en
[PicoTorrent](https://github.com/picotorrent/picotorrent) (licencia MIT) y usa el motor
[libtorrent](https://libtorrent.org) 2.0.

Este repositorio solo tiene las **descargas** de Brizna.

## Descargar

1. En [Releases](https://github.com/Brizna-Soft/brizna-descargas/releases/latest) baja `Brizna-<versión>-x64.zip`.
2. Descomprímelo en una carpeta propia y abre `Brizna.exe`. No hace falta instalarlo:
   guarda sus ajustes y su lista de torrents en esa misma carpeta.

Necesita el *Microsoft Visual C++ Redistributable* de 64 bits, que la mayoría de las PCs ya tienen.
Si Brizna no abre, se baja de Microsoft: https://aka.ms/vs/17/release/vc_redist.x64.exe

## Actualizaciones

Brizna busca versiones nuevas en este repositorio cada vez que se abre, y también desde
**Ayuda > Buscar actualización**. Cada versión está firmada con una clave Ed25519: Brizna
comprueba la firma y la huella del archivo antes de instalar, y rechaza cualquier archivo
que no coincida.

Para comprobar un `.zip` a mano, su huella SHA256 está en `SHA256SUMS.txt` dentro de cada versión.

## Licencias

Brizna incluye software de terceros (libtorrent, Boost, OpenSSL, wxWidgets y otros). Sus
licencias están en `AVISOS-DE-TERCEROS.txt`, junto al programa y en cada versión.

Hecho con ayuda de IA.
