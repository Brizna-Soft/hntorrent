# HNTorrent

HNTorrent es un cliente de BitTorrent liviano y gratis para Windows 10 y 11 (64 bits), de
Brizna Soft. Está basado en [PicoTorrent](https://github.com/picotorrent/picotorrent)
(licencia MIT) y usa el motor [libtorrent](https://libtorrent.org) 2.0.

Este repositorio solo tiene las **descargas** de HNTorrent.

## Descargar

1. En [Releases](https://github.com/Brizna-Soft/hntorrent-descargas/releases/latest) baja `HNTorrent-<versión>-x64.zip`.
2. Descomprímelo en una carpeta propia y abre `HNTorrent.exe`. No hace falta instalarlo:
   guarda sus ajustes y su lista de torrents en esa misma carpeta.
3. Si Windows dice «Windows protegió su PC», pulsa **Más información** y después **Ejecutar de todas formas**.
   Pasa porque el programa es nuevo y todavía no tiene firma digital.

Si quieres que los enlaces magnet y los archivos `.torrent` se abran con HNTorrent, ve a
**Ver > Preferencias > General** y pulsa **Abrir magnet y .torrent con HNTorrent...**. Es opcional, solo
cambia cosas de tu usuario y se quita con **Dejar de ofrecerlo**.

Necesita el *Microsoft Visual C++ Redistributable* de 64 bits, que la mayoría de las PCs ya tienen.
Si HNTorrent no abre, se baja de Microsoft: https://aka.ms/vs/17/release/vc_redist.x64.exe

## Si Windows no deja abrir el programa

Nuestros programas todavía no tienen firma digital, y por eso Windows puede frenarlos de dos maneras:

- **«Windows protegió su PC» (SmartScreen).** Pulsa **Más información** y después **Ejecutar de todas formas**.
- **«Control inteligente de aplicaciones».** Si lo tienes activado, puede bloquear el programa *sin* darte la opción
  de abrirlo igual. Microsoft explica que no se puede permitir un solo programa: la única salida es apagar esa
  protección en **Seguridad de Windows > Control de aplicaciones y exploradores**. Es tu decisión; con Windows al día
  se puede volver a encender después.
  ([Preguntas frecuentes de Microsoft](https://support.microsoft.com/es-es/topic/smart-app-control-frequently-asked-questions-285ea03d-fa88-4d56-882e-6698afdb7003))

Estamos preparando la firma digital para que esto deje de pasar. Mientras tanto, puedes comprobar que el archivo es
el original con su huella SHA-256 (`SHA256SUMS.txt` de cada versión) y, en las versiones nuevas, con
`gh attestation verify <archivo.zip> --repo Brizna-Soft/hntorrent`.

## Actualizaciones

La primera vez que lo abres, HNTorrent te pregunta si quieres que busque versiones nuevas en este
repositorio cada vez que se abre (se cambia en Preferencias). Siempre puedes buscarla tú en
**Ayuda > Buscar actualización**. Cada versión está firmada con una clave Ed25519: HNTorrent
comprueba la firma y la huella del archivo antes de instalar, y rechaza cualquier archivo
que no coincida.

Desde la 0.27.4, antes de cambiar nada HNTorrent prueba si Windows deja abrir la versión nueva. Si no la deja, no
la instala, sigues con la que tienes y te lo dice. Las copias de la 0.27.3 o anteriores no hacen esa prueba: si
tienes activado el «Control inteligente de aplicaciones» y una de ellas te ofrece una versión nueva, pulsa
**Ahora no**.

Para comprobar un `.zip` a mano, sus huellas SHA256 están en `SHA256SUMS.txt` dentro de cada versión.
Además, desde la 0.27.3 cada `.zip` tiene una atestación pública de que lo armó GitHub a partir del
código de [Brizna-Soft/hntorrent](https://github.com/Brizna-Soft/hntorrent):
`gh attestation verify HNTorrent-<versión>-x64.zip --repo Brizna-Soft/hntorrent`.

¿Algo falla? En HNTorrent, **Ayuda > Reportar un problema** abre el formulario con la versión ya puesta.

**¿Tenías Brizna?** Así se llamó HNTorrent hasta la versión 0.26, y se actualiza sola: al instalar
la versión nueva, el programa pasa a llamarse `HNTorrent.exe` y conserva tus torrents y ajustes.
Para eso cada versión trae también `Brizna-<versión>-x64.zip`, que es el mismo programa.

## Donar

HNTorrent es y seguirá siendo gratis, sin publicidad ni cobros. Si te sirve y quieres apoyarlo,
puedes donar por PayPal: **[paypal.me/BriznaSoft](https://paypal.me/BriznaSoft)**.
Cualquier monto ayuda. Gracias.

## Licencias

HNTorrent incluye software de terceros (libtorrent, Boost, OpenSSL, wxWidgets y otros). Sus
licencias están en `AVISOS-DE-TERCEROS.txt`, junto al programa y en cada versión.

Hecho con ayuda de IA.
