# ESP32-S3 · WiFi propio (sin router, sin nube)

El ESP32 crea la red **SolarCleaner_PRO** (IP fija `192.168.4.1`, también `http://limpiezapaneles.local`) y **sirve la página web de control**: cualquier celular o PC entra desde el navegador, sin instalar nada. Con el portal cautivo, el celular suele abrir la página solo al conectarse. La app de Android/Windows usa la misma interfaz.

La página es la misma que `www/index.html`. Cada vez que la modifiques, regenera el archivo que se carga en el ESP32:
```
python3 tools/generar_pagina.py
```

## Cargar el programa
1. Arduino IDE → instala el núcleo **esp32 by Espressif 3.x** (Gestor de placas) y, en el Gestor de librerías, **RTClib** (Adafruit) y **Adafruit SSD1306** (con Adafruit GFX).
2. Placa: **ESP32S3 Dev Module**. Activa **USB CDC On Boot** para ver el monitor serie.
3. Abre `SolarCleanerPro_ESP32/SolarCleanerPro_ESP32.ino`.
4. **Cambia la contraseña** `AP_PASS` (mínimo 8 caracteres) y sube el programa.
5. Si todavía no conectaste el reloj o la pantalla, pon `USAR_RTC` o `USAR_OLED` en 0 al inicio del programa.

## Pines usados (revísalos contra tu montaje)
| Función | GPIO |
|---|---|
| Final de carrera derecho (NO a GND) | 6 |
| Final de carrera izquierdo (NO a GND) | 7 |
| Relé K1 (bomba) | 18 |
| Relé K2 (fuente ATX) | 5 |
| Pulsador verde de inicio (NO a GND) | 10 |
| I2C: SDA / SCL (RTC DS3231 y OLED SSD1306) | 8 / 9 |
| Hongo de emergencia (contacto NC a GND) | 11 |
| BTS7960 #1: RPWM / LPWM | 4 / 21 |
| BTS7960 #2: RPWM / LPWM | 15 / 16 |

Los pines de los BTS7960 son un **supuesto** (no estaba claro cuál va a cuál en el diagrama). Si un motor gira al revés, intercambia los dos GPIO de ese BTS en el programa.
Los relés se tratan como **activos en bajo** (lo habitual en módulos de 2 canales). Si encienden al revés, pon `RELE_ACTIVO_BAJO = false`.

## Caja de mando: pulsador de inicio y hongo de emergencia
- **Pulsador verde (GPIO10):** un contacto NO entre GPIO10 y GND. Al presionarlo inicia el ciclo de limpieza (solo si el sistema está en espera).
- **Hongo rojo (GPIO11):** usa un bloque de **2 contactos NC**. Un contacto va entre GPIO11 y GND: cerrado es normal; abierto o con el cable cortado cuenta como emergencia.
- **Corte por hardware (importante):** el segundo contacto NC va **en serie con la fase de 220 V que alimenta la fuente de 12 V** (alternativa del informe: el +5 V de la bobina de K2, con K2 en módulo propio). Así el hongo corta los 12 V de motores y bomba aunque el programa o el WiFi fallen, y queda enclavado hasta liberarlo. El ESP32 sigue encendido con su cargador de 5 V y avisa "PARO DE EMERGENCIA" en la página.
- Al presionar el hongo: se apaga todo y el sistema queda bloqueado. Al liberarlo (girar) queda en STANDBY y **no arranca solo**; hay que pulsar el botón de inicio de nuevo.
- **Si todavía no conectaste el hongo**, pon un puente entre GPIO11 y GND; si no, el ESP32 lo toma como emergencia permanente.
- La instalación de 220 V debe hacerla personal calificado. El detalle está dibujado en la Hoja 2 de la app.

## Ajustar el ciclo a la limpieza real
Los tiempos del ciclo son variables al inicio del programa (`T_FUENTE_S`, `T_PRELAVADO_S`, `T_PAUSA_S`, `T_FIN_S`, `T_RAMPA_S`, `T_MAX_ESTADO_S`, `T_MAX_BOMBA_S`, `pwmVel`, `BOMBA_EN_RECORRIDO`, `VUELTAS_POR_DEFECTO`).
- **Cambiarlos sin reprogramar:** en la app, hoja de datos (icono de tabla de la cabecera) → **Enviar al ESP32** (solo con el sistema en espera). La app los lee sola al conectarse. Al reiniciar el ESP32 vuelve a los valores del programa.
- **Dejarlos fijos:** en la app toca **Copiar para el código**, pega esas líneas en el programa y vuelve a subirlo.
- **Si modificas el programa:** toca **Pegar desde mi código** y pega tus líneas; la app los reconoce y recalcula agua, tiempo de ciclo y protecciones.
- Si cambias la **secuencia** del ciclo (fases nuevas, otro orden), la simulación de la app no la conoce y hay que adaptarla: mándame el código modificado.

## Primera prueba (sin riesgo)
1. Con la fuente ATX **desconectada** de 220 V, enciende el ESP32 por USB.
2. Conecta el teléfono a la red `SolarCleaner_PRO` y abre la página (`192.168.4.1`) o la app: se conecta sola.
3. Presiona a mano cada final de carrera: debes ver GPIO6 y GPIO7 reaccionar en la app.
4. Conecta la fuente y prueba **Izq. / Der.** con la escobilla libre y velocidad baja.
5. Prueba el pulsador de inicio y el hongo: con el hongo presionado nada debe arrancar, y al liberarlo no debe arrancar solo.
6. Recién después prueba el ciclo completo con la bomba y agua.

## Protecciones incluidas en el ESP32 (funcionan sin la app)
- Parada por final de carrera, y error si los dos finales se activan a la vez.
- Tiempo máximo de 50 s en cada estado del ciclo y de 10 min con la bomba encendida.
- Pausa de 0,4 s antes de invertir el giro y arranque suave.
- Modo manual: la escobilla se detiene sola en el final de carrera derecho, si algo queda encendido todo se apaga a los 2 minutos, y si se pierde la conexión más de 3 s los motores se detienen. El ciclo automático termina solo.
- Hongo de emergencia: apaga todo al instante y bloquea hasta liberarlo.
- Al arrancar todo queda apagado.

## Notas por sistema
- **Android:** la red no tiene internet. Si el teléfono pregunta, elige **mantener la conexión**. Si la app no responde, desactiva los datos móviles.
- **PC:** al conectarte a la red del ESP32 pierdes internet mientras tanto.
- La posición del carro se estima por tiempo y se corrige en cada final de carrera (no hay encoder).

## Programación horaria y mantenimiento
- La limpieza programada (cada 1, 2, 3, 7 o 15 días a la hora elegida) la ejecuta el ESP32 con su reloj DS3231, aunque no haya celular conectado. Se guarda en la memoria flash (Preferences) y no se pierde con un corte de energía.
- El reloj se ajusta desde la página con **Sincronizar hora con el teléfono** (hoja Programación).
- La pantalla OLED muestra estado, hora, red WiFi, vuelta en curso y próxima limpieza.
- Vigila la memoria libre y se reinicia solo a las 3:00 a. m. si lleva más de 2 horas encendido y está en espera.
- La contraseña de la red WiFi es la que está escrita en el programa (`AP_PASS`); el programa trae `solar1234`. No hace falta recordarla: se anota en una etiqueta dentro del gabinete, y si se pierde se cambia en el programa y se vuelve a cargar.
