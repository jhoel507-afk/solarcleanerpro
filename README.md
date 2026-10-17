# SolarCleanerPro™ v2

Sistema automático de limpieza de paneles solares con control por ESP32-S3.

**Autores y titulares:** Leon Mena, Ángel Joel · Gallegos Valencia, Cristian Augusto
© 2026. Todos los derechos reservados (ver `LICENSE.txt`). Marca y autoría: `docs/MARCA_Y_AUTORIA.md`.
**Institución:** SENATI · Electrotecnia Industrial y Energías Renovables

## Descripción
Un carro motorizado con escobilla de nailon recorre una guía V-Slot 2020 sobre los paneles, mientras boquillas de abanico plano (fijas, una por panel; la app permite también una boquilla en el carro) riegan el vidrio. El ESP32-S3 controla la fuente de 12 V y la bomba con relés, dos motorreductores PG42-775 con puentes H BTS7960, y lee dos finales de carrera WS1. Un reloj DS3231 permite la limpieza programada y una pantalla OLED muestra el estado. La caja de mando tiene un pulsador de inicio y un hongo de emergencia en serie con la fase de la fuente.

El ESP32 crea su propia red WiFi (**SolarCleaner_PRO**) y sirve la página web de control en `http://192.168.4.1`, sin internet ni aplicaciones. El proyecto incluye además la **aplicación** (Android y Windows) con la misma interfaz, que simula el sistema, hace los cálculos de dimensionamiento y se conecta al ESP32.

## Estructura
```
www/            Aplicación (HTML, un solo archivo)
esp32/          Programa del ESP32-S3: pestaña para tu programa original (SolarCleanerPro_ORIGINAL) y programa alternativo (SolarCleanerPro_ESP32)
docs/           Manual de usuario, protocolo de pruebas, revisión del programa original, marca y autoría
tests/          Pruebas automáticas de la aplicación (npm test)
tools/          Genera la página web que se carga en el ESP32
electron/       Empaquetado para Windows
assets/ build/  Iconos y pantalla de arranque
.github/        Compilación automática del APK y del .exe
```

## Ciclo de limpieza
1. K2 enciende la fuente ATX.
2. K1 enciende la bomba (prelavado).
3. Los motores llevan la escobilla hasta el final de carrera derecho, pausan y regresan hasta el izquierdo; cada regreso es una vuelta.
4. Al completar las vueltas se apagan bomba y fuente.

Todos los tiempos, las vueltas, la velocidad y si la bomba sigue encendida durante el recorrido son parámetros editables (Hoja 3 de la app).

## Aplicación
- **Mecanismo:** esquema en planta animado, con 3 paneles (ampliable a 4).
- **Conexionado y funcionamiento:** diagrama de conexiones animado, ciclo explicado y detalle de la caja de mando.
- **Hoja de datos (oculta y con PIN):** se abre con el icono de tabla de la cabecera o con «Editar datos, precios y tiempos» en Cálculos. Ahí se escriben los precios de los materiales (4.1), las horas de cada tarea (4.2), los datos de la empresa y todos los supuestos técnicos; el resto de las hojas solo muestra resultados y se recalcula solo. Al abrirla por primera vez se crea un PIN de 4 a 8 caracteres (o se deja sin PIN); se bloquea al salir. El PIN evita cambios accidentales y no es una seguridad total. Los datos y el PIN se guardan en cada equipo y en cada dirección (192.168.4.1 y limpiezapaneles.local cuentan como distintas); la hoja incluye una copia de seguridad para pasar los datos de un equipo a otro. Si se olvida el PIN, la única salida es borrar los datos de esa hoja en ese equipo.
- **Cálculos:** caudal y presión en las boquillas, consumo de agua, dimensionamiento del motor, cables y fusible, margen eléctrico, vida útil y retorno de la inversión, con datos editables.
- **Costos y retorno (Capítulo IV):** tabla 4.1 de materiales (producto, marca, unidad, costo unitario, cantidad, total), tabla 4.2 de tiempo empleado con costo de mano de obra, costo total 4.1 + 4.2, datos de la empresa editables y escenarios según la pérdida por suciedad.
- **Texto para el informe:** genera la redacción, con el formato de la guía, de 3.2.3 (dimensionamiento), 3.2.4 (selección), 3.2.10 (especificaciones), 4.1, 4.2, 4.3.1 y 4.5 con tus fórmulas y valores, y lista qué datos faltan por medir.
- **Programación:** limpieza cada 1, 2, 3, 7 o 15 días a la hora elegida (la ejecuta el ESP32 con su reloj), sincronización de la hora y formas de uso.
- Se conecta sola al ESP32 cuando el teléfono o la PC está en su red WiFi; sin ESP32 funciona como simulación.

## Compilar la aplicación
**Sin instalar nada (GitHub Actions):** sube la carpeta a un repositorio, abre *Actions → Build APK y EXE → Run workflow* y descarga los resultados en *Artifacts* (`app-debug.apk` y los `.exe`).

**En tu computadora** (Node 20, Java 17 y Android Studio):
```
npm install
npx cap add android
sed -i 's/<application/<application android:usesCleartextTraffic="true"/' android/app/src/main/AndroidManifest.xml
npx @capacitor/assets generate --android
npx cap sync android
cd android && ./gradlew assembleDebug
```
APK: `android/app/build/outputs/apk/debug/app-debug.apk`. Windows: `npm run dist:win` (carpeta `dist/`). Probar sin empaquetar: `npm start`.
El APK es de tipo *debug* (instalable directo). Para Google Play hace falta firmarlo con tu propio keystore.

## ESP32
**Recomendado: el programa original de ustedes + la pestaña `Estado_API`** (carpeta `esp32/SolarCleanerPro_ORIGINAL/`, ver `LEEME_ORIGINAL.md`): se agregan una pestaña y 3 líneas; la app se conecta con las rutas del programa original, muestra el estado en vivo y suma el pulsador de inicio y la lectura del hongo. La revisión del programa está en `docs/REVISION_DEL_PROGRAMA_ORIGINAL.md`.

Alternativa: `esp32/SolarCleanerPro_ESP32/` es un programa completo equivalente que también sirve la página web desde el ESP32 y permite cambiar los parámetros desde la app (guía en `esp32/LEEME_ESP32.md`; si se modifica `www/index.html`, ejecutar `python3 tools/generar_pagina.py`).

La app detecta sola cuál de los dos programas está conectado.

## Seguridad
- El hongo de emergencia corta la fuente de 12 V por hardware (contacto NC en serie con la fase de 220 V de la fuente), sin depender del ESP32 ni del WiFi. Conexionado completo: `docs/conexionado/`.
- Las protecciones de recorrido y de bomba viven en el ESP32 y no dependen de la app.
- La instalación de 220 V debe hacerla personal calificado.
- Antes de operar con agua y motores, seguir `docs/PROTOCOLO_DE_PRUEBAS.md`.

## Estado y limitaciones
**Hecho:** aplicación con simulación, cálculos, hoja de datos con PIN, programa del ESP32, documentación y 38 pruebas automáticas (`npm test`) que revisan cálculos, textos del informe, valores extremos, el PIN y que la página del ESP32 esté actualizada.

**Todavía no verificado (hay que hacerlo antes de entregar):**
- El programa original más la pestaña, y el programa alternativo, se revisaron con un compilador de prueba y una simulación de su lógica, pero **no se han compilado con las librerías reales ni probado en la placa**. Pueden necesitar pequeños ajustes.
- La compilación del APK y del .exe con GitHub Actions no se ha ejecutado todavía.
- La interfaz se probó con pruebas automáticas, no en pantallas reales de teléfono y computadora.
- Muchos datos de cálculo son supuestos hasta medirlos en campo (ver «Datos que aún tienen el valor por defecto» en el texto del informe).
- El diagrama eléctrico (lámina) del proyecto debe actualizarse con el hongo, el pulsador, el reloj y la pantalla; en la app están dibujados aparte.
- La protección ante atascos de los motores depende de fusible, límite de tiempo y hongo: no hay medición de corriente.
