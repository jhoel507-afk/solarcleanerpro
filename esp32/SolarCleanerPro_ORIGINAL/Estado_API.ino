// ============================================================================
//  Estado_API.ino  ·  PESTAÑA ADICIONAL para el programa original SolarCleanerPro v2
//  Autores: Leon Mena, Ángel Joel · Gallegos Valencia, Cristian Augusto
// ----------------------------------------------------------------------------
//  No cambia la lógica de tu programa. Solo agrega:
//    1) /estado   -> estado del sistema en JSON para la app (Android / Windows).
//    2) Pulsador físico de inicio de la caja (GPIO10, contacto NO a GND).
//    3) Lectura del hongo de emergencia (GPIO11, contacto NC a GND).
//
//  Cómo usarla: en el Arduino IDE crea una pestaña nueva llamada Estado_API
//  (o copia este archivo a la carpeta de tu programa) y agrega 3 líneas en tu
//  archivo principal (ver LEEME_ORIGINAL.md). Esta pestaña debe quedar DESPUÉS
//  del archivo principal (el IDE las ordena por nombre; el principal va primero).
// ============================================================================

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
