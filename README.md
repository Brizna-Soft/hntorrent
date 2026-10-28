# HNTorrent

**Cliente de BitTorrent liviano y gratis para Windows.** En español, sin publicidad y sin recoger datos. Hecho por
[Brizna Soft](https://brizna-soft.github.io) en Honduras.

Este repositorio es el **código**. Para bajar el programa, ve a
**[hntorrent-descargas](https://github.com/Brizna-Soft/hntorrent-descargas/releases/latest)**.

## De dónde viene

HNTorrent nace de [PicoTorrent](https://github.com/picotorrent/picotorrent), de Viktor Elofsson y colaboradores
(licencia MIT), que dejó de actualizarse en 2023. Usa [libtorrent](https://libtorrent.org) para bajar y compartir,
y [wxWidgets](https://www.wxwidgets.org) para la ventana.

**Lo que cambiamos:**

- **Bibliotecas al día:** libtorrent 2.0.15, wxWidgets 3.3.4, y vcpkg 2026.10.16 con Boost, OpenSSL y SQLite.
- **Sin informes a terceros:** quitamos Crashpad, que mandaba informes de fallos a un servidor externo, y el
  buscador de actualizaciones viejo.
- **Actualizador propio:**
  - cada versión va firmada con Ed25519 y el programa comprueba la firma antes de instalarla;
  - la firma cubre la versión y la huella SHA-256 del paquete, así que nadie puede colar una versión vieja ni
    cambiar el archivo.
- **Interfaz nueva:** en español, con tema claro y oscuro, y confirmación antes de quitar un torrent.

## Cómo se publica cada versión

- **`.github/workflows/compilar.yml`** compila en cada cambio.
- **`.github/workflows/actualizaciones.yml`** revisa cada lunes si hay versiones nuevas de libtorrent y de las
  bibliotecas. Si las hay, compila, firma y publica en
  [hntorrent-descargas](https://github.com/Brizna-Soft/hntorrent-descargas). También se puede lanzar a mano.
- **Las claves:**
  - la privada solo vive como secreto de GitHub y nunca está en el código;
  - la pública va dentro del programa, en `src/picotorrent/updatechecker.cpp`.

Así, cada .zip que bajas lo armó GitHub a partir de este mismo código.

## Compilar

Necesitas Visual Studio 2022 (o sus Build Tools) con C++, y Git.

```
git clone --recurse-submodules https://github.com/Brizna-Soft/hntorrent.git
cd hntorrent
powershell -ExecutionPolicy Bypass -File compilar.ps1
```

La primera vez tarda, porque vcpkg compila Boost, OpenSSL y libtorrent. El programa queda en `..\programa`.

Otros scripts:
- `actualizar-libtorrent.ps1` y `actualizar-vcpkg.ps1`: suben las versiones de las bibliotecas.
- `empaquetar.ps1`: arma el .zip.
- `generar-avisos.ps1`: junta las licencias de terceros en `AVISOS-DE-TERCEROS.txt`.

## Política de firma de código (Code signing policy)

Pedimos la firma gratuita de SignPath Foundation para los .exe de HNTorrent. Cuando la aprueben, cada versión
llevará esta firma:

> Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by
> [SignPath Foundation](https://signpath.org).

- **Equipo:** el autor, el revisor y el aprobador es [Brizna-Soft](https://github.com/Brizna-Soft).
- **Qué se firma:** solo lo que GitHub Actions compila a partir de este repositorio. Cada firma la aprueba a mano
  Brizna Soft.
- **Privacidad:** HNTorrent no manda tus datos a nadie. Solo se conecta para lo que es su trabajo:
  - las conexiones de BitTorrent de los torrents que tú agregas;
  - al abrirse, la consulta de versiones nuevas en GitHub.

  Más detalles en la [página de privacidad](https://brizna-soft.github.io/privacidad.html).

## ¿Algo falla?

[Reporta el problema aquí](https://github.com/Brizna-Soft/hntorrent-descargas/issues/new/choose). Cuéntanos qué
pasó y qué esperabas.

## Apoyar

HNTorrent es y seguirá siendo gratis. Si te sirve, puedes apoyar a Brizna Soft con una
[donación por PayPal](https://paypal.me/BriznaSoft).

## Licencia

- **Licencia MIT**, igual que PicoTorrent: © 2015 Viktor Elofsson y colaboradores; © 2026 Brizna Soft por los
  cambios.
- **Bibliotecas:** cada una tiene su licencia, y van juntas en `AVISOS-DE-TERCEROS.txt` dentro de cada versión.
- **Solo para torrents legales:** HNTorrent no trae ni recomienda buscadores de torrents.

Hecho con ayuda de IA.
