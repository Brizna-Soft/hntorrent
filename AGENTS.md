# Instrucciones para agentes de IA (Codex, Claude y otros)

Este repositorio es de **Brizna Soft**, un estudio pequeño de Honduras que hace programas gratis, sin
publicidad y sin recoger datos. Estas reglas valen para cualquier agente que lea, revise o cambie el código.

## Reglas que no se rompen

- **Idioma:** todo lo que ve la persona va en español sencillo: ventanas, avisos, README y notas. Se trata
  de «tú». Sin jerga técnica en los avisos; si hace falta un código, va al final («Para el técnico: …»).
- **Privacidad:** el programa no se conecta a internet salvo para hacer su trabajo o cuando la persona lo
  pide (por ejemplo «Buscar actualización»). Sin telemetría, sin publicidad y sin datos personales en los
  registros ni en los reportes (nada de usuario, nombre de la PC, rutas completas ni direcciones completas).
- **Seguridad:**
  - Nada se borra sin una confirmación clara, y lo que se borra va a la Papelera siempre que se pueda.
  - Nada pide ser administrador ni cambia la configuración de Windows, salvo las excepciones de la guía,
    que son opcionales y solo pasan si la persona pulsa un botón.
  - Nada se baja ni se instala solo.
- **Autor:** los commits van como «Brizna Soft»
  (`349058083+Brizna-Soft@users.noreply.github.com`), sin líneas que nombren a una IA ni `Co-Authored-By`.
  Se puede decir «Hecho con ayuda de IA».
- **Publicar:** nadie publica una versión (etiqueta `v*`) ni sube a la rama principal sin que el dueño lo
  apruebe. Lo que borra archivos o toca datos personales pasa antes por una revisión de solo lectura.

## Revisiones de solo lectura

Cuando se pide una revisión sin cambiar archivos:
- No cambies nada, ni siquiera para probar.
- Responde en español sencillo y empieza con **APROBADO** o **FALTA**.
- Para cada falla: gravedad (alta, media o baja), archivo, línea y qué le pasaría a la persona.

## Más detalles

Las reglas completas, la estructura de cada programa y cómo compilar están en
`Documents\Brizna-Soft\programas\GUIA-PROGRAMAS.md` (en la PC del estudio) y en el README de este
repositorio.

## Este programa: HNTorrent

Cliente de torrents, basado en PicoTorrent (MIT). La rama principal es `version-propia`.

- Compila en GitHub: flujo «Compilar» (`compilar.yml`, también a mano). En la PC del estudio Boost no compila en
  rutas largas.
- Publica con el flujo «Revisar actualizaciones» (`actualizaciones.yml`) en `hntorrent` y `hntorrent-descargas`. Cada
  versión va firmada (Ed25519) y la copia instalada comprueba la firma antes de actualizarse.
- Lo de Brizna Soft del menú Ayuda está en `src/picotorrent/ui/brizna.hpp`; los textos, en `lang/es-ES.json` (y en
  `lang/en-US.json`, que es el respaldo).
- HNTorrent nunca borra archivos: «Quitar y abrir la carpeta» abre el Explorador con sus archivos marcados.
- Abrir magnet y .torrent con HNTorrent es opcional (botón en Preferencias) y solo anota cosas en HKCU.
