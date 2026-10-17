# Protocolo de pruebas

Marcar cada punto y anotar el resultado. No pasar a la siguiente etapa si falla una.

## 1. Antes de energizar
- [ ] Revisión visual del cableado contra el diagrama (polaridad de 12 V, GND común).
- [ ] Fusible de 2 A y tierra (PE) conectados; caja cerrada.
- [ ] Contraseña de la red WiFi cambiada en el programa.
- [ ] Puente o hongo conectado en GPIO11 (el ESP32 lo lee como emergencia si está abierto).
- [ ] Hongo con contacto NC de potencia (10 A / 250 V) en serie con la fase de la fuente de 12 V; segundo contacto NC a GPIO11.

## 2. Solo ESP32 (fuente ATX desconectada)
- [ ] Aparece la red SolarCleaner_PRO y la página (192.168.4.1 o limpiezapaneles.local) abre en el celular.
- [ ] Finales de carrera: al presionar cada uno, la app cambia GPIO6 y GPIO7.
- [ ] Pulsador de inicio: la app registra "Pulsador de la caja".
- [ ] Hongo: al presionarlo aparece PARO DE EMERGENCIA; al liberarlo queda en espera y no arranca solo.
- [ ] Relés: K1 y K2 encienden cuando deben. Si encienden al revés, cambiar `RELE_ACTIVO_BAJO`.

## 3. Con fuente ATX, sin agua y sin escobilla
- [ ] K2 enciende la fuente; medir 12 V en cada riel.
- [ ] Izquierda y derecha mueven los motores en el sentido correcto (si no, intercambiar pines del BTS).
- [ ] Velocidad baja primero (PWM 100); subir de a poco.
- [ ] El carro se detiene en cada final de carrera.
- [ ] El hongo corta los 12 V aunque el programa siga corriendo.

## 4. Con el riel real
- [ ] Medir el tiempo de una pasada y actualizar la velocidad del carro en la hoja de datos.
- [ ] Revisar que el tiempo máximo de movimiento sea mayor que una pasada con margen.
- [ ] Un ciclo completo sin agua.

## 5. Con agua
- [ ] Filtro de malla antes de la bomba y depósito con agua.
- [ ] Medir el caudal total (por tiempo de llenado de un balde) y la presión de las boquillas.
- [ ] Comparar con los resultados de la Hoja 3 y corregir los datos de entrada.
- [ ] Ajustar prelavado y espera final según la limpieza obtenida.
- [ ] Verificar que no queden fugas ni agua cerca de la fuente y los relés.

## 6. Registro
| Fecha | Parámetros (prelavado, vueltas, PWM) | Tiempo de ciclo | Agua usada (L) | Resultado | Observaciones |
|---|---|---|---|---|---|
|  |  |  |  |  |  |
|  |  |  |  |  |  |

Cuando los valores sean definitivos, copiarlos con **Copiar para el código** (hoja de datos) y pegarlos en el programa del ESP32.

## 7. Tabla de verificación (modelo del apartado 3.2.9 del informe)
Completar la columna "Valor obtenido" con las mediciones de campo; el estado es Cumple o Ajuste.

| Prueba | Parámetro verificado | Valor de referencia | Valor obtenido | Estado |
|---|---|---|---|---|
| 1. Alimentación de control | Tensión de la línea de 5 V al activar los relés | ≥ 4,7 V |  |  |
| | Corriente de control en ciclo | 450 a 550 mA |  |  |
| | Corriente de control en reposo | 150 a 250 mA |  |  |
| 2. Fuente de 12 V | Tensión sin carga | 12 V |  |  |
| | Corriente total en marcha (2 motores + bomba) | < 17 A |  |  |
| 3. Motorreductores | Corriente de cada motor en marcha | ≈ 5 A (nominal) |  |  |
| | Fuerza de arrastre del carro (dinamómetro) | Torque necesario < 0,20 N·m |  |  |
| | Tiempo de una pasada | < 50 s |  |  |
| 4. Bomba y boquilla | Caudal en la salida (volumen ÷ tiempo) | Valor de la Hoja Cálculos |  |  |
| | Abanico de agua sobre la zona de limpieza | Cubre el ancho de la escobilla |  |  |
| 5. Secuencia | Fuente, espera 1 s, prelavado 5 s | 1 s y 5 s |  |  |
| | Vueltas por ciclo | 2 o 3 |  |  |
| | Estado que supera 50 s | Detención de seguridad |  |  |
| 6. Programación | Limpieza programada a la hora elegida | Ejecuta el ciclo |  |  |
| | Programación tras cortar y volver la energía | Se conserva |  |  |
| 7. Seguridad | Hongo de emergencia | Corta los 12 V y no rearranca solo |  |  |
| | Modo manual sin detener | Todo apagado a los 2 min |  |  |
| 8. Interfaz | Acceso desde el celular sin internet | Página en 192.168.4.1 |  |  |

**Registro de ajustes (3.2.9.1):** anotar para cada "Ajuste" la causa, la acción realizada y el resultado final.

| Ajuste | Causa | Acción realizada | Resultado final |
|---|---|---|---|
|  |  |  |  |
