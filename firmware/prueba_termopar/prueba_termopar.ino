/*
 * ============================================================================
 *  ESP32 + MAX6675 + relé de estado sólido
 *  Potencia MANUAL con TECHO DE TEMPERATURA configurable. Todavía sin PID.
 * ----------------------------------------------------------------------------
 *  Librería necesaria (Arduino IDE → Gestor de librerías):
 *    "MAX6675 library" de Adafruit
 *
 *  CONEXIONES NUEVAS (las del relé no se tocan)
 *
 *    MAX6675 (HW-550)        ESP32
 *    ----------------        -----------------------------------
 *    VCC                 →   3V3     ← NUNCA 5V. Su pin SO saca lógica
 *    GND                 →   GND        al nivel de su alimentación, y los
 *    SCK                 →   GPIO18     GPIO del ESP32 no toleran 5 V.
 *    CS                  →   GPIO5
 *    SO                  →   GPIO19
 *    T+                  →   termopar, conductor AMARILLO (ANSI) o VERDE (IEC)
 *    T−                  →   termopar, conductor ROJO (ANSI) o BLANCO (IEC)
 *
 *  CÓMO SE USA
 *    Fase 1 — tanteo. El limitador arranca APAGADO. Ponés una potencia baja,
 *    mirás hasta dónde sube la temperatura y a qué velocidad, y decidís el
 *    techo. El comando `D` te da mínimo, máximo y tasa de cambio.
 *
 *    Fase 2 — con techo. Lo fijás con `T180` y lo activás con `LIM`. A partir
 *    de ahí, si la temperatura llega al techo el relé se apaga, y vuelve a
 *    encender cuando baja la histéresis. Es regulación todo-o-nada, no PID:
 *    la temperatura va a oscilar alrededor del techo, no a quedarse clavada.
 *
 *  OJO CON EL SOBREPASO
 *    Al cortar la potencia, la temperatura sigue subiendo un rato por inercia
 *    térmica. El pico real va a quedar POR ENCIMA del techo. Anotá cuánto: ese
 *    número es justamente lo que necesitamos para sintonizar el PID después.
 *
 *  COMANDOS (monitor serie a 115200 baudios)
 *    P20    potencia manual al 20 %   (P0 a P100, limitado por POTENCIA_MAX)
 *    ON     potencia al máximo permitido
 *    OFF    potencia a 0
 *    T180   fija el techo en 180 °C
 *    LIM    activa o desactiva el limitador
 *    D      lectura detallada
 *    R      reinicia mínimo y máximo registrados
 * ============================================================================
 */

#include <max6675.h>

// --------------------------------------------------------------- PINES ----
const int PIN_SO  = 19;   // MAX6675 SO  (MISO)
const int PIN_CS  = 5;    // MAX6675 CS
const int PIN_SCK = 18;   // MAX6675 SCK
const int PIN_SSR = 23;   // entrada (+) del relé
const int PIN_LED = 2;    // LED de la placa ESP32 DevKit v1

// ===================== AJUSTÁ ESTO =========================================
// Techo de temperatura. Mientras estés tanteando, dejá LIMITE_ACTIVO en false:
// el termopar solo mide y no interviene. Cuando sepas el número, ponelo acá y
// pasá LIMITE_ACTIVO a true. También se puede cambiar en caliente con T y LIM.
const bool   LIMITE_ACTIVO = false;   // true = el techo regula la potencia
const double TECHO_C       = 180.0;   // grados C
const double HISTERESIS_C  = 5.0;     // reanuda al bajar este tanto del techo

// Techo de potencia. Recorta cualquier comando, para que un P100 mal tecleado
// no pueda mandar la resistencia a plena potencia.
const int POTENCIA_MAX = 30;
// ===========================================================================

// ---------------------------------------------------------- PARÁMETROS ----
const unsigned long VENTANA_MS      = 2000;  // ventana de ancho de pulso
const unsigned long PERIODO_LECTURA = 300;   // el MAX6675 necesita ≥ 250 ms
const unsigned long PERIODO_LOG     = 1000;
const unsigned long PERIODO_MUESTRA = 5000;  // para la tasa de cambio

// Corte duro, independiente del techo configurable. Es el backstop por si el
// techo quedó mal puesto.
const double TEMP_MAX_ABSOLUTA = 260.0;
const double TEMP_MIN_VALIDA   = -5.0;
const double TEMP_MAX_VALIDA   = 500.0;
const uint8_t LECTURAS_MALAS_MAX = 5;

// -------------------------------------------------------------- ESTADO ----
MAX6675 termopar(PIN_SCK, PIN_CS, PIN_SO);

int    potenciaPedida   = 0;    // lo que pediste por serie
int    potenciaAplicada = 0;    // lo que realmente sale, tras techo y seguridad
double techo            = TECHO_C;
bool   limiteActivo     = LIMITE_ACTIVO;
bool   limitando        = false;   // el techo está recortando ahora mismo
bool   bloqueado        = false;   // seguridad disparada
bool   estadoSSR        = false;

double temp     = 0.0;
double tempMin  =  9999.0;
double tempMax  = -9999.0;
uint8_t lecturasMalas = 0;

double tempMuestra     = 0.0;
unsigned long tMuestra = 0;
double tasaPorMinuto   = 0.0;

unsigned long inicioVentana = 0;
unsigned long ultimaLectura = 0;
unsigned long ultimoLog     = 0;

// ---------------------------------------------------------------- UTIL ----
int limitarPotencia(int v) {
  if (v < 0) return 0;
  if (v > POTENCIA_MAX) return POTENCIA_MAX;
  return v;
}

void aplicar(bool encendido) {
  estadoSSR = encendido;
  digitalWrite(PIN_SSR, encendido ? HIGH : LOW);
  digitalWrite(PIN_LED, encendido ? HIGH : LOW);
}

void bloquear(const char *motivo) {
  if (bloqueado) return;
  bloqueado = true;
  potenciaPedida = 0;
  aplicar(false);
  Serial.println();
  Serial.println("###############################################");
  Serial.print  ("## SEGURIDAD: "); Serial.println(motivo);
  Serial.println("## Potencia forzada a 0.");
  Serial.println("## Corregi el problema y volve a mandar P<n>.");
  Serial.println("###############################################");
  Serial.println();
}

bool leer(double &out) {
  double t = termopar.readCelsius();
  if (isnan(t) || t < TEMP_MIN_VALIDA || t > TEMP_MAX_VALIDA) return false;
  out = t;
  return true;
}

bool sensorOK() { return lecturasMalas < LECTURAS_MALAS_MAX; }

// --------------------------------------------------------------- SETUP ----
void setup() {
  pinMode(PIN_SSR, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  aplicar(false);                 // arranque seguro

  Serial.begin(115200);
  delay(500);                     // el MAX6675 necesita estabilizarse

  Serial.println();
  Serial.println("=== Termopar + potencia manual con techo ===");
  Serial.print  ("Techo de potencia : "); Serial.print(POTENCIA_MAX); Serial.println(" %");
  Serial.print  ("Techo de temp.    : "); Serial.print(techo, 1);
  Serial.print  (" C  ["); Serial.print(limiteActivo ? "ACTIVO" : "apagado"); Serial.println("]");
  Serial.println();

  Serial.println("Leyendo el termopar 5 veces...");
  int validas = 0;
  double suma = 0;
  for (int i = 0; i < 5; i++) {
    double t;
    if (leer(t)) { validas++; suma += t; Serial.print("  "); Serial.print(t, 2); Serial.println(" C"); }
    else         { Serial.println("  --- lectura invalida ---"); }
    delay(350);
  }

  if (validas == 0) {
    Serial.println();
    Serial.println("El termopar NO responde. Revisa, en este orden:");
    Serial.println("  1. VCC del MAX6675 en 3V3 (no 5V) y GND comun con el ESP32.");
    Serial.println("  2. SCK=18, CS=5, SO=19. Es facil confundir SO con SCK.");
    Serial.println("  3. Que la sonda este bien apretada en los bornes T+ y T-.");
  } else {
    temp        = suma / validas;
    tempMuestra = temp;
    Serial.println();
    Serial.print("Temperatura inicial: "); Serial.print(temp, 2); Serial.println(" C");
    Serial.println("Si no se parece a la del cuarto, revisa la polaridad.");
    Serial.println("Prueba: calenta la punta con la mano. Debe SUBIR.");
  }

  Serial.println();
  Serial.println("Arranque en 0 %. No calienta hasta que lo pidas.");
  Serial.println("Comandos: P<0-100> | ON | OFF | T<grados> | LIM | D | R");
  Serial.println("--------------------------------------------------------");

  inicioVentana = millis();
  ultimaLectura = millis();
  tMuestra      = millis();
}

// ---------------------------------------------------------------- LOOP ----
void loop() {
  unsigned long ahora = millis();

  atenderSerial();

  // ---- Lectura del termopar ----
  if (ahora - ultimaLectura >= PERIODO_LECTURA) {
    ultimaLectura = ahora;
    double t;
    if (leer(t)) {
      temp = t;
      lecturasMalas = 0;
      if (temp < tempMin) tempMin = temp;
      if (temp > tempMax) tempMax = temp;
    } else {
      lecturasMalas++;
      if (lecturasMalas >= LECTURAS_MALAS_MAX) {
        bloquear("termopar abierto o lectura fuera de rango");
      }
    }
  }

  // ---- Tasa de cambio ----
  if (ahora - tMuestra >= PERIODO_MUESTRA) {
    double minutos = (ahora - tMuestra) / 60000.0;
    tasaPorMinuto  = (temp - tempMuestra) / minutos;
    tempMuestra    = temp;
    tMuestra       = ahora;
  }

  // ---- Corte duro por sobretemperatura ----
  if (sensorOK() && temp >= TEMP_MAX_ABSOLUTA) {
    bloquear("sobretemperatura absoluta");
  }

  // ---- Techo de temperatura, con histéresis ----
  // Regulación todo-o-nada: corta al llegar al techo, reanuda al bajar la
  // histéresis. La temperatura oscila dentro de esa banda.
  if (limiteActivo && sensorOK() && !bloqueado) {
    if (limitando) {
      if (temp <= techo - HISTERESIS_C) {
        limitando = false;
        Serial.print(">> Por debajo de "); Serial.print(techo - HISTERESIS_C, 1);
        Serial.println(" C: reanuda el calentamiento.");
      }
    } else {
      if (temp >= techo) {
        limitando = true;
        Serial.print(">> Techo alcanzado ("); Serial.print(temp, 1);
        Serial.println(" C): corta la potencia.");
      }
    }
  } else {
    limitando = false;
  }

  // ---- Potencia efectiva ----
  potenciaAplicada = potenciaPedida;
  if (bloqueado || limitando) potenciaAplicada = 0;

  // ---- Ancho de pulso sobre la ventana ----
  while (ahora - inicioVentana >= VENTANA_MS) {
    inicioVentana += VENTANA_MS;
  }
  unsigned long transcurrido = ahora - inicioVentana;
  unsigned long tiempoON     = (unsigned long)((potenciaAplicada / 100.0) * VENTANA_MS);

  aplicar(tiempoON > transcurrido);

  logPeriodico(ahora);
}

// ----------------------------------------------------------- TELEMETRIA ---
void logPeriodico(unsigned long ahora) {
  if (ahora - ultimoLog < PERIODO_LOG) return;
  ultimoLog = ahora;

  if (bloqueado)       Serial.print("[BLOQUEADO] ");
  else if (limitando)  Serial.print("[ TECHO   ] ");
  else if (!sensorOK())Serial.print("[SIN LECT ] ");
  else                 Serial.print("[   OK    ] ");

  Serial.print("T=");
  if (!sensorOK()) Serial.print("  ---");
  else             Serial.print(temp, 1);
  Serial.print(" C  ");

  Serial.print("techo ");
  Serial.print(techo, 0);
  Serial.print(limiteActivo ? "(ON)  " : "(--)  ");

  Serial.print("tasa=");     Serial.print(tasaPorMinuto, 1);   Serial.print(" C/min  ");
  Serial.print("pedida ");   Serial.print(potenciaPedida);     Serial.print(" %  ");
  Serial.print("aplicada "); Serial.print(potenciaAplicada);   Serial.print(" %  ");
  Serial.print("SSR ");      Serial.print(estadoSSR ? "ON " : "OFF");

  int casillas = (potenciaAplicada * 20) / 100;
  Serial.print("  [");
  for (int i = 0; i < 20; i++) Serial.print(i < casillas ? '#' : '.');
  Serial.println("]");
}

void lecturaDetallada() {
  Serial.println();
  Serial.println("--- detalle ---");
  Serial.print("Temperatura actual  : "); Serial.print(temp, 2); Serial.println(" C");
  Serial.print("Minima registrada   : ");
  if (tempMin > 9000) Serial.println("sin datos"); else { Serial.print(tempMin, 2); Serial.println(" C"); }
  Serial.print("Maxima registrada   : ");
  if (tempMax < -9000) Serial.println("sin datos"); else { Serial.print(tempMax, 2); Serial.println(" C"); }
  Serial.print("Tasa de cambio      : "); Serial.print(tasaPorMinuto, 2); Serial.println(" C/min");
  Serial.print("Techo de temperatura: "); Serial.print(techo, 1); Serial.print(" C  ");
  Serial.println(limiteActivo ? "[ACTIVO]" : "[apagado]");
  Serial.print("Histeresis          : "); Serial.print(HISTERESIS_C, 1); Serial.println(" C");
  Serial.print("Potencia pedida     : "); Serial.print(potenciaPedida); Serial.println(" %");
  Serial.print("Potencia aplicada   : "); Serial.print(potenciaAplicada); Serial.println(" %");
  Serial.print("Techo de potencia   : "); Serial.print(POTENCIA_MAX); Serial.println(" %");
  Serial.print("Estado              : ");
  if (bloqueado)      Serial.println("BLOQUEADO");
  else if (limitando) Serial.println("recortando por techo");
  else                Serial.println("normal");
  Serial.println("---------------");
  Serial.println();
}

// --------------------------------------------------------------- SERIAL ---
void atenderSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (cmd == "D") { lecturaDetallada(); return; }

  if (cmd == "R") {
    tempMin =  9999.0;
    tempMax = -9999.0;
    Serial.println(">> Minimo y maximo reiniciados.");
    return;
  }

  if (cmd == "LIM") {
    limiteActivo = !limiteActivo;
    Serial.print(">> Limitador por techo: ");
    Serial.println(limiteActivo ? "ACTIVO" : "apagado");
    if (limiteActivo) {
      Serial.print("   Techo en "); Serial.print(techo, 1);
      Serial.print(" C, reanuda al bajar de "); Serial.print(techo - HISTERESIS_C, 1);
      Serial.println(" C");
    }
    return;
  }

  if (cmd.charAt(0) == 'T') {
    double v = cmd.substring(1).toFloat();
    if (v < 30.0 || v > TEMP_MAX_ABSOLUTA - 10.0) {
      Serial.print(">> Techo fuera de rango. Usa entre 30 y ");
      Serial.print(TEMP_MAX_ABSOLUTA - 10.0, 0); Serial.println(" C.");
    } else {
      techo = v;
      Serial.print(">> Nuevo techo: "); Serial.print(techo, 1); Serial.println(" C");
      if (!limiteActivo) Serial.println("   (el limitador sigue apagado; activalo con LIM)");
    }
    return;
  }

  // Los comandos de potencia intentan desbloquear primero.
  bool pedidoDePotencia = (cmd == "ON" || cmd == "OFF" || cmd.charAt(0) == 'P');
  if (pedidoDePotencia && bloqueado) {
    if (!sensorOK()) {
      Serial.println(">> Sigue sin lectura valida del termopar. No se desbloquea.");
      return;
    }
    if (temp >= TEMP_MAX_ABSOLUTA) {
      Serial.print(">> Sigue por encima del limite absoluto ("); Serial.print(temp, 1);
      Serial.println(" C). No se desbloquea.");
      return;
    }
    bloqueado = false;
    Serial.println(">> Desbloqueado.");
  }

  if (cmd == "ON") {
    potenciaPedida = POTENCIA_MAX;
    Serial.print(">> Potencia al maximo permitido: "); Serial.print(potenciaPedida); Serial.println(" %");
  } else if (cmd == "OFF") {
    potenciaPedida = 0;
    Serial.println(">> Potencia 0 %");
  } else if (cmd.charAt(0) == 'P') {
    int v = cmd.substring(1).toInt();
    if (v < 0 || v > 100) {
      Serial.println(">> Fuera de rango. Usa P0 a P100.");
    } else {
      potenciaPedida = limitarPotencia(v);
      Serial.print(">> Potencia manual: "); Serial.print(potenciaPedida); Serial.println(" %");
      if (potenciaPedida < v) {
        Serial.print("   (recortado por POTENCIA_MAX = "); Serial.print(POTENCIA_MAX); Serial.println(" %)");
      }
    }
  } else {
    Serial.println(">> Comando no reconocido. Usa P<0-100>, ON, OFF, T<grados>, LIM, D o R.");
  }
}
