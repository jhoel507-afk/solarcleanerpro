# Revisión del programa original (SolarCleanerPro v2)

Revisión de lectura y de compilación de prueba. **No es una auditoría completa**: no se probó en la placa ni con las librerías reales.

## Lo que está bien
- Compila sin errores ni advertencias en un compilador de prueba con las librerías simuladas (revisa sintaxis y nombres, no el comportamiento en la placa).
- Salidas seguras al arrancar: los relés se ponen en alto antes de ser salidas y los motores en cero.
- Protecciones: tiempo máximo por estado (50 s), modo manual limitado a 2 minutos, finales de carrera en conflicto, vigilancia de memoria y reinicio preventivo.
- Programación guardada en memoria permanente, hora corregida desde el celular y página que se actualiza sola.
- Compatible con el núcleo 2.x y 3.x del ESP32 (PWM y vigilante).

## Observaciones y sugerencias
| Tema | Observación | Sugerencia |
|---|---|---|
| Hongo y pulsador | El programa no lee el hongo ni tiene el pulsador físico de inicio. | Incluidos en la pestaña `Estado_API` (GPIO10 y GPIO11). |
| Contraseña WiFi | `SolarPro2026` figura en el programa y, si el informe se publica, quedaría expuesta. Con `USAR_CLAVE_WEB = false`, quien entre a la red puede operar el sistema. | Cambiarla antes de la instalación final o activar `USAR_CLAVE_WEB` (el `/stop` nunca pide clave). |
| PWM de los motores | 1 kHz suena (zumbido) en los motores. | Probar 20 kHz (`PWM_FRECUENCIA`); el BTS7960 lo soporta. |
| Arranque de motores | Arrancan de golpe a la velocidad fija; los motores en bloqueo toman hasta 35,7 A. | Un arranque suave (subir el PWM en unos 0,4 s) reduce el pico de corriente y el golpe mecánico. |
| Tiempo por estado | Con 4 paneles (4,58 m) la pasada debe durar menos de 50 s con margen. | Medir el tiempo de pasada real y ajustar `VELOCIDAD_MOTOR` o `TIMEOUT_SEGURIDAD`. |
| Bomba en seco | No hay sensor de nivel. | Agregar un sensor de nivel o un flotador antes de operar sin supervisión. |
| Demoras | Las pausas con `delay()` (1 s al encender la fuente, 0,5 s al invertir) bloquean la página unos instantes. | Es aceptable; si se nota, pasar esas esperas a la máquina de estados. |
| Hora de respaldo | Si el reloj pierde la energía toma la hora de compilación. | La página la corrige sola; revisar la pila CR2032. |
| Protección de motores | Sin medición de corriente: la única protección ante atascos es el tiempo máximo y el fusible. | Conectar R_IS/L_IS de los BTS7960 o añadir un limitador. |

## Qué cambió en la app para que coincida con este programa
- Rutas y comandos de la app = los de este programa (ver `LEEME_ORIGINAL.md`).
- Valores por defecto de los cálculos y de la simulación = constantes de este programa: pausa de 0,5 s, velocidad PWM 180, sin espera final, 50 s por estado.
- La escobilla manual de la simulación se comporta como en este programa: avanza a la derecha, regresa sola y se detiene al inicio.
