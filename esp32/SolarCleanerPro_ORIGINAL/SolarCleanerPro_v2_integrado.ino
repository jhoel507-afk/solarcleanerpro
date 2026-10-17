// SolarCleanerPro v2 - ESP32-S3
// Limpiador de paneles solares: escobilla + bomba, control web (AP), RTC DS3231 y pantalla OLED.
//
// VERSION INTEGRADA: programa original + caja de mando + /estado para la app.
// SolarCleanerPro(TM) · (c) 2026 Leon Mena, Angel Joel · Gallegos Valencia, Cristian Augusto.
// Todos los derechos reservados.
// Los cambios respecto al programa original estan marcados con "AGREGADO":
//   1) /estado  -> estado del sistema en JSON para la app.
//   2) Pulsador fisico de inicio (GPIO10, contacto NO a GND).
//   3) Lectura del hongo de emergencia (GPIO11, contacto NC a GND).
//   El corte de energia lo hace el hongo por hardware. Si todavia no conectas
//   el hongo, pon un puente entre GPIO11 y GND.

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// ============================================================
//  TIPOS
// ============================================================

// STANDBY -> ENCENDIENDO_FUENTE -> PRELAVADO -> (DERECHA -> IZQUIERDA) x vueltas -> STANDBY
enum EstadoSistema { STANDBY, ENCENDIENDO_FUENTE, PRELAVADO, DERECHA, IZQUIERDA };

// AGREGADO: prototipos de la caja de mando (definidas al final del archivo)
void handleEstado();
void configurarCajaDeMando();
void vigilarCajaDeMando();

// ============================================================
//  CONFIGURACION
// ============================================================

// --- Pantalla OLED ---
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

// --- Pines ---
const int limitDerecha   = 6;   // fin de carrera derecho
const int limitIzquierda = 7;   // fin de carrera izquierdo (punto de partida)
const int relayBomba     = 18;  // rele de la bomba
const int relayFuente    = 5;   // rele de 220V de la fuente de 12V
const int RPWM           = 4;   // BTS7960 #1
const int LPWM           = 21;  // BTS7960 #1
const int RPWM2          = 15;  // BTS7960 #2
const int LPWM2          = 16;  // BTS7960 #2
// R_EN y L_EN de ambos BTS7960 van fijos a 5V.

// false = fin de carrera normalmente abierto (LOW al presionar); true = normalmente cerrado (HIGH al presionar)
const bool FINALES_NC = false;

// --- Motor ---
const int PWM_FRECUENCIA = 1000;  // Hz
const int PWM_RESOLUCION = 8;     // bits
const int VELOCIDAD_MOTOR = 180;  // 0-255

// --- Tiempos (ms) ---
const unsigned long TIEMPO_ENCENDIDO_FUENTE = 1000;
const unsigned long TIEMPO_PRELAVADO        = 5000;
const unsigned long PAUSA_CAMBIO_GIRO       = 500;
const unsigned long TIMEOUT_SEGURIDAD       = 50000;
const unsigned long TIMEOUT_MANUAL          = 120000UL;

// --- Mantenimiento ---
const unsigned long INTERVALO_LOG_HEAP     = 3600000UL;
const int           HORA_REINICIO_PREVENTIVO = 3;
const unsigned long UPTIME_MINIMO_REINICIO = 2UL * 3600UL * 1000UL;

// --- Red ---
const char* NOMBRE_LOCAL = "limpiezapaneles";  // http://limpiezapaneles.local

// --- Acceso web (la parada de emergencia nunca pide clave) ---
const bool  USAR_CLAVE_WEB = false;
const char* WEB_USUARIO    = "admin";
const char* WEB_CLAVE      = "SolarPro2026";

// ============================================================
//  OBJETOS Y VARIABLES GLOBALES
// ============================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
IPAddress apIP(192, 168, 4, 1);
WebServer server(80);
DNSServer dnsServer;
RTC_DS3231 rtc;
Preferences prefs;

bool rtcDisponible = false;

// --- Estado ---
EstadoSistema estadoSistema = STANDBY;
int cicloActual = 0;
bool fuenteEncendida = false;
bool estadoBombaWeb = false;
bool estadoMotorWeb = false;      // manual: hacia la derecha
bool estadoMotorIzqWeb = false;   // manual: hacia la izquierda
String ultimaLimpiezaStr = "Ultima limpieza: sin registro";

// --- Programacion (se guarda en memoria permanente) ---
bool timerActivo = false;
int intervaloDias = 2;
int vueltasConfiguradas = 3;      // 2 o 3
int progAnio = 2026;
int progMes = 1;
int progDia = 1;
int progHora = 8;                 // formato 12h
int progMinuto = 0;
unsigned long ultimaEjecucionEpoch = 0;

// --- Temporizadores ---
unsigned long tiempoInicioAccion = 0;
unsigned long tiempoInicioSecuencia = 0;
unsigned long inicioManual = 0;
unsigned long ultimaActualizacionLCD = 0;
unsigned long ultimoLogHeap = 0;
uint32_t heapMinimoRegistrado = UINT32_MAX;
int diaReinicioHecho = -1;

// ============================================================
//  HORA Y FECHAS
// ============================================================

// Lee el RTC como maximo 1 vez por segundo
DateTime horaActual() {
  static DateTime cache;
  static unsigned long ultimaLectura = 0;
  static bool inicializado = false;
  if (!inicializado || millis() - ultimaLectura >= 1000) {
    cache = rtc.now();
    ultimaLectura = millis();
    inicializado = true;
  }
  return cache;
}

void formatoHora12(int hora24, int minuto, char* out, size_t outSize) {
  int h12 = hora24 % 12;
  if (h12 == 0) h12 = 12;
  const char* ampm = (hora24 >= 12) ? "PM" : "AM";
  snprintf(out, outSize, "%02d:%02d %s", h12, minuto, ampm);
}

void calcularProximaLimpieza(char* out, size_t outSize) {
  if (!timerActivo) {
    snprintf(out, outSize, "Prox: sin programar");
    return;
  }
  if (!rtcDisponible) {
    snprintf(out, outSize, "Prox: falla RTC");
    return;
  }

  DateTime now = horaActual();
  DateTime base(progAnio, progMes, progDia, progHora, progMinuto, 0);
  uint32_t segundosPorIntervalo = (uint32_t)intervaloDias * 86400UL;
  if (segundosPorIntervalo == 0) {
    snprintf(out, outSize, "Prox: sin programar");
    return;
  }

  uint32_t tBase = base.unixtime();
  uint32_t tNow = now.unixtime();
  uint32_t tProxima;

  if (tNow < tBase) {
    tProxima = tBase;
  } else {
    uint32_t transcurridos = tNow - tBase;
    uint32_t ciclosPasados = transcurridos / segundosPorIntervalo;
    tProxima = tBase + ciclosPasados * segundosPorIntervalo;
    if (tProxima <= tNow) tProxima += segundosPorIntervalo;
  }

  DateTime prox(tProxima);
  snprintf(out, outSize, "Prox: %02d/%02d %02d:%02d", prox.day(), prox.month(), prox.hour(), prox.minute());
}

// ============================================================
//  FINALES DE CARRERA
// ============================================================

// Antirrebote: si el contacto se ve activo, se confirma 8 ms despues
bool leerLimite(int pin) {
  const int nivelActivo = FINALES_NC ? HIGH : LOW;
  if (digitalRead(pin) != nivelActivo) return false;
  delay(8);
  return digitalRead(pin) == nivelActivo;
}

bool limiteDerechaActivo()   { return leerLimite(limitDerecha); }
bool limiteIzquierdaActivo() { return leerLimite(limitIzquierda); }

// ============================================================
//  MOTOR (BTS7960)
// ============================================================

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  void pwmIniciar(int pin, int canal) { ledcAttach(pin, PWM_FRECUENCIA, PWM_RESOLUCION); }
  void pwmEscribir(int pin, int canal, int valor) { ledcWrite(pin, valor); }
#else
  void pwmIniciar(int pin, int canal) { ledcSetup(canal, PWM_FRECUENCIA, PWM_RESOLUCION); ledcAttachPin(pin, canal); }
  void pwmEscribir(int pin, int canal, int valor) { ledcWrite(canal, valor); }
#endif

void motorPwmIniciar() {
  pwmIniciar(RPWM, 0);
  pwmIniciar(LPWM, 1);
  pwmIniciar(RPWM2, 2);
  pwmIniciar(LPWM2, 3);
}

void motorPwm(int r1, int l1, int r2, int l2) {
  pwmEscribir(RPWM, 0, r1);
  pwmEscribir(LPWM, 1, l1);
  pwmEscribir(RPWM2, 2, r2);
  pwmEscribir(LPWM2, 3, l2);
}

void motorDetener()   { motorPwm(0, 0, 0, 0); }
void motorDerecha()   { motorPwm(VELOCIDAD_MOTOR, 0, VELOCIDAD_MOTOR, 0); }
void motorIzquierda() { motorPwm(0, VELOCIDAD_MOTOR, 0, VELOCIDAD_MOTOR); }

// ============================================================
//  BOMBA Y FUENTE (reles activos en LOW)
// ============================================================

void bombaEncender() {
  digitalWrite(relayBomba, LOW);
  estadoBombaWeb = true;
}

void bombaApagar() {
  digitalWrite(relayBomba, HIGH);
  estadoBombaWeb = false;
}

void fuenteEncender() {
  fuenteEncendida = true;
  digitalWrite(relayFuente, LOW);
}

void fuenteApagar() {
  fuenteEncendida = false;
  digitalWrite(relayFuente, HIGH);
}

// ============================================================
//  NOMBRES DE ESTADO
// ============================================================

const char* nombreEstado(EstadoSistema e) {
  switch (e) {
    case STANDBY:            return "STANDBY";
    case ENCENDIENDO_FUENTE: return "FUENTE";
    case PRELAVADO:          return "PRELAVADO";
    case DERECHA:            return "DERECHA";
    case IZQUIERDA:          return "IZQUIERDA";
  }
  return "DESCONOCIDO";
}

const char* nombreEstadoWeb(EstadoSistema e) {
  if (e == STANDBY) return "STANDBY";
  return "Limpiando...";
}

// ============================================================
//  PARADA DE EMERGENCIA
// ============================================================

void detenerTodo() {
  motorDetener();
  bombaApagar();
  fuenteApagar();
  estadoMotorWeb = false;
  estadoMotorIzqWeb = false;
  estadoSistema = STANDBY;
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("[SEGURIDAD] Sistema detenido por completo.");
}

// ============================================================
//  MEMORIA PERMANENTE
// ============================================================

void guardarIntSiCambio(const char* clave, int valor) {
  if (prefs.getInt(clave, -9999) != valor) prefs.putInt(clave, valor);
}

void guardarProgramacion() {
  if (!prefs.begin("solarcfg", false)) {
    Serial.println("[MEMORIA] No se pudo abrir la memoria para guardar.");
    return;
  }
  guardarIntSiCambio("activo",  timerActivo ? 1 : 0);
  guardarIntSiCambio("dias",    intervaloDias);
  guardarIntSiCambio("hora",    progHora);
  guardarIntSiCambio("minuto",  progMinuto);
  guardarIntSiCambio("vueltas", vueltasConfiguradas);
  guardarIntSiCambio("anio",    progAnio);
  guardarIntSiCambio("mes",     progMes);
  guardarIntSiCambio("dia",     progDia);
  prefs.end();
}

void cargarProgramacion() {
  if (!prefs.begin("solarcfg", true)) {
    Serial.println("[MEMORIA] Sin programacion guardada, usando valores por defecto.");
    return;
  }
  timerActivo         = (prefs.getInt("activo", 0) == 1);
  intervaloDias       = prefs.getInt("dias",    intervaloDias);
  progHora            = prefs.getInt("hora",    progHora);
  progMinuto          = prefs.getInt("minuto",  progMinuto);
  vueltasConfiguradas = prefs.getInt("vueltas", vueltasConfiguradas);
  progAnio            = prefs.getInt("anio",    progAnio);
  progMes             = prefs.getInt("mes",     progMes);
  progDia             = prefs.getInt("dia",     progDia);
  prefs.end();

  if (intervaloDias < 1) intervaloDias = 1;
  if (progHora < 0 || progHora > 23) progHora = 8;
  if (progMinuto < 0 || progMinuto > 59) progMinuto = 0;
  if (vueltasConfiguradas != 2 && vueltasConfiguradas != 3) vueltasConfiguradas = 3;
  if (progMes < 1 || progMes > 12) progMes = 1;
  if (progDia < 1 || progDia > 31) progDia = 1;
  if (progAnio < 2024 || progAnio > 2099) progAnio = 2026;

  Serial.println("[MEMORIA] Programacion cargada desde la memoria permanente.");
}

// ============================================================
//  MODO MANUAL
// ============================================================

bool modoManualActivo() {
  return estadoBombaWeb || estadoMotorWeb || estadoMotorIzqWeb;
}

// La fuente se enciende sola si hay bomba o motor activos y se apaga cuando no queda ninguno
void actualizarFuenteManual() {
  if (modoManualActivo()) fuenteEncender();
  else fuenteApagar();
}

void asegurarFuenteManual() {
  if (!fuenteEncendida) {
    fuenteEncender();
    delay(TIEMPO_ENCENDIDO_FUENTE);
  }
}

void vigilarModoManual() {
  if (estadoSistema != STANDBY) return;

  // Al tocar el limite derecho: pausa y regreso automatico al punto de partida
  if (estadoMotorWeb && limiteDerechaActivo()) {
    motorDetener();
    estadoMotorWeb = false;
    Serial.println("[MANUAL] Limite derecho alcanzado: pausa y regreso al punto de partida.");
    delay(PAUSA_CAMBIO_GIRO);
    estadoMotorIzqWeb = true;
    motorIzquierda();
    inicioManual = millis();
  }

  if (estadoMotorIzqWeb && limiteIzquierdaActivo()) {
    motorDetener();
    estadoMotorIzqWeb = false;
    actualizarFuenteManual();
    Serial.println("[MANUAL] Escobilla detenida en el limite izquierdo.");
  }

  if (modoManualActivo() && (millis() - inicioManual > TIMEOUT_MANUAL)) {
    Serial.println("[MANUAL] Tiempo limite superado. Apagando bomba, escobilla y fuente.");
    detenerTodo();
  }
}

// ============================================================
//  CICLO AUTOMATICO
// ============================================================

void iniciarCicloCompleto() {
  if (estadoSistema != STANDBY) return;
  if (modoManualActivo()) return;

  cicloActual = 0;
  fuenteEncender();
  tiempoInicioAccion = millis();
  tiempoInicioSecuencia = millis();
  estadoSistema = ENCENDIENDO_FUENTE;
  Serial.println("[CICLO] Encendiendo fuente de 12V...");
}

void actualizarMaquinaEstados() {
  if (estadoSistema == STANDBY) return;

  if (millis() - tiempoInicioAccion > TIMEOUT_SEGURIDAD) {
    Serial.println("[ERROR] Timeout de seguridad superado. Deteniendo motores.");
    detenerTodo();
    ultimaLimpiezaStr = "ERROR: tiempo excedido, revisar finales de carrera";
    return;
  }

  // Ambos finales activos a la vez indica cable en corto o mal cableado
  if ((estadoSistema == DERECHA || estadoSistema == IZQUIERDA) &&
      limiteDerechaActivo() && limiteIzquierdaActivo()) {
    Serial.println("[ERROR] Ambos finales de carrera activos. Revisar cableado.");
    detenerTodo();
    ultimaLimpiezaStr = "ERROR: finales de carrera en conflicto";
    return;
  }

  if (estadoSistema == ENCENDIENDO_FUENTE) {
    if (millis() - tiempoInicioSecuencia >= TIEMPO_ENCENDIDO_FUENTE) {
      bombaEncender();
      tiempoInicioSecuencia = millis();
      estadoSistema = PRELAVADO;
      Serial.println("[CICLO] Prelavado: bomba sola por 5s...");
    }
  }
  else if (estadoSistema == PRELAVADO) {
    if (millis() - tiempoInicioSecuencia >= TIEMPO_PRELAVADO) {
      digitalWrite(LED_BUILTIN, HIGH);
      tiempoInicioAccion = millis();
      estadoSistema = DERECHA;
      Serial.printf("[CICLO] Vuelta 1/%d: moviendo a la derecha...\n", vueltasConfiguradas);
    }
  }
  else if (estadoSistema == DERECHA) {
    if (limiteDerechaActivo()) {
      motorDetener();
      delay(PAUSA_CAMBIO_GIRO);
      tiempoInicioAccion = millis();
      estadoSistema = IZQUIERDA;
      Serial.println("[CICLO] Regresando a la izquierda...");
    } else {
      motorDerecha();
    }
  }
  else if (estadoSistema == IZQUIERDA) {
    if (limiteIzquierdaActivo()) {
      motorDetener();
      cicloActual++;

      if (cicloActual < vueltasConfiguradas) {
        delay(PAUSA_CAMBIO_GIRO);
        tiempoInicioAccion = millis();
        estadoSistema = DERECHA;
        Serial.printf("[CICLO] Vuelta %d/%d completa. Iniciando siguiente...\n", cicloActual, vueltasConfiguradas);
      } else {
        bombaApagar();
        fuenteApagar();
        estadoSistema = STANDBY;
        digitalWrite(LED_BUILTIN, LOW);

        char buf[40];
        if (rtcDisponible) {
          DateTime now = horaActual();
          sprintf(buf, "Ultima limpieza: %02d/%02d %02d:%02d", now.day(), now.month(), now.hour(), now.minute());
        } else {
          sprintf(buf, "Ultima limpieza: hace %lus", (unsigned long)(millis() / 1000));
        }
        ultimaLimpiezaStr = String(buf);

        Serial.printf("[CICLO] Ciclo completo terminado (%d vueltas). Apagado automatico.\n", vueltasConfiguradas);
      }
    } else {
      motorIzquierda();
    }
  }
}

void verificarTimerRecurrente() {
  if (!rtcDisponible || !timerActivo || estadoSistema != STANDBY) return;
  if (modoManualActivo()) return;

  DateTime now = horaActual();
  DateTime tProgramada(progAnio, progMes, progDia, progHora, progMinuto, 0);

  uint32_t tiempoActualSec = now.unixtime();
  uint32_t tiempoProgSec = tProgramada.unixtime();
  if (tiempoActualSec < tiempoProgSec) return;

  uint32_t segundosTranscurridos = tiempoActualSec - tiempoProgSec;
  uint32_t segundosPorIntervalo = (uint32_t)intervaloDias * 86400UL;
  if (segundosPorIntervalo == 0) return;

  if (now.hour() != progHora || now.minute() != progMinuto) return;
  if (segundosTranscurridos % segundosPorIntervalo >= 60) return;
  if (ultimaEjecucionEpoch == (tiempoActualSec / 60)) return;

  Serial.println("[TIMER] Ciclo recurrente disparado por RTC!");
  ultimaEjecucionEpoch = (tiempoActualSec / 60);
  iniciarCicloCompleto();
}

// ============================================================
//  MANTENIMIENTO
// ============================================================

void registrarDiagnosticoHeap() {
  if (millis() - ultimoLogHeap < INTERVALO_LOG_HEAP) return;
  ultimoLogHeap = millis();

  uint32_t heapLibre = ESP.getFreeHeap();
  if (heapLibre < heapMinimoRegistrado) heapMinimoRegistrado = heapLibre;

  Serial.printf("[DIAGNOSTICO] Heap libre: %u bytes | Minimo historico: %u bytes | Uptime: %lu h\n",
                heapLibre, heapMinimoRegistrado, millis() / 3600000UL);

  if (heapLibre < 20000) {
    Serial.println("[ALERTA] Heap libre bajo. Posible fuga de memoria, revisar.");
  }
}

void verificarReinicioPreventivo() {
  if (!rtcDisponible || estadoSistema != STANDBY) return;
  if (modoManualActivo()) return;
  if (millis() < UPTIME_MINIMO_REINICIO) return;

  DateTime now = horaActual();
  if (now.hour() == HORA_REINICIO_PREVENTIVO && now.day() != diaReinicioHecho) {
    diaReinicioHecho = now.day();
    Serial.println("[MANTENIMIENTO] Reinicio preventivo programado ejecutandose...");
    delay(200);
    ESP.restart();
  }
}

// ============================================================
//  PANTALLA OLED
// ============================================================

void actualizarPantallaOLED() {
  if (millis() - ultimaActualizacionLCD <= 500) return;
  ultimaActualizacionLCD = millis();

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(nombreEstado(estadoSistema));

  display.setTextSize(1);

  display.setCursor(0, 22);
  if (rtcDisponible) {
    DateTime now = horaActual();
    char horaBuf[20];
    sprintf(horaBuf, "H: %02d:%02d:%02d", now.hour(), now.minute(), now.second());
    display.print(horaBuf);
  } else {
    display.print(F("H: --:--:--"));
  }

  display.setCursor(0, 34);
  display.print(F("AP: SolarCleaner_PRO"));

  display.setCursor(0, 46);
  if (estadoSistema == PRELAVADO || estadoSistema == DERECHA || estadoSistema == IZQUIERDA) {
    int vueltaMostrada = cicloActual + 1;
    if (vueltaMostrada > vueltasConfiguradas) vueltaMostrada = vueltasConfiguradas;
    char vueltaBuf[20];
    sprintf(vueltaBuf, "Vuelta: %d/%d", vueltaMostrada, vueltasConfiguradas);
    display.print(vueltaBuf);
  } else {
    char vueltasBuf[20];
    sprintf(vueltasBuf, "Vueltas: %d", vueltasConfiguradas);
    display.print(vueltasBuf);
  }

  display.setCursor(0, 56);
  char proxBuf[24];
  calcularProximaLimpieza(proxBuf, sizeof(proxBuf));
  display.print(proxBuf);

  display.display();
}

// ============================================================
//  PAGINA WEB
// ============================================================

const char PAGINA_HEAD[] PROGMEM = R"HTML(<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1, viewport-fit=cover'>
<title>Limpiador Solar Pro</title>
<style>
:root{
  --bg1:#0f172a;--bg2:#1e293b;--panel:#1a2436;--panel-border:rgba(255,255,255,.08);
  --text:#e6edf5;--muted:#93a3b8;--input-bg:#0f172a;
  --accent:#14b8a6;--accent-dark:#0d9488;--ok:#22c55e;--danger:#ef4444;
}
*{box-sizing:border-box}
html,body{height:100%}
body{margin:0;background:linear-gradient(160deg,var(--bg1),var(--bg2));color:var(--text);font-family:-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;display:flex;justify-content:center;padding:calc(16px + env(safe-area-inset-top,0px)) 16px calc(16px + env(safe-area-inset-bottom,0px));min-height:100%}
.wrap{width:100%;max-width:420px;display:flex;flex-direction:column;gap:14px}
.header{text-align:center;margin-bottom:2px}
.header h1{margin:0;font-size:1.5rem;font-weight:700;letter-spacing:.02em}
.header p{margin:4px 0 0;font-size:.78rem;color:var(--muted);font-weight:600;text-transform:uppercase;letter-spacing:.08em}
.hero{background:var(--panel);border:1px solid var(--panel-border);border-radius:18px;padding:18px 20px;display:flex;align-items:center;gap:14px;box-shadow:0 10px 30px rgba(0,0,0,.3)}
.led{width:12px;height:12px;border-radius:50%;background:var(--ok);box-shadow:0 0 12px var(--ok);flex-shrink:0}
.led.busy{background:var(--accent);box-shadow:0 0 12px var(--accent);animation:pulse 1.4s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}
.hero-text{flex:1;min-width:0}
.hero-status{font-size:1.2rem;font-weight:700}
.hero-sub{font-size:.8rem;color:var(--muted);margin-top:2px}
.btn-primary{width:100%;border:none;border-radius:14px;padding:15px 16px;background:linear-gradient(135deg,var(--accent),var(--accent-dark));color:#fff;font-weight:700;font-size:1.02rem;display:flex;align-items:center;justify-content:center;gap:8px;text-decoration:none;box-shadow:0 8px 20px rgba(20,184,166,.28)}
.card{background:var(--panel);border:1px solid var(--panel-border);border-radius:16px;padding:16px 18px;box-shadow:0 6px 16px rgba(0,0,0,.22)}
.card-title{font-size:.95rem;font-weight:700}
.card-note{font-size:.78rem;color:var(--muted);margin-top:8px;line-height:1.5}
.sched-head{display:flex;justify-content:space-between;align-items:center}
.sched-fields{display:flex;align-items:center;gap:6px;margin-top:12px;font-size:.85rem;color:var(--muted);flex-wrap:wrap}
.sched-fields select,.sched-fields input{background:var(--input-bg);color:var(--text);border:1px solid var(--panel-border);border-radius:8px;padding:7px 8px;font-size:.88rem;font-family:inherit}
.switch{position:relative;width:46px;height:27px;flex-shrink:0;display:inline-block;text-decoration:none}
.switch .slider{position:absolute;inset:0;background:#334155;border:1px solid var(--panel-border);border-radius:27px;transition:.2s}
.switch .slider:before{content:'';position:absolute;width:19px;height:19px;left:3px;top:3px;background:#e6edf5;border-radius:50%;transition:.2s;box-shadow:0 1px 3px rgba(0,0,0,.35)}
.switch.on .slider{background:var(--accent-dark);border-color:var(--accent)}
.switch.on .slider:before{transform:translateX(19px);background:#fff}
.switch.off-locked{opacity:.4;pointer-events:none}
.row{display:flex;justify-content:space-between;align-items:center;padding:9px 0}
.row+.row{border-top:1px solid var(--panel-border)}
.row-label{font-size:.9rem;font-weight:600}
.btn-stop{width:100%;border-radius:14px;padding:13px;font-weight:700;font-size:.95rem;background:rgba(239,68,68,.12);color:var(--danger);border:1px solid rgba(239,68,68,.35);text-decoration:none;display:block;text-align:center}
.btn-submit{width:100%;border:none;border-radius:10px;padding:11px;background:linear-gradient(135deg,var(--accent),var(--accent-dark));color:#fff;font-weight:700;font-size:.88rem;margin-top:10px;cursor:pointer}
footer{font-size:.66rem;color:var(--muted);text-align:center;margin-top:4px}
</style></head><body><div class='wrap'>
<div class='header'><h1>Limpiador Solar Pro</h1><p>Sistema de limpieza automatizado</p></div>
)HTML";

const char PAGINA_SCHED_FORM_INICIO[] PROGMEM = R"HTML(<form action='/guardarTimer' method='GET'><div class='sched-fields'><span>Cada</span><select name='dias'>
)HTML";

const char PAGINA_SCHED_FORM_MID[] PROGMEM = R"HTML(</select><span>dias, a las</span>
)HTML";

const char PAGINA_MANUAL_INICIO[] PROGMEM = R"HTML(<div class='card' id='manual'><div class='card-title' style='margin-bottom:2px'>Control manual</div>
)HTML";

const char PAGINA_PIE[] PROGMEM = R"HTML(<a href='/stop' class='btn-stop'>Parada de emergencia</a>
<footer>Hecho en Peru &copy; 2026</footer></div>
<script>
var tm;
function programar(){var h=document.getElementById('hero');clearTimeout(tm);if(h&&h.getAttribute('data-act')==='1')tm=setTimeout(function(){actualizar('/');},2000);}
function actualizar(u){fetch(u).then(function(r){return r.text();}).then(function(t){var d=new DOMParser().parseFromString(t,'text/html');['hero','accion','manual'].forEach(function(i){var n=d.getElementById(i),o=document.getElementById(i);if(n&&o)o.outerHTML=n.outerHTML;});programar();}).catch(function(){programar();});}
document.addEventListener('click',function(e){var a=e.target.closest?e.target.closest('a'):null;if(!a)return;var h=a.getAttribute('href');if(h==='/auto'||h==='/bomba'||h==='/motor'||h==='/stop'){e.preventDefault();actualizar(h);}});
programar();
try{var d=new Date();fetch('/ajustarHora?a='+d.getFullYear()+'&m='+(d.getMonth()+1)+'&d='+d.getDate()+'&h='+d.getHours()+'&n='+d.getMinutes()+'&s='+d.getSeconds());}catch(e){}</script>
</body></html>
)HTML";

void enviarProgmem(const char* bloque) {
  server.sendContent_P(bloque);
}

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  enviarProgmem(PAGINA_HEAD);

  {
    bool ocupado = (estadoSistema != STANDBY);
    char bloque[420];
    snprintf(bloque, sizeof(bloque),
      "<div class='hero' id='hero' data-act='%d'><div class='led %s'></div><div class='hero-text'>"
      "<div class='hero-status'>%s</div><div class='hero-sub'>%s</div></div></div>",
      (ocupado || modoManualActivo()) ? 1 : 0, ocupado ? "busy" : "",
      nombreEstadoWeb(estadoSistema), ultimaLimpiezaStr.c_str());
    server.sendContent(bloque);
  }

  {
    bool enProceso = (estadoSistema != STANDBY);
    bool bloqueadoPorManual = (!enProceso && modoManualActivo());
    char bloque[260];
    snprintf(bloque, sizeof(bloque),
      "<a href='/auto' id='accion' class='btn-primary'%s><span>%s</span></a>",
      bloqueadoPorManual ? " style='opacity:.4;pointer-events:none'" : "",
      enProceso ? "Detener limpieza completa"
                : (bloqueadoPorManual ? "Apaga el modo manual primero" : "Iniciar limpieza completa"));
    server.sendContent(bloque);
  }

  {
    char bloque[250];
    snprintf(bloque, sizeof(bloque),
      "<div class='card'><div class='sched-head'><span class='card-title'>Programacion automatica</span>"
      "%s</div>",
      timerActivo
        ? "<a href='/cancelarTimer' class='switch on'><span class='slider'></span></a>"
        : "<span class='switch'><span class='slider'></span></span>");
    server.sendContent(bloque);
  }

  enviarProgmem(PAGINA_SCHED_FORM_INICIO);

  const int opcionesDias[] = {1, 2, 3, 7, 15};
  for (int i = 0; i < 5; i++) {
    char opt[60];
    snprintf(opt, sizeof(opt), "<option value='%d'%s>%d</option>",
             opcionesDias[i], (intervaloDias == opcionesDias[i] ? " selected" : ""), opcionesDias[i]);
    server.sendContent(opt);
  }

  enviarProgmem(PAGINA_SCHED_FORM_MID);

  {
    char horaValue[6];
    snprintf(horaValue, sizeof(horaValue), "%02d:%02d", progHora, progMinuto);
    char inputBuf[140];
    snprintf(inputBuf, sizeof(inputBuf),
      "<input type='time' name='hora' lang='en-US' value='%s'>", horaValue);
    server.sendContent(inputBuf);
  }

  {
    char bloque[220];
    snprintf(bloque, sizeof(bloque),
      "<span>con</span><select name='vueltas'><option value='2'%s>2</option><option value='3'%s>3</option></select><span>vueltas</span></div>",
      (vueltasConfiguradas == 2 ? " selected" : ""), (vueltasConfiguradas == 3 ? " selected" : ""));
    server.sendContent(bloque);
  }

  {
    char bloque[220];
    snprintf(bloque, sizeof(bloque),
      "<button type='submit' class='btn-submit'>%s</button></form>",
      timerActivo ? "Actualizar programacion" : "Guardar programacion");
    server.sendContent(bloque);
  }

  {
    char horaFormateada[16];
    formatoHora12(progHora, progMinuto, horaFormateada, sizeof(horaFormateada));
    char bloque[220];
    if (timerActivo) {
      snprintf(bloque, sizeof(bloque),
        "<div class='card-note'>Activada: cada %d dia(s) a las %s, con %d vueltas.</div></div>",
        intervaloDias, horaFormateada, vueltasConfiguradas);
    } else {
      snprintf(bloque, sizeof(bloque),
        "<div class='card-note'>Desactivada: el sistema solo limpia con el boton manual (%d vueltas).</div></div>",
        vueltasConfiguradas);
    }
    server.sendContent(bloque);
  }

  enviarProgmem(PAGINA_MANUAL_INICIO);
  {
    bool bloqueado = (estadoSistema != STANDBY);
    bool escobillaMoviendo = estadoMotorWeb || estadoMotorIzqWeb ||
                             estadoSistema == DERECHA || estadoSistema == IZQUIERDA;
    char bloque[800];
    snprintf(bloque, sizeof(bloque),
      "<div class='row'><span class='row-label'>Bomba de agua</span>"
      "<a href='/bomba' class='switch %s %s'><span class='slider'></span></a></div>"
      "<div class='row'><span class='row-label'>Escobilla a la derecha</span>"
      "<a href='/motor' class='switch %s %s'><span class='slider'></span></a></div></div>",
      estadoBombaWeb ? "on" : "", bloqueado ? "off-locked" : "",
      escobillaMoviendo ? "on" : "", bloqueado ? "off-locked" : "");
    server.sendContent(bloque);
  }

  enviarProgmem(PAGINA_PIE);
  server.sendContent("");
}

// ============================================================
//  RUTAS WEB
// ============================================================

bool accesoPermitido() {
  if (!USAR_CLAVE_WEB) return true;
  if (server.authenticate(WEB_USUARIO, WEB_CLAVE)) return true;
  server.requestAuthentication();
  return false;
}

void handleAuto() {
  if (!accesoPermitido()) return;
  if (estadoSistema == STANDBY) {
    if (!modoManualActivo()) iniciarCicloCompleto();
  } else {
    detenerTodo();
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleStop() {
  detenerTodo();
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleBomba() {
  if (!accesoPermitido()) return;
  if (estadoSistema == STANDBY) {
    if (estadoBombaWeb) {
      bombaApagar();
      actualizarFuenteManual();
    } else {
      asegurarFuenteManual();
      bombaEncender();
      inicioManual = millis();
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleMotor() {
  if (!accesoPermitido()) return;
  if (estadoSistema == STANDBY) {
    if (estadoMotorIzqWeb) {
      estadoMotorIzqWeb = false;
      motorDetener();
      actualizarFuenteManual();
    } else if (estadoMotorWeb) {
      // Al apagarlo, la escobilla regresa sola al punto de partida
      estadoMotorWeb = false;
      motorDetener();
      if (!limiteIzquierdaActivo()) {
        delay(PAUSA_CAMBIO_GIRO);
        estadoMotorIzqWeb = true;
        motorIzquierda();
        inicioManual = millis();
        Serial.println("[MANUAL] Escobilla apagada: regresando al punto de partida.");
      } else {
        actualizarFuenteManual();
      }
    } else if (limiteDerechaActivo()) {
      Serial.println("[MANUAL] Escobilla ya en el limite derecho, no se mueve.");
    } else {
      asegurarFuenteManual();
      estadoMotorWeb = true;
      motorDerecha();
      inicioManual = millis();
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleGuardarTimer() {
  if (!accesoPermitido()) return;
  if (server.hasArg("hora") && server.hasArg("dias")) {
    String horaStr = server.arg("hora");  // "HH:MM" en 24h
    int hNueva = horaStr.substring(0, 2).toInt();
    int mNueva = horaStr.substring(3, 5).toInt();
    bool formatoOk = (horaStr.length() >= 5 && horaStr.charAt(2) == ':' &&
                      hNueva >= 0 && hNueva <= 23 && mNueva >= 0 && mNueva <= 59);
    if (!formatoOk) {
      Serial.println("[TIMER] Hora invalida recibida, se ignora.");
      server.sendHeader("Location", "/", true);
      server.send(303);
      return;
    }
    progHora   = hNueva;
    progMinuto = mNueva;

    int diasRecibidos = server.arg("dias").toInt();
    intervaloDias = (diasRecibidos > 0 && diasRecibidos <= 365) ? diasRecibidos : 1;

    if (server.hasArg("vueltas")) {
      int v = server.arg("vueltas").toInt();
      vueltasConfiguradas = (v == 2 || v == 3) ? v : 3;
    }

    if (rtcDisponible) {
      DateTime now = horaActual();
      progAnio = now.year();
      progMes  = now.month();
      progDia  = now.day();
    } else {
      progAnio = 2026; progMes = 1; progDia = 1;
    }

    timerActivo = true;
    ultimaEjecucionEpoch = 0;
    guardarProgramacion();
    Serial.println("[TIMER] Programacion recurrente guardada con exito.");
  }
  server.sendHeader("Location", "/", true);
  server.send(303);
}

void handleCancelarTimer() {
  if (!accesoPermitido()) return;
  timerActivo = false;
  guardarProgramacion();
  Serial.println("[TIMER] Programacion recurrente desactivada.");
  server.sendHeader("Location", "/", true);
  server.send(303);
}

// La pagina envia la hora del celular; solo corrige el RTC en STANDBY y si difiere mas de 2 minutos
void handleAjustarHora() {
  if (rtcDisponible && estadoSistema == STANDBY &&
      server.hasArg("a") && server.hasArg("m") && server.hasArg("d") &&
      server.hasArg("h") && server.hasArg("n") && server.hasArg("s")) {
    int anio = server.arg("a").toInt();
    int mes  = server.arg("m").toInt();
    int dia  = server.arg("d").toInt();
    int hh   = server.arg("h").toInt();
    int mm   = server.arg("n").toInt();
    int ss   = server.arg("s").toInt();

    if (anio >= 2024 && anio <= 2099 && mes >= 1 && mes <= 12 && dia >= 1 && dia <= 31 &&
        hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59 && ss >= 0 && ss <= 59) {
      DateTime horaCelular(anio, mes, dia, hh, mm, ss);
      long diferencia = (long)horaCelular.unixtime() - (long)rtc.now().unixtime();
      if (diferencia < 0) diferencia = -diferencia;

      if (diferencia > 120) {
        rtc.adjust(horaCelular);
        ultimaEjecucionEpoch = 0;
        Serial.println("[RTC] Hora corregida automaticamente desde el celular.");
      }
    }
  }
  server.send(200, "text/plain", "ok");
}

void handleNoEncontrado() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// ============================================================
//  CONFIGURACION INICIAL
// ============================================================

void configurarPines() {
  pinMode(limitDerecha, INPUT_PULLUP);
  pinMode(limitIzquierda, INPUT_PULLUP);

  // Nivel HIGH antes de configurar como salida, para que el rele no haga "clic" al arrancar
  digitalWrite(relayBomba, HIGH);
  digitalWrite(relayFuente, HIGH);
  pinMode(relayBomba, OUTPUT);
  pinMode(relayFuente, OUTPUT);

  motorPwmIniciar();
  motorDetener();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  configurarCajaDeMando();   // AGREGADO: pulsador de inicio y hongo de emergencia
}

void configurarOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Fallo al iniciar pantalla OLED SSD1306"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.print("SolarCleaner Pro");
  display.display();
  delay(1000);
}

void configurarRTC() {
  if (!rtc.begin()) {
    Serial.println("[ADVERTENCIA] Modulo RTC no conectado o fallo I2C.");
    rtcDisponible = false;
    return;
  }
  rtcDisponible = true;
  Serial.println("[OK] Modulo RTC DS3231 detectado correctamente.");
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void configurarWatchdog() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfgWdt = { .timeout_ms = 10000, .idle_core_mask = 0, .trigger_panic = true };
  if (esp_task_wdt_init(&cfgWdt) == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&cfgWdt);
#else
  esp_task_wdt_init(10, true);
#endif
  esp_task_wdt_add(NULL);
}

void configurarWiFiYServidor() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("SolarCleaner_PRO", "SolarPro2026");

  dnsServer.start(53, "*", apIP);

  if (MDNS.begin(NOMBRE_LOCAL)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[RED] Nombre local activo: http://limpiezapaneles.local");
  } else {
    Serial.println("[RED] No se pudo iniciar mDNS (la IP 192.168.4.1 sigue funcionando).");
  }

  server.on("/", handleRoot);
  server.on("/auto", handleAuto);
  server.on("/stop", handleStop);
  server.on("/bomba", handleBomba);
  server.on("/motor", handleMotor);
  server.on("/guardarTimer", handleGuardarTimer);
  server.on("/cancelarTimer", handleCancelarTimer);
  server.on("/ajustarHora", handleAjustarHora);
  server.on("/estado", handleEstado);   // AGREGADO: estado en JSON para la app
  server.onNotFound(handleNoEncontrado);

  server.begin();
  Serial.println("Servidor Web iniciado correctamente.");
}

void setup() {
  configurarPines();  // primero: reles y motor en estado seguro
  Serial.begin(115200);
  delay(500);

  Wire.begin();
  configurarOLED();
  configurarRTC();
  cargarProgramacion();
  configurarWatchdog();
  configurarWiFiYServidor();
}

void loop() {
  esp_task_wdt_reset();
  dnsServer.processNextRequest();
  server.handleClient();
  vigilarCajaDeMando();   // AGREGADO: hongo de emergencia y pulsador de inicio
  actualizarMaquinaEstados();
  vigilarModoManual();
  actualizarPantallaOLED();
  verificarTimerRecurrente();
  registrarDiagnosticoHeap();
  verificarReinicioPreventivo();
  delay(5);
}

// ============================================================
//  AGREGADO: CAJA DE MANDO (pulsador, hongo) Y /estado PARA LA APP
// ============================================================

const int  pinPulsadorInicio = 10;     // pulsador verde de la caja (NO a GND)
const int  pinHongo          = 11;     // contacto NC del hongo a GND (cerrado = normal)
const bool USAR_HONGO        = true;   // false SOLO para pruebas sobre la mesa; con true y sin hongo
                                       // conectado hay que poner un puente entre GPIO11 y GND

bool emergenciaActiva = false;
bool pulsadorPrevio   = false;
unsigned long ultimoCambioPulsador = 0;

void configurarCajaDeMando() {
  pinMode(pinPulsadorInicio, INPUT_PULLUP);
  pinMode(pinHongo, INPUT_PULLUP);
  pulsadorPrevio = (digitalRead(pinPulsadorInicio) == LOW);   // no arranca si estaba presionado al encender
}

// Llamar en cada vuelta del loop()
void vigilarCajaDeMando() {
  // --- Hongo de emergencia (abierto o cable cortado = emergencia) ---
  bool hongoAbierto = USAR_HONGO && (digitalRead(pinHongo) == HIGH);
  if (hongoAbierto) {
    if (!emergenciaActiva) {
      emergenciaActiva = true;
      detenerTodo();
      ultimaLimpiezaStr = "PARADA DE EMERGENCIA (hongo)";
      Serial.println("[SEGURIDAD] Hongo de emergencia presionado.");
    } else if (estadoSistema != STANDBY || modoManualActivo()) {
      detenerTodo();                       // mantiene todo apagado mientras siga presionado
    }
  } else if (emergenciaActiva) {
    emergenciaActiva = false;              // al liberarlo queda en STANDBY y NO arranca solo
    ultimaLimpiezaStr = "Hongo liberado: sistema en espera";
    Serial.println("[SEGURIDAD] Hongo liberado.");
  }

  // --- Pulsador físico de inicio (por flanco, antirrebote 50 ms) ---
  bool presionado = (digitalRead(pinPulsadorInicio) == LOW);
  if (presionado != pulsadorPrevio && millis() - ultimoCambioPulsador > 50) {
    ultimoCambioPulsador = millis();
    pulsadorPrevio = presionado;
    if (presionado) {
      if (!emergenciaActiva && estadoSistema == STANDBY && !modoManualActivo()) {
        Serial.println("[CAJA] Pulsador de inicio: arrancando ciclo.");
        iniciarCicloCompleto();
      } else {
        Serial.println("[CAJA] Pulsador ignorado (ciclo en marcha, modo manual o emergencia).");
      }
    }
  }
}

// ---------------------------------------------------------------------------
//  /estado  (JSON para la app)
// ---------------------------------------------------------------------------
static const char* faseJson() {
  switch (estadoSistema) {
    case STANDBY:            return "reposo";
    case ENCENDIENDO_FUENTE: return "fuente";
    case PRELAVADO:          return "prelavado";
    case DERECHA:            return "der";
    case IZQUIERDA:          return "izq";
  }
  return "reposo";
}

// Próxima limpieza como hora local (misma lógica que calcularProximaLimpieza)
static uint32_t proximaLimpiezaEpoch() {
  if (!timerActivo || !rtcDisponible) return 0;
  uint32_t seg = (uint32_t)intervaloDias * 86400UL;
  if (seg == 0) return 0;
  DateTime base(progAnio, progMes, progDia, progHora, progMinuto, 0);
  uint32_t tBase = base.unixtime(), tNow = horaActual().unixtime(), t;
  if (tNow < tBase) t = tBase;
  else {
    t = tBase + ((tNow - tBase) / seg) * seg;
    if (t <= tNow) t += seg;
  }
  return t;
}

void handleEstado() {
  const int nivelActivo = FINALES_NC ? HIGH : LOW;
  bool lR = (digitalRead(limitDerecha)   == nivelActivo);   // lectura directa, sin retardo
  bool lL = (digitalRead(limitIzquierda) == nivelActivo);
  int dir = (estadoSistema == DERECHA   || estadoMotorWeb)    ? 1
          : (estadoSistema == IZQUIERDA || estadoMotorIzqWeb) ? -1 : 0;
  bool esError = ultimaLimpiezaStr.startsWith("ERROR");

  String s;
  s.reserve(760);
  s += "{\"fw\":\"orig\",\"pos\":-1";
  s += ",\"dir\":";   s += dir;
  s += ",\"k1\":";    s += (estadoBombaWeb ? 1 : 0);
  s += ",\"k2\":";    s += (fuenteEncendida ? 1 : 0);
  s += ",\"run\":";   s += (estadoSistema != STANDBY ? 1 : 0);
  s += ",\"es\":";    s += (emergenciaActiva ? 1 : 0);
  s += ",\"ph\":\"";  s += faseJson();  s += "\"";
  s += ",\"laps\":";  s += cicloActual;
  s += ",\"tot\":";   s += vueltasConfiguradas;
  s += ",\"lR\":";    s += (lR ? 1 : 0);
  s += ",\"lL\":";    s += (lL ? 1 : 0);
  s += ",\"err\":\""; s += (esError ? ultimaLimpiezaStr : String("")); s += "\"";
  s += ",\"ult\":\""; s += ultimaLimpiezaStr; s += "\"";
  s += ",\"up\":";    s += (unsigned long)(millis() / 1000);
  s += ",\"t\":";     s += (rtcDisponible ? (unsigned long)horaActual().unixtime() : 0UL);
  s += ",\"nx\":";    s += (unsigned long)proximaLimpiezaEpoch();
  s += ",\"prog\":{\"a\":"; s += (timerActivo ? 1 : 0);
  s += ",\"d\":";     s += intervaloDias;
  s += ",\"h\":";     s += progHora;
  s += ",\"m\":";     s += progMinuto;  s += "}";
  s += ",\"cfg\":{\"tFuente\":"; s += String(TIEMPO_ENCENDIDO_FUENTE / 1000.0, 2);
  s += ",\"tPre\":";    s += String(TIEMPO_PRELAVADO / 1000.0, 2);
  s += ",\"tPausa\":";  s += String(PAUSA_CAMBIO_GIRO / 1000.0, 2);
  s += ",\"tFin\":0";
  s += ",\"tMaxMov\":"; s += String(TIMEOUT_SEGURIDAD / 1000.0, 1);
  s += ",\"pwm\":";     s += VELOCIDAD_MOTOR;
  s += ",\"bombaRec\":1";
  s += ",\"laps\":";    s += vueltasConfiguradas;  s += "}";
  s += ",\"heap\":";  s += (unsigned long)ESP.getFreeHeap();
  s += ",\"evn\":0,\"ev\":[]}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}
