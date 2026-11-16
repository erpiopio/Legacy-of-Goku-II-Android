# Menú táctil inferior — Legacy of Goku II (ALFP)

Referencias visuales del usuario: `Screenshot_20261113-173545.png` (Diario/Misiones) y `Screenshot_20261113-173611.png` (Estado). Recursos gráficos suministrados: `plantilla.png`, `pestaña.png` y `cartel.png`. Estos archivos están en `Desktop/legacy 2` y aún no se han integrado al APK.

## Navegación

- Los rótulos y flechas de la pantalla inferior se pulsan con el dedo para cambiar de sección. La pantalla superior continúa mostrando el juego.
- El panel azul es el contenedor común. Diario muestra Misiones; Estado muestra las estadísticas; Objetos muestra el Inventario. El contenido cambia dentro del panel, sin abrir el menú GBA sobre el juego.
- La pestaña activa queda delante; las demás se oscurecen y desplazan. Al cambiar de sección se conserva el fondo morado animado y se intercambia el orden visual de las pestañas.
- Las flechas dentro del panel se usan para recorrer contenido largo. Los toques de navegación no se reenvían como botones del juego.

## Inventario

- Al entrar en Objetos se consulta un snapshot coherente de los objetos poseídos en la partida ALFP. El catálogo gráfico o los nombres por sí solos no representan posesión.
- Cada entrada muestra el nombre original, icono cuando esté disponible y cantidad real. Tocar un objeto lo selecciona y muestra sus detalles en el mismo panel.
- El objeto seleccionado puede consumirse mediante una acción táctil explícita **Usar**. La selección por sí sola no consume nada.
- **Usar** solo se habilita si el motor confirma que el objeto es utilizable en ese momento y su cantidad es positiva. Los objetos de misión o no consumibles se muestran, pero no ofrecen consumo.
- La acción solicita al motor original el uso del ID de objeto ALFP; la interfaz no cambia cantidades ni PV/PE por su cuenta. Tras la respuesta se vuelve a leer el estado real y se actualizan inventario y estadísticas. Si el uso falla o se cancela, la cantidad no cambia.
- Mientras una solicitud esté pendiente, se impiden toques repetidos de **Usar**. Tras cargar partida o cambiar de personaje, se invalida la selección que ya no exista.

## Idioma

- La pantalla inferior lee el idioma activo de la partida ALFP (inglés, francés, alemán, italiano o español) desde el estado real del juego. No lo deduce del idioma de Android.
- Los rótulos de pestañas, Misiones, Inventario, detalles y acciones táctiles usan ese mismo idioma. Al cambiarlo en el juego o cargar una partida con otro idioma, la pantalla inferior se actualiza sin reiniciar la aplicación.
- Los nombres y descripciones originales se extraen de las tablas de texto correspondientes de la ROM importada. Los textos propios de la interfaz inferior tienen traducciones para los cinco idiomas.
- Antes de implementarlo se debe verificar en ALFP la variable de idioma, su codificación, su valor inicial y su comportamiento al cambiar idioma y al cargar partida. Las direcciones preliminares del documento de contexto no son offsets confirmados.

### Hallazgo ALFP verificado hasta ahora

La tabla de cinco punteros en ROM `0x0802A964` contiene, por orden, `English`, `Français`, `Deutsch`, `Italiano` y `Español`. La rutina Thumb en `0x0800BE28` lee un byte desde `0x03001213` y lo utiliza como índice de esa tabla para mostrar el nombre del idioma. La ROM contiene el puntero base `0x03001210` en `0x0802B1C8`. En una captura de RAM antes de elegir idioma el byte vale `0`. Una reproducción controlada de cuatro pulsaciones hacia abajo y A en el selector original lo cambió a `4`, y el juego avanzó a la pantalla Atari. Una ejecución posterior con una copia aislada del guardado creado recuperó el valor `4` en ese mismo byte a los 600 fotogramas. Esto confirma inglés y español y la persistencia del idioma; aún falta validar los índices intermedios y enlazar el snapshot a Android.

## Verificación pendiente

La integración requiere que el arranque ALFP sea estable, que la aplicación Android y la pantalla inferior existan, y que se identifiquen para esta ROM las estructuras de inventario, la rutina original de uso y el estado del idioma. Probar en la Thor: navegación táctil, selección, uso de un consumible, rechazo de un objeto no utilizable, actualización de cantidad/PV/PE, persistencia tras guardar y cargar, y cambio de idioma reflejado en ambas pantallas.
