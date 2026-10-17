# Manual de usuario · SolarCleanerPro™

Para quien opera el sistema de limpieza de paneles. No hace falta instalar nada: basta un celular o una computadora con WiFi.

## 1. Conectarse
1. Enciende el sistema (la pantalla OLED muestra STANDBY, la hora y la red).
2. En el celular, conéctate a la red WiFi **SolarCleaner_PRO** (la contraseña es la del programa del ESP32 y debe estar anotada en la etiqueta del gabinete). La red no tiene internet: si el teléfono pregunta, elige mantener la conexión.
3. Abre el navegador en `http://192.168.4.1` o `http://limpiezapaneles.local`. Muchos teléfonos abren la página solos.

## 2. Hojas de la página
| Hoja | Para qué sirve |
|---|---|
| Mecanismo | Ver el carro, la bomba y los finales de carrera en tiempo real. |
| Conexionado | Diagrama de conexiones, funcionamiento del ciclo y detalle de la caja de mando. Se puede ver en pantalla completa. |
| Cálculos | Resultados de agua, motor, cables, vida útil y retorno de la inversión. |
| Programación | Limpieza automática cada 1, 2, 3, 7 o 15 días a una hora elegida. |

La barra de arriba (o el panel lateral en computadora) tiene los controles. La hoja de datos, protegida con PIN, se abre con el icono de tabla de la cabecera.

## 3. Formas de limpiar
- **A pedido:** botón **Iniciar ciclo** de la página, o pulsador verde de la caja. El ciclo enciende la fuente, riega (prelavado), recorre el panel de ida y vuelta (2 o 3 vueltas) y se apaga solo.
- **Programada:** en la hoja Programación, elige el estado Activada, cada cuántos días y la hora. Antes, toca **Sincronizar hora con el teléfono** para ajustar el reloj. La limpieza se ejecuta aunque no haya ningún celular conectado.
- **Manual:** **Bomba** y **Escobilla** se encienden por separado. La escobilla avanza a la derecha y se detiene sola en el final de carrera. Si algo queda encendido, todo se apaga a los 2 minutos.

## 4. Detener el sistema
- **Detener todo** en la página apaga bomba, fuente y motores.
- **Hongo de emergencia (rojo) en la caja:** corta la fuente de 12 V al instante, aunque falle el WiFi o la página. Queda enclavado. Para volver a usar el sistema, primero se revisa por qué se detuvo, se gira el hongo para liberarlo y se inicia de nuevo con el pulsador o la página. **No arranca solo.**

## 5. Mensajes de alarma
| Mensaje | Qué significa | Qué hacer |
|---|---|---|
| PARO DE EMERGENCIA / PARADA DE EMERGENCIA (hongo) | Se presionó el hongo o se cortó su cable. | Revisar la causa y liberar el hongo. |
| ERROR: tiempo excedido, revisar finales de carrera | Un paso del ciclo duró más de 50 s (carro atascado, final de carrera que no cierra). | Revisar el riel y los finales de carrera; usar Detener todo. |
| ERROR: finales de carrera en conflicto | Los dos finales se activaron a la vez. | Revisar cableado y posición. |
| Sin conexión | La página perdió el contacto con el ESP32. | Revisar que el teléfono siga en la red SolarCleaner_PRO. El ciclo en marcha termina solo. |

## 6. Mantenimiento recomendado
| Elemento | Cuándo | Qué hacer |
|---|---|---|
| Filtro de la bomba y boquillas | Cada mes (o antes si baja el chorro) | Limpiar la malla y destapar las boquillas; con agua dura, desincrustar. |
| Escobilla de nailon | Cada 2 años o si deja de limpiar | Reemplazar. |
| Riel y ruedas | Cada 6 meses | Limpiar el polvo y revisar el juego del carro. |
| Conexiones del gabinete | Cada 6 meses | Revisar bornes y que no entre humedad. |
| Pila del reloj (CR2032) | Cada 5 años | Reemplazar y volver a sincronizar la hora. |
| Prueba del hongo | Cada 3 meses | Presionarlo con el sistema en marcha: debe cortar todo. |
| Nivel del depósito | Antes de cada limpieza | Comprobar que hay agua: la bomba no debe funcionar en seco. |

Los plazos son orientativos; los de la escobilla, la pila y las boquillas salen de la hoja Cálculos con los datos actuales.

## 7. Seguridad
- Dentro del gabinete hay 220 V: solo personal calificado debe abrirlo.
- Quien se conecte a la red WiFi puede operar el sistema; no compartan la contraseña.
- Con el carro en movimiento, mantener las manos fuera del riel.

© 2026 Leon Mena, Ángel Joel · Gallegos Valencia, Cristian Augusto. Todos los derechos reservados.
