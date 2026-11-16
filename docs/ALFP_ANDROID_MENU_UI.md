# Menú Android ALFP para AYN Thor

## Idioma y datos

El panel inferior usa rótulos españoles y textos originales de la columna
española ALFP para habilidades, objetos y diario. No cambia el idioma ni la
memoria del juego para traducir el panel. La pantalla de importación compartida
del motor conserva actualmente sus textos ingleses; el juego superior utiliza
su propia selección de idioma. La partida copiada usada como evidencia tiene
español seleccionado.

El único método JNI de lectura de la presentación es
getMenuSnapshot(int personajeId): Object[]. Estado, personaje inspeccionado,
retrato, tres habilidades, 46 entradas de catálogo/cantidades y 43 misiones
proceden de una publicación bloqueada única. Java no lee RAM GBA mutable.
Un paquete inválido limpia los datos anteriores. Las cantidades y condiciones
del diario se calculan en la misma publicación del hilo del motor.

OBJETOS muestra las entradas poseídas, con cantidad positiva, nombre y
descripción; el catálogo completo contiene 46 IDs nativos. El panel actual no
ofrece consumo de objetos. La rutina original de uso requiere efectos de motor
y posibles diálogos; no se simulan cambios de cantidades, PV o PE en Java.

## Rutas táctiles

Coordenadas lógicas del lienzo de 1240×1080. La Thor observada proporciona
1240×1080 en la pantalla inferior (display lógico 4): escala 1:1. En un área
útil diferente se aplica la misma transformación centrada a dibujo y toques.
Los toques en los márgenes exteriores se descartan.

| Control | Zona lógica | Resultado y condición |
|---|---|---|
| DIARIO | x100–440, y950–1080 | Abre MISIONES al entrar desde otra sección. |
| ESTADO | x450–790, y890–1040 | Abre estadísticas o la subpágina previamente elegida. |
| OBJETOS | x800–1170, y950–1080 | Abre inventario poseído con cantidades y descripciones. |
| Flecha superior izquierda/derecha en ESTADO | x30–140 / x1100–1210, y35–140 | Recorre únicamente personajes disponibles, con vuelta; deshabilitada con menos de dos. La selección no cambia el personaje GBA. |
| Flecha superior derecha en DIARIO | x1100–1210, y35–140 | MISIONES → TERMINADO; deshabilitada en TERMINADO. |
| Flecha superior izquierda en DIARIO | x30–140, y35–140 | TERMINADO → MISIONES; deshabilitada en MISIONES. |
| Flecha vertical inferior en ESTADO | x124–208, y677–768 | Estadísticas → habilidades, si hay personaje disponible. |
| Flecha vertical superior en ESTADO | x124–208, y242–333 | Habilidades → estadísticas. |
| Tarjeta de habilidad | x230–1130, y275–808 | Selección informativa local. No equipa una técnica. |
| Arrastre en habilidades | x230–1130, y275–808 | Desplaza descripciones largas dentro del panel. |
| Arrastre en OBJETOS | x180–1080, y275–820 | Desplaza la lista poseída y sus descripciones. |
| Arrastre en DIARIO | x180–1080, y245–820 | Desplaza misiones; cada página conserva su posición. |

Un movimiento de más de 14 unidades lógicas cancela el toque simple. Un segundo
dedo cancela el gesto. Las acciones inferiores se consumen en la presentación y
no generan botones GBA. La presentación no toma el foco de los controles del
juego superior. Se cierra al pausar o retirar la segunda pantalla y se reconstruye
al reanudar o reconectar una pantalla válida.

Las misiones ocultas (0) no aparecen; las pendientes (1) están en MISIONES y las
terminadas (2) en TERMINADO. Un estado desconocido (-1) se distingue de una lista
vacía verificada. No se inventan misiones ni cantidades cuando faltan datos.

## Evidencia y aceptación pendiente

- Java compilado contra Android 35: 19 fuentes de título, plataforma y SDL.
- JNI comprobado sintácticamente con NDK 27.1.12297006, ARM64 y las opciones
  CMake reales; esto no sustituye el enlace de la biblioteca completa.
- python3 tools/test_alfp_menu_jni.py usa JNI de producción y JVM real
  -Xcheck:jni, con 3000 snapshots concurrentes sintéticos.
- La opción --rom <ALFP.gba> --state <copia.GBAS> comprueba además los
  decodificadores reales y el paso JNI sobre memoria copiada: Trunks único
  disponible, nivel 6, PV149/149, PE39/39, EXP1000, SIG550, 46 IDs/textos/cantidades
  y 43 textos/estados. Los archivos fuente se verifican por identidad y sólo se leen.
- El APK anterior instalado se probó en la Thor: OBJETOS → MISIONES → TERMINADO
  → ESTADO. Sus capturas muestran todavía el inventario pendiente de esa versión.
  Se verificaron iguales los hashes de ROM, BIOS, save y ambas suspensiones antes
  y después; se restauraron volumen y estado apagado del dispositivo.
- Aún hace falta probar la nueva APK completa: contenido real de las cuatro
  vistas, navegación entre varios personajes, desplazamiento, carga/reanudación,
  reconexión de pantalla y ausencia de efectos GBA al tocar el panel. Las pruebas
  sobre memoria copiada y la navegación del APK previo no cubren esa aceptación.

La evidencia detallada está en docs/evidence/alfp-android-jni-checks.json.

## Comprobación estática de la APK

tools/check_thor_apk.py verifica identidad/firma compatibles con la instalación
respaldada, ABI ARM64 de todas las bibliotecas, símbolo JNI definido, clases y
firma Java del snapshot, actividades de entrada, cuatro assets exactos y el
payload Android actual. Rechaza ZIP corrupto/duplicado y archivos privados
ROM/BIOS/save/state/replay incluidos. Recorre todas las unidades DEX, incluidas
classes10.dex y posteriores. python3 tools/test_check_thor_apk.py prueba
contratos válidos e inválidos de ELF, DEX, JNI y manifiesto sin generar una APK.

Este control es integridad estática del artefacto. No certifica el panorámico,
el arranque, el rendimiento, el audio ni el comportamiento en dispositivo, y no
modifica el bloqueo de tools/build_thor_offline.sh.
