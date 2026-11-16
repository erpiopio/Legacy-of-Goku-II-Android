# HABILIDADES en la pantalla inferior

Las tres entradas del STATUS original (especial físico y dos especiales
energéticos) se presentan en filas transparentes dentro del panel existente.
El nombre, tipo y descripción utilizan Pixel Operator y el mismo salmón
`#FFAA9B` del texto de MISIONES. Los iconos se amplían a 120×120 sin filtrado;
una habilidad bloqueada no dibuja icono. Las filas crecen con el texto completo
y se desplazan dentro del panel. La técnica activa recibe un borde dorado y
una sombra interior negra muy suave; tocar una fila no cambia ese resaltado
hasta recibir la selección confirmada del motor.

## Selección original ALFP

La ROM utilizada tiene SHA-1 `db36ff52fcd63f753f9d66439aa3d2216701c326`.
El manejador de L `0x0801D5AC` consulta el bit 9 del mando y llama en
`0x0801D5CE` a la rutina Thumb `0x08008B04`. Esta rutina recibe el registro
`0x030010B8 + personaje*0x1C`, cicla el byte `+0x09` módulo cuatro y omite
las posiciones cuyo bit `8 << posición` no esté desbloqueado en `+0x14`.
El tipo real de técnica se lee de `+0x18 + posición` y se emplea tanto en
el HUD (`0x0801D734`) como en el despachador de ataques (`0x08011FF2`).

Las posiciones nativas 0 y 1 corresponden a las dos especiales energéticas
de STATUS. Las posiciones 2 (transformación) y 3 (ki básico) no resaltan
ninguna de las tres filas de especiales. El ataque físico es informativo.

Android encola personaje, fila y generación; el motor valida de nuevo
personaje activo, desbloqueo y contexto, y ejecuta la rutina original de L
las veces necesarias para llegar al destino (cuatro iteraciones como máximo).
Una pulsación física de L tiene prioridad. Las peticiones se invalidan al
cargar estado, cambiar personaje o abandonar el escenario. No se escribe el
selector desde Java ni se guarda una selección ficticia en la presentación.

El snapshot JNI mantiene unidos estado, retrato, habilidades, inventario y
diario y añade selección e IDs gráficos bajo el mismo bloqueo. Los IDs de
energía proceden de los descriptores reales; el físico identifica la forma
actual mediante `-1 - record[8]`.

## Alcance de la entrega

Se conservan los recursos del panel, pestañas, cartel, LOADING de cinco
segundos y las vistas DIARIO, estadísticas de ESTADO, OBJETOS y MAPA.
No se ejecutan pruebas de selección en la Thor ni se desbloquean técnicas
en la partida del usuario, por indicación expresa. Compilar e instalar no
certifica la selección en juego.
