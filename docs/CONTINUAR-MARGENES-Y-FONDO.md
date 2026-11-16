# Continuación — Legacy of Goku II / Thor — 16-11-2026

## Petición y límites

Resolver línea negra/parpadeo al alejarse de ambos extremos de los mapas;
extender el fondo morado de selección sin reflejar/repetir título, mandos,
ranuras ni textos. Última tarea: el usuario ha confirmado que quiere el
mismo fondo morado también detrás de los menús inferiores (Diario,
Habilidades, etc.), no solamente al seleccionar partida.

Mantener HUD completo dentro del 3:2. No clasificar OAM como HUD por slot,
tile o posición: esa regla ya se eliminó porque movía NPC. No inventar
habitaciones mediante espejo: los interiores y mapas no confirmados no
reciben prolongación cosmética. No tocar colisiones, eventos ni entidades.
No publicar en GitHub (el usuario canceló esa parte).

## Entorno real

- Workspace Windows: `C:\Users\rober\Documents\ChatGPT\Legacy of Goku II`.
- Proyecto AUTORITATIVO remoto: `/home/plex-rober/legacy-goku2-thor`.
- SSH: `plex-rober@192.168.1.129` (autorizado).
- ADB: `/home/plex-rober/Android/Sdk/platform-tools/adb`, siempre
  `-s 192.168.1.128:33313` (hay un segundo alias del mismo dispositivo).
- Paquete: `org.erpiopio.legacygoku2.thor`.

## Estado al escribir este informe

Las correcciones más recientes de PARPADEO y MENÚ MORADO están editadas
en la fuente remota y pasan las pruebas de escritorio. Todavía hay que
compilar/instalar el APK que las incluya. El APK previamente instalado
contiene extensión de ciudades y protección de interiores, pero aún el
algoritmo anterior que producía la línea y reflejaba el menú.

El fondo inferior YA está implementado: asset original extraído como
`fondo_nebulosa.png`, cargado y dibujado detrás de la plantilla en
`Legacy2MenuPresentation.java`. Imagen única, escala uniforme, recorte
centrado; sin mosaicos ni espejos. No se toca loading.png ni sus 5 segundos.
Pendiente en este momento: compilación/instalación final Android.

## Hallazgos y correcciones nuevas

Fuente: `src/legacy2_extended_view_europe.cpp`.

1. Parpadeo reproducido desde `/tmp/legacy2-worldmap/edge-flicker.state`.
   Nivel `082A244C`, cámara inicial X=11, habitación 584x1344. Con 12
   fotogramas a derecha aparece línea negra vertical; con 24 desaparece.
   Antes: `/tmp/legacy2-rain-work/edge-away-12.png`. Después:
   `edge-away-fixed12.png` y `edge-away-fixed24.png`, sin línea.
2. Se retiró la doble reflexión usando cámara RAM viva y después límites
   de recurso. Ahora `legacy2_bg_x_provider` usa solamente el scroll BG que
   se está dibujando y el descriptor nativo: escala X en +04 y offset X en
   +0C, con 10 bits fraccionarios. Banda válida:
   `left=offset>>10`, `right=((room_width-240)*scale+offset)>>10)+240`.
   Así no se mezcla la cámara del siguiente frame con el BG actual y se
   respeta el paralaje. Un foreground corto no se refleja arbitrariamente
   dentro de un mapa. Casos de prueba comparan cámara RAM +/-1 con scroll
   fijo en ambos bordes. Cambio aplicado por
   `/tmp/legacy2-worldmap/sync_margin_scroll.py` (NO ejecutar de nuevo).
3. Selección de partida: BG3 es solo la nebulosa; BG2 contiene título,
   mandos, bolas y textos; BG1 el marco. El menú retiene en RAM el nivel de
   muerte `082EB82C`, y la antigua detección lo confundía con nubes y
   reflejaba todas las capas. Nuevo `is_native_nebula_menu` reconoce la
   textura VIVA: BG3CNT=1F83, tilemap primeros 8 entries 0..7 y estas ocho
   palabras del primer tile:
   `E0E0E0E0 D8CACACA CAE0E0E0 DFCACACA CACAE0E0 DFDFDFCA CACACAE0 DFDFDFDF`.
   Verificado en `select-menu.state` y `left-map-edge.state`, con niveles
   previos distintos. Cache por bus/frame/state_epoch, independiente de
   paleta, idioma y scroll.
4. Menú reconocido: se escala UNA VEZ el viewport nativo de BG3 a la
   anchura extendida (`output_x*240/width`); BG0..2 rechazan márgenes y
   conservan su presentación 3:2. `is_overworld` excluye esta pantalla.
   Cambio aplicado por `fix_nebula_menu.py` (NO repetir).

## Validación actual

- `/tmp/test_alfp_view_hooks`: **1330 casos PASS**, fuente actual.
- Build escritorio actual correcto (`nebula-desktop-build.log`).
- Menú de comprobación: `/tmp/legacy2-rain-work/select-menu-fixed.png`.
  Falta verlo antes de instalar; archivo generado correctamente.
- Los scripts de un fotograma encadenado muestran márgenes totalmente
  negros por inicialización del renderer al restaurar; NO usarlos para
  diagnosticar este fallo. Las reproducciones de 12/24 frames sí lo muestran.
- Diagnósticos fuera de la compilación normal:
  `tools/diagnostics/widescreen/`; estados adicionales en
  `/tmp/legacy2-worldmap/`; reproductor en
  `/tmp/legacy2-rain-work/progress_rain.py`.
- Informe acumulado: `docs/margenes-panoramicos.md` remoto y copia local.

## Fondo inferior: punto de entrada

`android/app/src/main/java/org/erpiopio/legacygoku2/thor/Legacy2MenuPresentation.java`.
Clase `EstadoView`: campos de assets alrededor de 228, carga de assets
365..375, `onDraw` alrededor de 1819. Dibuja `plantilla.png` en 1894 y
`cartel.png` en 1913. Hay `dibujarFondo...` alrededor de 481 que pinta
`Color.rgb(29,10,64)`. Inspeccionar 1819..1930 antes de editar.

No se ha alterado `plantilla.png`, pestañas, cartel ni LOADING de 5 s.
El usuario autoriza cambiar el fondo de los menús inferiores, pero conservar
su estructura y contenido. Reutilizar la nebulosa ORIGINAL de BG3, no
generar ni reflejar un fondo nuevo. Las extracciones por capas están en
`/tmp/legacy2-worldmap/select-layers-bg3.ppm/.png`; son solo el fondo limpio.
La imagen tiene corrección de color distinta de la captura Android; conviene
consultar la conversión del renderer si se exporta a un asset Java.

## Compilación y despliegue

Escritorio: `cmake --build build-europe --target DragonBallZLegacyofGokuIIRecomp -- -j2`.

Android (desde `android/`): Java 17 y SDK en
`/home/plex-rober/Android/Sdk`; `CMAKE_BUILD_PARALLEL_LEVEL=1`.
Gradle offline: `/home/plex-rober/.gradle/wrapper/dists/gradle-8.7-bin/bhs2wmbdwecv87pi65oeuq5iu/gradle-8.7/bin/gradle`
con `--offline --no-daemon --max-workers=1 --console=plain -PgbaNativeJobs=1
-PgbaAbis=arm64-v8a :app:assembleDebug`.
APK: `android/app/build/outputs/apk/debug/app-debug.apk`.

Preservar punto del usuario: HOME, esperar 2 s, guardar por `run-as` el
`files/roms/legacy_of_goku_ii_europe_alfp.suspend.state`; instalar `-r`.
Restaurar con un slot temporal `.state9`: respaldar su contenido si existe,
copiar el suspend, abrir `.Legacy2SetupActivity` con extra
`org.gbarecomp.extra.AUTOSTART=true`, esperar 7 s y enviar keyevent 139 (F9).
Después devolver el slot9 previo o retirar únicamente el temporal. No
reiniciar la partida ni sobrescribir guardados normales. Las sesiones previas
siguen este proceso en los tool logs; ya se usó sin perder progreso.

## Próximo orden de trabajo

1. Ver `select-menu-fixed.png` y confirmar que BG3 llena sin duplicar UI.
2. Completar el fondo morado inferior si el uso alcanza; si no, dejarlo
   explícitamente pendiente y priorizar compilar/instalar las dos correcciones.
3. Guardar la fuente y estados útiles, actualizar este informe con estado
   real de instalación (no dar por instalado antes de `Success`).
4. Dejar al usuario comprobar en la Thor; no prolongar investigaciones.

## Actualización de última tarea

Pruebas de escritorio actuales: **1330 PASS**. Menú superior revisado en
`select-menu-fixed.png`: UI dentro del 3:2 y nebulosa sin duplicados.
Fondo inferior implementado según lo confirmado por el usuario. La imagen
se extrajo de la VRAM y paleta originales, sin generación artificial.
Para cualquier continuación, comprobar el último apartado de instalación
antes de asumir que el APK actual ya contiene estos cambios.
