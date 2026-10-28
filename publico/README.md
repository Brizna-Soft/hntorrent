# HNTorrent

HNTorrent es un cliente de BitTorrent liviano y gratis para Windows 10 y 11 (64 bits), de
Brizna Soft. Está basado en [PicoTorrent](https://github.com/picotorrent/picotorrent)
(licencia MIT) y usa el motor [libtorrent](https://libtorrent.org) 2.0.

Este repositorio solo tiene las **descargas** de HNTorrent.

## Descargar

1. En [Releases](https://github.com/Brizna-Soft/hntorrent-descargas/releases/latest) baja `HNTorrent-<versión>-x64.zip`.
2. Descomprímelo en una carpeta propia y abre `HNTorrent.exe`. No hace falta instalarlo:
   guarda sus ajustes y su lista de torrents en esa misma carpeta.

Necesita el *Microsoft Visual C++ Redistributable* de 64 bits, que la mayoría de las PCs ya tienen.
Si HNTorrent no abre, se baja de Microsoft: https://aka.ms/vs/17/release/vc_redist.x64.exe

## Actualizaciones

HNTorrent busca versiones nuevas en este repositorio cada vez que se abre, y también desde
**Ayuda > Buscar actualización**. Cada versión está firmada con una clave Ed25519: HNTorrent
comprueba la firma y la huella del archivo antes de instalar, y rechaza cualquier archivo
que no coincida.

Para comprobar un `.zip` a mano, sus huellas SHA256 están en `SHA256SUMS.txt` dentro de cada versión.

**¿Tenías Brizna?** Así se llamó HNTorrent hasta la versión 0.26, y se actualiza sola: al instalar
la versión nueva, el programa pasa a llamarse `HNTorrent.exe` y conserva tus torrents y ajustes.
Para eso cada versión trae también `Brizna-<versión>-x64.zip`, que es el mismo programa.

## Donar

HNTorrent es y seguirá siendo gratis, sin publicidad ni cobros. Pronto se podrá apoyar con una
donación por PayPal; el enlace estará aquí.

## Licencias

HNTorrent incluye software de terceros (libtorrent, Boost, OpenSSL, wxWidgets y otros). Sus
licencias están en `AVISOS-DE-TERCEROS.txt`, junto al programa y en cada versión.

Hecho con ayuda de IA.
