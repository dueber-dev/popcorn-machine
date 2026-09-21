/*
 * ============================================================================
 *  PRUEBA DE BANCO 2 — ESP32 + MAX6675 + relé de estado sólido
 *  Lectura del termopar + control MANUAL de la potencia. Todavía sin PID.
 * ----------------------------------------------------------------------------
 *  Librería necesaria (Arduino IDE → Gestor de librerías):
 *    "MAX6675 library" de Adafruit
 *
 *  CONEXIONES
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
 *    Relé de estado sólido   ESP32
 *    ---------------------   -----------------------------------
 *    terminal 3 (+)      →   GPIO23
 *    terminal 4 (−)      →   GND
 *
 *    Terminales 1 y 2 del relé: VACÍOS mientras sea prueba de banco.
 *
 *  QUÉ VERIFICAR
 *    1. Temperatura ambiente plausible al arrancar.
 *    2. Polaridad: calentá la punta con la mano. La lectura debe SUBIR.
 *       Si baja, T+ y T− están invertidos.
 *    3. Agua hirviendo: ~100 °C a nivel del mar, restá ~1 °C por cada 300 m
 *       de altitud. Agua con hielo: ~0 °C. Dos puntos que cuadren dentro de
 *       2-3 °C confirman sonda y módulo.
 *    4. Desconectá el termopar en caliente: debe aparecer FALLO DE SENSOR y
 *       la potencia debe caer a 0 sola.
 *
 *  COMANDOS (monitor serie a 115200 baudios)
 *    P50    potencia manual al 50 %   (P0 a P100)
 *    ON     100 %
 *    OFF    0 %
 *    T      lectura detallada de una sola vez
 *    R      reinicia los valores mínimo y máximo registrados
 * ============================================================================
 */

#include <max6675.h>

// --------------------------------------------------------------- PINES ----
const int PIN_SO  = 19;   // MAX6675 SO  (MISO)
const int PIN_CS  = 5;    // MAX6675 CS
const int PIN_SCK = 18;   // MAX6675 SCK
const int PIN_SSR = 23;   // relé, terminal 3 (+)
const int PIN_LED = 2;    // LED de la placa ESP32 DevKit v1

// ---------------------------------------------------------- PARÁMETROS ----
const unsigned long VENTANA_MS        = 2000;  // ventana de ancho de pulso
const unsigned long PERIODO_LECTURA   = 300;   // el MAX6675 necesita ≥ 250 ms
const unsigned long PERIODO_LOG       = 1000;
const unsigned long PERIODO_MUESTRA   = 5000;  // para la tasa de cambio

// Seguridad: la potencia es manual, así que alguien podría poner 100 % y
// distraerse. Estos dos límites fuerzan la potencia a 0 pase lo que pase.
const double TEMP_MAX         = 250.0;  // corte por sobretemperatura
const double TEMP_MIN_VALIDA  = -5.0;
const double TEMP_MAX_VALIDA  = 500.0;
const uint8_t LECTURAS_MALAS_MAX = 5;

// -------------------------------------------------------------- ESTADO ----
MAX6675 termopar(PIN_SCK, PIN_CS, PIN_SO);

int    potencia   = 0;        // 0-100 %, puesta a mano
double temp       = 0.0;
double tempMin    =  9999.0;
double tempMax    = -9999.0;
bool   estadoSSR  = false;
bool   bloqueado  = false;    // seguridad disparada: potencia forzada a 0
uint8_t lecturasMalas = 0;

double tempMuestra      = 0.0;   // para calcular °C/min
unsigned long tMuestra  = 0;
double tasaPorMinuto    = 0.0;

unsigned long inicioVentana = 0;
unsigned long ultimaLectura = 0;
unsigned long ultimoLog     = 0;

// ---------------------------------------------------------------- UTIL ----
void aplicar(bool encendido) {
  estadoSSR = encendido;
  digitalWrite(PIN_SSR, encendido ? HIGH : LOW);
  digitalWrite(PIN_LED, encendido ? HIGH : LOW);
}

void bloquear(const char *motivo) {
  if (bloqueado) return;
  bloqueado = true;
  potencia  = 0;
  aplicar(false);
  Serial.println();
  Serial.println("###############################################");
  Serial.print  ("## SEGURIDAD: "); Serial.println(motivo);
  Serial.println("## Potencia forzada a 0.");
  Serial.println("## Corregi el problema y volve a mandar P<n>.");
  Serial.println("###############################################");
  Serial.println();
}

// Devuelve true si la lectura es válida.
bool leer(double &out) {
  double t = termopar.readCelsius();
  if (isnan(t) || t < TEMP_MIN_VALIDA || t > TEMP_MAX_VALIDA) return false;
  out = t;
  return true;
}

// --------------------------------------------------------------- SETUP ----
void setup() {
  pinMode(PIN_SSR, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  aplicar(false);                 // arranque seguro

  Serial.begin(115200);
  delay(500);                     // el MAX6675 necesita estabilizarse

  Serial.println();
  Serial.println("=== PRUEBA 2 :: termopar + potencia manual ===");
  Serial.println();

  // Cinco lecturas de arranque para ver si el sensor responde y es estable.
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
    Serial.println("El control manual sigue disponible, pero sin lectura.");
  } else {
    temp        = suma / validas;
    tempMuestra = temp;
    Serial.println();
    Serial.print("Temperatura inicial: "); Serial.print(temp, 2); Serial.println(" C");
    Serial.println("Si no se parece a la temperatura del cuarto, revisa la polaridad.");
    Serial.println("Prueba: calenta la punta con la mano. Debe SUBIR.");
  }

  Serial.println();
  Serial.println("Comandos: P<0-100> | ON | OFF | T | R");
  Serial.println("------------------------------------------------------");

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

  // ---- Tasa de cambio, util para juzgar la respuesta termica ----
  if (ahora - tMuestra >= PERIODO_MUESTRA) {
    double minutos = (ahora - tMuestra) / 60000.0;
    tasaPorMinuto  = (temp - tempMuestra) / minutos;
    tempMuestra    = temp;
    tMuestra       = ahora;
  }

  // ---- Seguridad por sobretemperatura ----
  if (lecturasMalas == 0 && temp >= TEMP_MAX) {
    bloquear("sobretemperatura");
  }

  // ---- Ancho de pulso sobre la ventana ----
  while (ahora - inicioVentana >= VENTANA_MS) {
    inicioVentana += VENTANA_MS;
  }
  unsigned long transcurrido = ahora - inicioVentana;
  unsigned long tiempoON     = (unsigned long)((potencia / 100.0) * VENTANA_MS);

  aplicar(!bloqueado && tiempoON > transcurrido);

  logPeriodico(ahora);
}

// ----------------------------------------------------------- TELEMETRIA ---
void logPeriodico(unsigned long ahora) {
  if (ahora - ultimoLog < PERIODO_LOG) return;
  ultimoLog = ahora;

  if (bloqueado)            Serial.print("[BLOQUEADO] ");
  else if (lecturasMalas)   Serial.print("[SIN LECT ] ");
  else                      Serial.print("[   OK    ] ");

  Serial.print("T=");
  if (lecturasMalas >= LECTURAS_MALAS_MAX) Serial.print("  ---");
  else                                     Serial.print(temp, 1);
  Serial.print(" C  ");

  Serial.print("tasa=");  Serial.print(tasaPorMinuto, 1);  Serial.print(" C/min  ");
  Serial.print("Pot=");   Serial.print(potencia);          Serial.print(" %  ");
  Serial.print("SSR ");   Serial.print(estadoSSR ? "ON " : "OFF");

  // Barra de 20 casillas con la potencia
  int casillas = (potencia * 20) / 100;
  Serial.print("  [");
  for (int i = 0; i < 20; i++) Serial.print(i < casillas ? '#' : '.');
  Serial.println("]");
}

void lecturaDetallada() {
  Serial.println();
  Serial.println("--- lectura detallada ---");
  Serial.print("Temperatura actual : "); Serial.print(temp, 2); Serial.println(" C");
  Serial.print("Minima registrada  : ");
  if (tempMin > 9000) Serial.println("sin datos"); else { Serial.print(tempMin, 2); Serial.println(" C"); }
  Serial.print("Maxima registrada  : ");
  if (tempMax < -9000) Serial.println("sin datos"); else { Serial.print(tempMax, 2); Serial.println(" C"); }
  Serial.print("Tasa de cambio     : "); Serial.print(tasaPorMinuto, 2); Serial.println(" C/min");
  Serial.print("Potencia            : "); Serial.print(potencia); Serial.println(" %");
  Serial.print("Lecturas malas segu.: "); Serial.println(lecturasMalas);
  Serial.print("Estado              : "); Serial.println(bloqueado ? "BLOQUEADO" : "normal");
  Serial.println("-------------------------");
  Serial.println();
}

// --------------------------------------------------------------- SERIAL ---
void atenderSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (cmd == "T") {
    lecturaDetallada();
    return;
  }

  if (cmd == "R") {
    tempMin =  9999.0;
    tempMax = -9999.0;
    Serial.println(">> Minimo y maximo reiniciados.");
    return;
  }

  // Los comandos de potencia intentan desbloquear primero.
  bool pedidoDePotencia = (cmd == "ON" || cmd == "OFF" || cmd.charAt(0) == 'P');
  if (pedidoDePotencia && bloqueado) {
    if (lecturasMalas >= LECTURAS_MALAS_MAX) {
      Serial.println(">> Sigue sin lectura valida del termopar. No se desbloquea.");
      return;
    }
    if (temp >= TEMP_MAX) {
      Serial.print(">> Sigue por encima del limite ("); Serial.print(temp, 1);
      Serial.println(" C). No se desbloquea.");
      return;
    }
    bloqueado = false;
    Serial.println(">> Desbloqueado.");
  }

  if (cmd == "ON") {
    potencia = 100;
    Serial.println(">> Potencia 100 %");
  } else if (cmd == "OFF") {
    potencia = 0;
    Serial.println(">> Potencia 0 %");
  } else if (cmd.charAt(0) == 'P') {
    int v = cmd.substring(1).toInt();
    if (v < 0 || v > 100) {
      Serial.println(">> Fuera de rango. Usa P0 a P100.");
    } else {
      potencia = v;
      Serial.print(">> Potencia manual: "); Serial.print(potencia); Serial.println(" %");
    }
  } else {
    Serial.println(">> Comando no reconocido. Usa P<0-100>, ON, OFF, T o R.");
  }
}
