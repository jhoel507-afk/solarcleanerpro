# Programa original + pestaña Estado_API

Tu programa original (SolarCleanerPro v2) **no se reemplaza**. Para que la app muestre el estado en tiempo real y para agregar el pulsador de la caja y la lectura del hongo, solo se agrega una pestaña y 3 líneas.

## Qué agrega
| Función | Detalle |
|---|---|
| `/estado` | Estado del sistema en JSON para la app (fase, bomba, fuente, escobilla, vueltas, finales de carrera, programación, hora del reloj, parámetros). |
| Pulsador de inicio | GPIO10, contacto NO a GND. Arranca el ciclo solo si el sistema está en espera. |
| Lectura del hongo | GPIO11, contacto NC a GND. Si se abre: apaga todo, lo muestra en la app y no deja arrancar hasta liberarlo. No arranca solo al liberar. |

El hongo corta la fuente por hardware, en serie con la **fase de 220 V que alimenta la fuente de 12 V** (alternativa del informe: el +5 V de la bobina de K2); GPIO11 solo le avisa al ESP32. El dibujo completo está en `docs/conexionado/`.
**Si todavía no conectaste el hongo**, pon un puente entre GPIO11 y GND, o `USAR_HONGO = false` solo para pruebas sobre la mesa.

## Cómo instalarla (3 pasos)
1. En el Arduino IDE, con tu programa abierto, crea una pestaña nueva con el nombre **Estado_API** y pega todo el contenido de `Estado_API.ino`. (O copia el archivo a la carpeta de tu programa y reabre el IDE.) Debe quedar después del archivo principal.
2. En tu archivo principal agrega estas **3 líneas** (marcadas con `// AGREGADO`):

   En `configurarWiFiYServidor()`, junto a los otros `server.on(...)`:
   ```cpp
   server.on("/estado", handleEstado);                 // AGREGADO (Estado_API)
   ```
   En `configurarPines()`, al final:
   ```cpp
   configurarCajaDeMando();                            // AGREGADO (Estado_API)
   ```
   En `loop()`, después de `server.handleClient();`:
   ```cpp
   vigilarCajaDeMando();                               // AGREGADO (Estado_API)
   ```
3. Compila y carga. Abre `http://192.168.4.1/estado` en el navegador: debe verse el JSON.

## Cómo se conecta la app con tu programa
La app detecta sola que es tu programa (el JSON trae `"fw":"orig"`) y usa tus rutas:
| En la app | Ruta de tu programa |
|---|---|
| Iniciar ciclo | `/auto` |
| Detener todo | `/stop` |
| Bomba | `/bomba` |
| Escobilla | `/motor` (avanza a la derecha, regresa sola y se detiene al inicio) |
| Programación | `/guardarTimer?dias=..&hora=HH:MM&vueltas=..` y `/cancelarTimer` |
| Sincronizar hora | `/ajustarHora?a=..&m=..&d=..&h=..&n=..&s=..` |
| Estado en vivo | `/estado` (la pestaña nueva) |

Red: **SolarCleaner_PRO**, contraseña **SolarPro2026** (la que está en tu programa, `WiFi.softAP`), `http://192.168.4.1` o `http://limpiezapaneles.local`.

## Lo que la app no puede hacer con el programa original
- **K2 y los botones ◀ ⏸ ▶ por separado:** tu programa mueve la escobilla solo con el interruptor Escobilla, y enciende la fuente solo cuando hace falta.
- **Cambiar los tiempos desde la app:** en tu programa son constantes (`const`). La app los lee, los usa en los cálculos y genera las líneas listas para pegar en tu código («Copiar para el código»). También entiende tus líneas si las pegas en «Pegar desde mi código».
- **Posición exacta del carro:** tu programa no la mide; la app la estima con la fase del ciclo, el tiempo de pasada y los finales de carrera.

Tu propia página (`http://192.168.4.1`) sigue funcionando igual que antes.
