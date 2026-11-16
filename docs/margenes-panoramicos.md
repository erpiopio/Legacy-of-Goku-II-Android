# Márgenes panorámicos ALFP: 16-11-2026

## Cambios comunes de presentación

Fuente: `src/legacy2_extended_view_europe.cpp`. No se modifica la ROM, los
eventos, las colisiones ni el estado lógico de objetos.

1. Los objetos de capa se identifican por el índice BG nativo, escrito por
   `08005334` en payload+4 (base de asignación+0C), y por su recurso en el
   descriptor de nivel activo `03000F80`, offsets `14+BG*4`. La comparación
   anterior de píxeles con VRAM queda como respaldo. Una capa escasa o
   animada ya no queda excluida por no superar esa comparación.
2. Los chunks sin datos o fuera del recurso usan entrada de tilemap cero,
   como las rutinas nativas `08005386`/`0800546A`. Entrada cero no equivale
   necesariamente a transparencia. Era una causa de las esquinas negras.
3. Solo se amplía la consulta del rectángulo de visibilidad de dibujo.
   El gestor `0801E2E8` llama al método virtual en `0801E30E`, retorno
   `LR=0801E313`. Permanecen las lecturas exactas verificadas de actores
   `08015348/0801534E`, `08016156/0801615C` y gráficos `0801D218/0801D21E`.
   Se usa el desplazamiento realmente renderizado; no se amplían consultas
   de eventos, actualizaciones ni tiempos de vida.
4. En el mapamundi, modo proyectado, BG0/BG1 conservan su textura nativa
   periódica. El cielo se extiende sin usar la geometría de un mapa normal.
5. Para habitaciones cuya anchura real es menor que la vista extendida,
   se prolonga visualmente la franja exterior del fondo por reflexión,
   por preferencia expresa del usuario. El escenario previo al
   mapamundi mide 256x512 frente a una vista de 284x160. No existe terreno
   adicional después de ese límite; la antigua franja era otra capa de
   paralaje expuesta fuera de la habitación. Se respeta el área original de 240 píxeles; solo se refleja la franja
   externa del fondo, sin estirar píxeles ni duplicar sprites o HUD.

## Reproducciones y evidencia

Estados aislados en `/tmp/legacy2-worldmap/` del equipo de compilación:

- `corner.state`: esquinas inferiores de la calle con Goku. Resultado
  `/tmp/legacy2-rain-work/corner-native-pad.png`: toda la fila inferior,
  incluidos x=0,21,22,261,262,283, contiene RGB(206,99,0), antes negro
  en los 22 píxeles exteriores de cada lado.
- `live.state`: calle, coche y palmera. `city-native.png` restaura piezas
  de las capas que la selección por comparación dejaba fuera.
- `projected-0.state` a `projected-2.state`: tres posiciones reales del
  mapamundi. Resultados `fixed-0.png` a `fixed-2.png`, cielo extendido.
- `entry-current.state`: escenario estrecho previo al mapamundi.
  `/tmp/legacy2-rain-work/entry-reflected.png`: textura exterior reflejada,
  sin franja falsa de hierba ni banda negra.
- `/tmp/door-before.state`: gráfico OAM5, tile380, 32x64. A 30 fotogramas
  quieto aparece en X=-33 con la consulta de dibujo ampliada; no se fuerza
  el estado de la puerta. Detalles en `puertas-investigacion.md`.
- Modo original 240: `cmp /tmp/door-probe/before-240.png
  /tmp/legacy2-worldmap/final240.png` satisfactorio.

Reproductor fuera de la compilación normal:
`/tmp/legacy2-rain-work/progress_rain.py STATE ETIQUETA 30 '[[0,1023]]'`.
Usa ejecutable de escritorio, guardado aislado y vista de 284 píxeles.

## Alcance de la comprobación

Los cambios de capas y relleno están comprobados en los estados anteriores.
No se da por demostrada la eliminación de todos los casos de palmeras:
si una pieza desaparece todavía, capturar el estado antes/después y seguir
su método de visibilidad/emisor, sin ampliar la lógica de activación.
La conversión de coordenadas por chunks ensayada no mejoró el resultado y
se retiró; no debe reintroducirse como supuesto arreglo.

No se publica en GitHub, por instrucción del usuario.

## Validación final

Pruebas de hooks de producción: 64 casos de rutas de lectura, OAM, contexto
virtual de dibujo, extensión visual de habitaciones estrechas, memoria sin
cambios por las consultas y modo original desactivado. Compilación Android
arm64 offline. La extensión sustituye a la máscara negra inicial.

## Corrección de paralaje durante el movimiento

La primera prolongación reflejaba todas las capas usando cámara X. La
captura 17:53:59 mostró una franja negra móvil: en el estado inicial cámara
X=16, pero BG3HOFS=3. Un píxel reflejado respecto a cámara aún podía quedar
fuera del recurso BG3. Se corrige usando el scroll completo y la anchura
del recurso de cada capa. Los recursos periódicos conservan su envoltura.
Regresión: 374 muestras de margen con origen de paralaje entre 0 y 16,
además de las 64 comprobaciones anteriores. Reproducciones de movimiento
en `entry-current.state`, con desplazamientos a izquierda y derecha.

## Entrega y reproducción conservada

APK arm64 compilado correctamente e instalado por ADB el 16-11-2026 tras
la corrección de paralaje. Se conserva y restaura el estado suspendido del
usuario; el slot temporal F9 se retira o devuelve a su contenido anterior.
Pruebas de hooks: 438 casos satisfactorios. Movimiento aislado: 90 fotogramas
a izquierda y otros 90 de regreso a derecha; sin franja negra entre el
fondo reflejado y el original en las imágenes finales. Se comprobó también
la captura real de la Thor después de instalar.

Reproductor y estados conservados en `tools/diagnostics/widescreen/`, fuera
de la compilación normal; consultar su README. La palmera permanece visible
en el margen izquierdo al reproducir 10 y 20 fotogramas de avance desde
`palm-edge.state`. Esto no cubre todos los objetos y mapas posibles.

## HUD y fondos: revisión posterior

Se retira por completo la clasificación de HUD por slots OAM 0..2/8..10
y posición X<96,Y<32. La barra conserva todas sus coordenadas originales
dentro del viewport central 3:2, por petición expresa del usuario. No se
clasifica ningún sprite como HUD ni se desplaza un actor por su slot o tile.

Mapamundi: aislamiento de BG0 confirma relleno oscuro en los últimos 16
píxeles de su ring de 256. BG0 contiene niebla/minimapa; BG1 contiene cielo.
Solo los márgenes de BG0 toman su columna nativa válida más cercana.

Muerte: nivel observado 082EB82C, recursos 082EB360/082EB530/082EB6B8/
082EB7D8, todos de constructor 08005F49. Son una presentación de planos
periódicos, sin terreno fuera del viewport. Cuando todas las capas activas
son de esa clase (al menos dos), se prolonga por reflexión el área nativa
de 240 píxeles, conservando el scroll vertical y el cartel original.
La lluvia de la escena de Trunks tiene capas de terreno normales además
de su capa periódica, por lo que no recibe esta transformación.

Se añaden 512 casos de actores junto a la esquina con distintos slots y
tiles, y seis comprobaciones de niebla, cielo y fondo periódico. Los
estados de diagnóstico se conservan fuera de la compilación normal.

## Extensión izquierda en mapas anchos

Se amplía la continuación visual izquierda a cualquier mapa finito cuando
la muestra queda en X<0 del recurso de la capa. Antes solo se prolongaban
habitaciones más estrechas que 284 píxeles, por lo que un mapa ancho con
cámara en su límite izquierdo aún mostraba una franja negra. El terreno
real disponible se conserva; solo se refleja la franja de fondo que queda
fuera del mapa. No cambia el HUD, los sprites ni el viewport nativo 3:2.

## Extensión derecha en mapas anchos

La prolongación visual también se aplica cuando una muestra del margen
derecho supera la anchura real del recurso de su capa. Es simétrica a la
extensión izquierda: no sustituye terreno válido, no cambia sprites/HUD
ni modifica el área original 3:2. Se comprueba tanto un recurso de una
columna como el límite de cámara derecho de un mapa de 512 píxeles.

## Continuación de toda la ciudad, incluidos recursos con relleno

Estado real observado: nivel 082A9C3C, habitación 688x1208, cámara X=0.
BG1/BG2/BG3 usan scroll X=64/128/192: el borde real de la ciudad queda
dentro de sus recursos, precedido por relleno negro. Detectar solo X<0
del recurso no cubría ese caso. Ahora se refleja primero la muestra que
queda fuera de los límites lógicos de la habitación. Si vuelve al viewport
nativo, se usa directamente esa geometría; se conserva la corrección por
capa para los márgenes de paralaje que todavía queden fuera del recurso.

La regla se aplica a ambos extremos de cualquier ciudad/mapa, sin lista de
lugares ni parches por edificio. Solo afecta al fondo de los 22 píxeles de
margen; HUD, carteles, personajes y datos del juego permanecen originales.
Se prueban ambos extremos, los offsets de relleno 64/128/192, la entrada y
salida progresiva del límite y la conservación del terreno real disponible.

## Protección expresa de interiores

Por indicación del usuario, la prolongación cosmética de capas finitas
requiere un exterior ALFP confirmado visualmente. Se mantiene una lista
explícita y conservadora de los niveles de las capturas verificadas:
08236E30, 08297D88, 0829BBF4, 082A244C, 082A9C3C, 082B2C18, 082BA04C,
082DCE54. La regla de geometría es común a todos ellos y cubre ambos
extremos completos de cada ciudad. No hay excepciones por objeto o edificio.

Las habitaciones y los mapas desconocidos conservan sus límites y su
gráfica real: no se refleja una habitación más allá de su pared/espacio
vacío. Tampoco se desactiva la representación panorámica del terreno real
de esos mapas; solo se rechaza la prolongación cosmética inventada.
El interior real 082D2B44 (240x408) queda excluido y tiene regresión
explícita para ambos lados. Los fondos de presentación de nubes y cielo
mantienen sus transformaciones ya aprobadas, sin geometría jugable nueva.

## Línea negra al alejarse del borde: sincronía de dibujo

Reproducción confirmada desde `edge-flicker.state`: 12 fotogramas a derecha
producen una línea vertical negra; a 24 desaparece. Nivel 082A244C, punto
inicial cámara X=11, ciudad 584x1344. Recursos BG1/BG2/BG3 tienen escala X
1024 y offsets X 65536/131072/196608, es decir 64/128/192 píxeles.

Se elimina la combinación de cámara RAM viva con el scroll del BG que se
está renderizando. El margen se calcula solo con ese scroll y los campos
nativos del descriptor: escala +04 y offset +0C, ambos con 10 bits de
fracción. La cobertura horizontal de la capa va de offset>>10 a
((room_width-240)*scale+offset)>>10 + 240. Así se respeta también el
paralaje; no se calcula la cobertura como room_width*scale.

Una sola transformación continua sustituye al doble reflejo por habitación
y recurso. No se refleja el final de un foreground corto que simplemente
termina dentro del mapa. Se comprueba que cambiar cámara RAM +/-1 con el
BG invariable no altera ninguna muestra de margen, en ambos extremos.
Se mantienen la protección de interiores y las posiciones originales OAM.

## Selección de partida: fondo limpio sin UI reflejada

BG3 contiene solo la nebulosa; BG2 contiene título, mandos, bolas y textos;
BG1 contiene el marco de selección. Se amplía una sola vez la ventana
nativa de BG3 a 284 píxeles; BG1/BG2 permanecen dentro del área 3:2.
No se refleja ni se repite la composición del menú.

La selección retiene 082EB82C (nivel de muerte) aunque sus gráficos vivos
ya son otros. Eso la hacía pasar por el fondo de nubes. Ahora se reconoce
la nebulosa por ocho palabras de su primer tile y su tilemap nativo,
verificadas en dos estados de selección con distinto nivel previo. La
huella es independiente de paleta, idioma, ranura y scroll. Se excluye
esa presentación de la clasificación de campo y de su culling.

## Fondo inferior original

Se extrae la imagen completa 256x256 de la nebulosa original de BG3 desde
el estado nativo de selección: tilemap 1F, tiles de 8bpp y paleta del juego.
Asset `android/app/src/main/assets/fondo_nebulosa.png`.
`Legacy2MenuPresentation.EstadoView.fondoAnimado` la dibuja una sola vez,
con escala uniforme para cubrir y recorte centrado. No hay mosaico ni espejo.
Se conserva el dibujo posterior de plantilla, pestañas, cartel y contenido.
La rama de loading.png y sus cinco segundos no se modifica.
