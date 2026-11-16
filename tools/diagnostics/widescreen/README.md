# Reproducciones de márgenes ALFP

Este directorio no forma parte de los objetivos de compilación ni del APK.
Los estados contienen una copia de memoria del juego. No se altera la partida.

Desde la raíz del proyecto, después de compilar escritorio:

```sh
python3 tools/diagnostics/widescreen/replay_frame.py tools/diagnostics/widescreen/states/entry-moving-gap.state edge-still 30 '[[0,1023]]'
python3 tools/diagnostics/widescreen/replay_frame.py tools/diagnostics/widescreen/states/entry-current.state edge-left 90 '[[0,991],[89,1023]]'
```

Salida predeterminada `/tmp/legacy2-widescreen-replay/`, modificable con
`LEGACY2_DIAGNOSTIC_OUT`. La ROM original y BIOS deben existir en sus rutas
habituales del proyecto. Se usa un guardado aislado de diagnóstico.

`entry-current`: habitación estrecha antes del mapamundi; `entry-moving-gap`:
posición real posterior a la captura del fallo de paralaje; `corner`: esquinas
inferiores negras; `live`: coche/capas de ciudad; `palm-edge`: palmera en el
margen izquierdo; `projected-0`: mapamundi real.

Hallazgos, límites de validación y direcciones en `docs/margenes-panoramicos.md`.
