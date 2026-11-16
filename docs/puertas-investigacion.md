# Puertas en los márgenes panorámicos — ALFP / AYN Thor

Fecha: 16 de noviembre de 2026. Estado: **investigación abierta; ningún parche de puertas instalado**.

## Alcance y restricciones

Proyecto remoto: `/home/plex-rober/legacy-goku2-thor`. ROM europea ALFP de
8.388.608 bytes, SHA-1 `db36ff52fcd63f753f9d66439aa3d2216701c326`.
Corregir únicamente la representación gráfica. No cambiar entidades,
colisiones, estados lógicos, interacciones ni transiciones. Mantener los
arreglos previos de lluvia y menús. No publicar en GitHub.

## Reproducción conservada

- Estado inicial: `/tmp/door-before.state` (GBAS v6), capturado de la Thor.
- Fotograma inicial: 22346. La puerta parece abierta en el margen izquierdo.
- Reproducción de aproximación: 100 fotogramas; mando activo bajo:
  `{0:0x03FF, 1:0x03DF, 80:0x03FF}` relativos al inicio.
- CSV absoluto: `/tmp/legacy2-rain-work/door-near.csv`.
- Resultado: `/tmp/legacy2-rain-work/door-near.state` y `.png`.
- Se usaron guardados aislados en `/tmp/door-probe/`, sin sustituir el
  guardado de cartucho. El estado GBAS y las capturas no deben publicarse.

## Hallazgos confirmados

1. Antes de acercarse hay cinco entradas OAM activas. Después hay seis.
2. La nueva entrada es OAM 5 y corresponde visualmente a la puerta cerrada:
   sprite de 32×64, 8 bpp, tile inicial `0x380`, prioridad 2.
3. Primer valor distinto de deshabilitado registrado en su transferencia:
   `0xC1E1A011` (attr0=`0xA011`, attr1=`0xC1E1`), es decir X firmado **−31**,
   Y=17. El sprite empieza a intersectar el ancho original de GBA.
4. Más cerca: attr0=`0xA011`, attr1=`0xC005`, attr2=`0x0B80`: X=5, Y=17.
5. DMA3 transfiere esa entrada desde **`0x030022E4`** a **`0x07000028`**
   dentro del envío de OAM completo (256 palabras de 32 bits, CNT_H=`0x8400`).
   La base de la lista fuente se deduce como `0x030022BC`.
6. Esto demuestra cuándo aparece en la lista de dibujo; **no demuestra**
   que se cree entonces la entidad ni que cambie el estado lógico de la puerta.

La observación del usuario indica que el cambio puede ocurrir antes de que
la puerta entre por completo en 3:2. Es compatible con una prueba de
intersección del rectángulo del sprite; aún falta identificar su condición.

## Datos de la escena

`DISPCNT=0x1F40`, descriptor de nivel `0x08297D88`.

| BG | BGCNT | Scroll inicial X,Y | Recurso | Objeto resuelto |
|---|---|---|---|---|
| 0 | 1C83 | 01B8,00B7 | 082944DC | 00000000 |
| 1 | 1D81 | 01F8,00F7 | 08294F94 | 02035EA0 |
| 2 | 1E83 | 0238,0137 | 08295890 | 02033E70 |
| 3 | 1F83 | 0278,0177 | 082977B8 | 02031E40 |

Los cuatro recursos usan constructor `0x080056F9`. Existe un objeto de
terreno BG0 candidato en `0x0202FC70`, pero sus slots de chunks son `FF`
y las muestras de terreno inspeccionadas están vacías. Esto no identifica
la puerta como BG0.

## Vías ya descartadas o no demostradas

- Investigación previa: BG1–BG3 coincidían con VRAM (cero diferencias);
  cobertura izquierda 60/60. La caché RAM→ROM no solucionó el fallo.
- Culling previo: lecturas `0x0801534E` / `0x08015348` activas;
  `0x0801615C` / `0x08016156` sin llamadas. Contar llamadas no demuestra
  qué condición elimina este sprite.
- Prueba nueva y aislada: permitir BG0 envuelta cuando no se resuelve su
  objeto **no solucionó** la puerta y produjo márgenes incorrectos. Retirada.
- Watchpoints de `runtime_trace_event` sobre `0x030022E4`, con valores
  `0xC1E1A011` y `0xA011`, no dispararon durante la reproducción.
  No se ha demostrado si la escritura sucede por DMA, copia BIOS u otra
  ruta que no pasa por ese evento. No inferir una rutina de esos intentos.

## Estado de fuentes y diagnósticos

Fuente principal: `src/legacy2_extended_view_europe.cpp`.
Backup anterior a estas pruebas: `/tmp/legacy2-doors-before.cpp`.
Se confirmó igualdad binaria entre ambos tras retirar la prueba de BG0.
El APK de la Thor no recibió ningún cambio de puertas.

Diagnósticos útiles, fuera de los objetivos normales de compilación:
`tools/diagnostics/puertas/`. `probe_door.cpp` incluye la fuente de producción
para inspección de una copia de memoria; no es código para incorporar al APK.
En GBAS las secciones son `BUS0` e **`IO_0`** (no `IO00`). BUS0 concatena
EWRAM, IWRAM, paleta, VRAM y OAM; VRAM comienza en offset `0x48400`.

## Último paso dirigido y continuación

Seguir quién escribe el elemento de lista `0x030022E4`, antes de DMA3.
Un watchpoint nativo sobre su dirección anfitriona permite observar también
copias BIOS/DMA que el watchpoint de código recompilado no ve. Capturar PC
invitado, LR y pila de llamadas en el primer valor con attr0=`0xA011`.
Después retroceder desde el emisor OAM a su comprobación de visibilidad.

Solo si se demuestra que esa comprobación es puramente de dibujo y usa el
ancho original, ampliar sus límites hasta −22 y 262 para la vista de 284×160.
Validar ambos márgenes y el modo fiel de 240×160, manteniendo idénticas las
colisiones, los eventos y los flags. Si no se identifica la condición exacta,
detenerse sin parche.


## Actualización: solución común de dibujo, 16-11-2026

Watchpoint nativo confirmado: escritor invitado 0801D27A, emisor0801D23E. Visibilidad0801D206: comprueba enable+10 sin modificarlo y cruza bbox+18..24 con cámara03000F3C..48. LecturasX en0801D218(derecha) y0801D21E(izquierda). Caja inicialdelgráfico:407..439, cámaraizquierda440; por eso se descartaba aunque fuera visible enel margen. Se amplían solo esas lecturas de dibujo, con el desplazamiento real g_render_view_shift. No se cambian flags, entidades, colisionesni eventos.

La prueba de dos fotogramas no bastaba para actualizarla lista. A30fotogramas estacionarios el OAM5 aparece yaenX=-33 (attr1C1DF), con la puerta cerrada visible enel margen. Captura /tmp/legacy2-worldmap/static-fixed30.png. La comparación del modo240 conserva exactamenteelPNG anterior (cmp satisfactorio). No se incorporó la pruebaBG0 envuelta ni la selección alternativa de capas: no mejoraban la escena.

La misma corrección general de visibilidad se integra con las capas regulares del modo de vuelo, que conservan su textura nativa periódica. Capturas de regresión: /tmp/legacy2-rain-work/corner-general.png ycity-general.png, /tmp/legacy2-worldmap/fixed-0.png..fixed-2.png.
