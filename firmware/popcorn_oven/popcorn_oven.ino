/*
 * ============================================================================
 *  Horno de conveccion basado en palomitera AC
 *  ESP32 + MAX6675 (termopar tipo K) + SSR-25DA
 * ----------------------------------------------------------------------------
 *  Control PID por ancho de pulso en ventana de tiempo (time-proportional),
 *  con capas de seguridad en software:
 *
 *    1. Deteccion de fallo del termopar (NAN / lectura fuera de rango).
 *    2. Limite duro de temperatura (corte inmediato del SSR).
 *    3. Deteccion de fuga termica / sensor suelto (100% de salida sin que
 *       la temperatura suba -> aborta).
 *    4. Estado FALLO enclavado: solo sale con reset fisico del ESP32.
 *    5. Arranque seguro: SSR en LOW antes que cualquier otra cosa.
 *
 *  Librerias (Arduino IDE -> Gestor de librerias):
 *    - "MAX6675 library" (Adafruit)
 *    - "PID" de Brett Beauregard (PID_v1)
 *
 *  HARDWARE OBLIGATORIO:
 *    - Resistencia de 10 kOhm entre PIN_SSR y GND (pulldown). Sin ella el
 *      GPIO queda flotante durante el boot/reset y el SSR puede dispararse.
 *    - El MAX6675 se alimenta a 3.3 V, NUNCA a 5 V: su pin SO atacaria el
 *      GPIO19 del ESP32, que no es tolerante a 5 V.
 * ============================================================================
 */

#include <max6675.h>
#include <PID_v1.h>

// ---------------------------------------------------------------- PINES ----
const int PIN_SO  = 19;   // MAX6675 SO  (MISO)
const int PIN_CS  = 5;    // MAX6675 CS
const int PIN_SCK = 18;   // MAX6675 SCK
const int PIN_SSR = 23;   // Entrada (+) del SSR-25DA  [requiere pulldown 10k]

// ------------------------------------------------------------ PARAMETROS ----
const double SETPOINT_INICIAL = 200.0;  // grados C
const double SETPOINT_MIN     = 40.0;
const double SETPOINT_MAX     = 240.0;  // tope de consigna que acepta el usuario

// Seguridad
const double TEMP_MAX_ABSOLUTA = 270.0; // corte duro: por encima -> FALLO
const double TEMP_MIN_VALIDA   = -5.0;  // lectura por debajo -> sensor malo
const double TEMP_MAX_VALIDA   = 500.0; // lectura por encima -> sensor malo
const uint8_t LECTURAS_MALAS_MAX = 5;   // lecturas invalidas seguidas toleradas

// Deteccion de fuga termica: con salida al 100% durante este tiempo,
// la temperatura debe haber subido al menos DELTA grados.
const unsigned long RUNAWAY_VENTANA_MS = 90000UL;  // 90 s
const double        RUNAWAY_DELTA_MIN  = 5.0;      // grados C

// Temporizacion
const unsigned long PERIODO_LECTURA_MS = 300;   // MAX6675 necesita >= 250 ms
const unsigned long VENTANA_SSR_MS     = 2000;  // ventana de ciclo del SSR
const unsigned long PERIODO_PID_MS     = 1000;  // sample time del PID
const unsigned long PERIODO_LOG_MS     = 1000;

// --------------------------------------------------------------- PID -------
// La salida del PID esta en PORCENTAJE de potencia (0-100), no en ms.
// Asi las ganancias no dependen del tamano de la ventana.
//
//   Kp = 100 / banda_proporcional   ->  Kp = 4.0 equivale a una banda de 25 C
//   Ti = Kp / Ki = 80 s   (accion integral)
//   Td = Kd / Kp = 5 s    (accion derivativa)
//
// Punto de partida conservador. Ver "Sintonizacion" en el README.
double Kp = 4.0, Ki = 0.05, Kd = 20.0;

double tempActual   = 0.0;
double salidaPID    = 0.0;   // 0-100 %
double tempDeseada  = SETPOINT_INICIAL;

MAX6675 termopar(PIN_SCK, PIN_CS, PIN_SO);
PID myPID(&tempActual, &salidaPID, &tempDeseada, Kp, Ki, Kd, DIRECT);

// -------------------------------------------------------------- ESTADO -----
enum Estado { ESPERA, CALENTANDO, FALLO };
Estado estado = ESPERA;
String motivoFallo = "";

unsigned long inicioVentana     = 0;
unsigned long ultimaLectura     = 0;
unsigned long ultimoLog         = 0;
unsigned long inicioRunaway     = 0;
double        tempInicioRunaway = 0.0;
bool          runawayArmado     = false;
uint8_t       lecturasMalas     = 0;

// --------------------------------------------------------------- UTIL ------
void apagarSSR() {
  digitalWrite(PIN_SSR, LOW);
}

void entrarEnFallo(const String &motivo) {
  apagarSSR();
  myPID.SetMode(MANUAL);
  salidaPID = 0.0;
  estado = FALLO;
  motivoFallo = motivo;
  Serial.println();
  Serial.println("################################################");
  Serial.print  ("## FALLO: "); Serial.println(motivo);
  Serial.println("## Resistencia APAGADA. Reset fisico para salir.");
  Serial.println("################################################");
}

// Lee el termopar. Devuelve true si la lectura es valida.
bool leerTemperatura(double &out) {
  double t = termopar.readCelsius();
  if (isnan(t) || t < TEMP_MIN_VALIDA || t > TEMP_MAX_VALIDA) return false;
  out = t;
  return true;
}

// ---------------------------------------------------------------- SETUP ----
void setup() {
  // Lo PRIMERO: garantizar que el SSR esta abierto.
  pinMode(PIN_SSR, OUTPUT);
  apagarSSR();

  Serial.begin(115200);
  delay(500);                 // el MAX6675 necesita estabilizarse tras el power-up

  Serial.println();
  Serial.println("=== Horno palomitera :: ESP32 + MAX6675 + SSR ===");

  // Primera lectura de cordura antes de permitir calentar.
  double t;
  if (!leerTemperatura(t)) {
    entrarEnFallo("Termopar no responde en el arranque");
    return;
  }
  tempActual = t;
  Serial.print("Temperatura inicial: "); Serial.print(tempActual); Serial.println(" C");

  myPID.SetOutputLimits(0, 100);          // salida en % de potencia
  myPID.SetSampleTime(PERIODO_PID_MS);
  myPID.SetMode(AUTOMATIC);

  inicioVentana = millis();
  ultimaLectura = millis();
  estado = CALENTANDO;

  Serial.print("Setpoint: "); Serial.print(tempDeseada); Serial.println(" C");
  Serial.println("Comandos: S<valor> = setpoint  |  OFF = parar  |  ON = reanudar");
  Serial.println("--------------------------------------------------");
}

// ----------------------------------------------------------------- LOOP ----
void loop() {
  unsigned long ahora = millis();

  atenderSerial();

  // ---- 1. Adquisicion ------------------------------------------------------
  if (ahora - ultimaLectura >= PERIODO_LECTURA_MS) {
    ultimaLectura = ahora;
    double t;
    if (leerTemperatura(t)) {
      tempActual = t;
      lecturasMalas = 0;
    } else {
      lecturasMalas++;
      if (lecturasMalas >= LECTURAS_MALAS_MAX && estado != FALLO) {
        entrarEnFallo("Lecturas invalidas del termopar (cable abierto?)");
      }
    }
  }

  // ---- 2. Seguridades ------------------------------------------------------
  if (estado == FALLO) {
    apagarSSR();                 // redundancia: nunca confiar en un solo punto
    logPeriodico(ahora);
    return;
  }

  if (tempActual >= TEMP_MAX_ABSOLUTA) {
    entrarEnFallo("Sobretemperatura: " + String(tempActual, 1) + " C");
    return;
  }

  vigilarRunaway(ahora);

  // ---- 3. Control ----------------------------------------------------------
  if (estado == CALENTANDO) {
    myPID.Compute();
  } else {
    salidaPID = 0.0;
  }

  // ---- 4. Actuacion: ancho de pulso sobre la ventana -----------------------
  // while (no if) para que un bloqueo largo del loop no deje la ventana
  // desalineada de forma permanente.
  while (ahora - inicioVentana >= VENTANA_SSR_MS) {
    inicioVentana += VENTANA_SSR_MS;
  }

  unsigned long transcurrido = ahora - inicioVentana;
  unsigned long tiempoON     = (unsigned long)((salidaPID / 100.0) * VENTANA_SSR_MS);

  if (estado == CALENTANDO && tiempoON > transcurrido) {
    digitalWrite(PIN_SSR, HIGH);
  } else {
    apagarSSR();
  }

  logPeriodico(ahora);
}

// ------------------------------------------------- DETECCION DE RUNAWAY ----
// Si el PID pide practicamente toda la potencia durante RUNAWAY_VENTANA_MS
// y la temperatura no sube al menos RUNAWAY_DELTA_MIN grados, algo va mal:
// termopar descolgado del cilindro, SSR muerto, resistencia abierta o
// bimetalico abierto. En cualquiera de esos casos hay que parar.
void vigilarRunaway(unsigned long ahora) {
  bool aPlenaPotencia = (estado == CALENTANDO) && (salidaPID > 95.0);

  if (!aPlenaPotencia) {
    runawayArmado = false;
    return;
  }

  if (!runawayArmado) {
    runawayArmado     = true;
    inicioRunaway     = ahora;
    tempInicioRunaway = tempActual;
    return;
  }

  if (ahora - inicioRunaway >= RUNAWAY_VENTANA_MS) {
    if (tempActual - tempInicioRunaway < RUNAWAY_DELTA_MIN) {
      entrarEnFallo("Sin respuesta termica al 100% de potencia");
    } else {
      // Rearmar la ventana para seguir vigilando.
      inicioRunaway     = ahora;
      tempInicioRunaway = tempActual;
    }
  }
}

// ------------------------------------------------------------ TELEMETRIA ---
void logPeriodico(unsigned long ahora) {
  if (ahora - ultimoLog < PERIODO_LOG_MS) return;
  ultimoLog = ahora;

  const char *nombreEstado = (estado == FALLO)      ? "FALLO"
                           : (estado == CALENTANDO) ? "CALENTANDO"
                                                    : "ESPERA";

  Serial.print("["); Serial.print(nombreEstado); Serial.print("] ");
  Serial.print("T=");   Serial.print(tempActual, 1);  Serial.print(" C  ");
  Serial.print("SP=");  Serial.print(tempDeseada, 1); Serial.print(" C  ");
  Serial.print("Pot="); Serial.print(salidaPID, 1);   Serial.print(" %");
  if (estado == FALLO) { Serial.print("  <- "); Serial.print(motivoFallo); }
  Serial.println();
}

// --------------------------------------------------------------- SERIAL ----
// S220  -> cambia el setpoint a 220 C
// OFF   -> deja de calentar (sigue midiendo)
// ON    -> reanuda el control
void atenderSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (estado == FALLO) {
    Serial.println("En FALLO: haz reset fisico del ESP32 tras revisar el hardware.");
    return;
  }

  if (cmd == "OFF") {
    estado = ESPERA;
    myPID.SetMode(MANUAL);
    salidaPID = 0.0;
    apagarSSR();
    Serial.println(">> Calentamiento detenido.");
  } else if (cmd == "ON") {
    estado = CALENTANDO;
    myPID.SetMode(AUTOMATIC);   // PID_v1 hace bumpless transfer por si solo
    Serial.println(">> Calentamiento reanudado.");
  } else if (cmd.charAt(0) == 'S') {
    double nuevo = cmd.substring(1).toFloat();
    if (nuevo < SETPOINT_MIN || nuevo > SETPOINT_MAX) {
      Serial.print(">> Setpoint fuera de rango (");
      Serial.print(SETPOINT_MIN); Serial.print(" - ");
      Serial.print(SETPOINT_MAX); Serial.println(" C)");
    } else {
      tempDeseada = nuevo;
      Serial.print(">> Nuevo setpoint: "); Serial.print(tempDeseada); Serial.println(" C");
    }
  } else {
    Serial.println(">> Comando no reconocido. Usa S<valor>, ON u OFF.");
  }
}
