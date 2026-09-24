/*
 * ============================================================================
 *  PRUEBA DE BANCO — ESP32 + relé de estado sólido (comercial o construido)
 *  Control MANUAL de la potencia por ancho de pulso. Sin termopar, sin PID.
 * ----------------------------------------------------------------------------
 *  Sirve para dos cosas:
 *    1. Verificar el relé en seco o con una lámpara de prueba.
 *    2. Primeros disparos con la resistencia real, a baja potencia.
 *
 *  CONEXIONES (solo tres cables del lado de control):
 *    ESP32 GPIO23  →  entrada (+) del relé   [MOC3063 pin 1 vía 330 Ω]
 *    ESP32 GND     →  entrada (−) del relé   [MOC3063 pin 2]
 *    ESP32 USB     →  computadora
 *
 *  COMANDOS (monitor serie a 115200 baudios):
 *    P50    fija la potencia en 50 %   (P0 a P100, limitado por POTENCIA_MAX)
 *    AUTO   barrido automático: 0, 25, 50, 75, 100 %, 8 s cada escalón
 *    ON     potencia al máximo permitido
 *    OFF    0 %
 *
 *  CÓMO VERIFICAR SIN CARGA
 *    Multímetro en VOLTIOS DC entre GPIO23 y GND: como el multímetro promedia,
 *    deberías leer aproximadamente potencia% × 3,3 V.
 *      100 % → ~3,3 V    50 % → ~1,65 V    25 % → ~0,83 V    0 % → ~0 V
 * ============================================================================
 */

const int PIN_SSR = 23;   // entrada (+) del relé
const int PIN_LED = 2;    // LED de la placa ESP32 DevKit v1

const unsigned long VENTANA_MS   = 2000;  // misma ventana que el firmware real
const unsigned long PASO_AUTO_MS = 8000;  // duración de cada escalón en AUTO
const unsigned long PERIODO_LOG  = 500;

// ------------------------------------------------------- CONFIGURACIÓN ----
// Techo de potencia. Con la resistencia real, empezá en 25 y subilo recién
// cuando sepas cómo responde. Con este tope, un P100 mal tecleado no puede
// mandar la resistencia a plena potencia.
const int POTENCIA_MAX = 25;

// Con carga real conviene arrancar quieto: en manual y en 0 %.
// Para la prueba con lámpara podés poner ambos en true.
const bool ARRANCAR_EN_AUTO   = false;  // true = barrido automático al bootear
const bool PULSOS_DE_ARRANQUE = false;  // true = 3 pulsos de 1 s A PLENA POTENCIA

// -------------------------------------------------------------- ESTADO ----
int  potencia    = 0;       // 0-100 %
bool modoAuto    = false;
int  pasoAuto    = 0;
const int ESCALONES[] = {0, 25, 50, 75, 100};
const int N_ESCALONES = 5;

unsigned long inicioVentana = 0;
unsigned long ultimoPaso    = 0;
unsigned long ultimoLog     = 0;
bool estadoSSR = false;

int limitar(int v) {
  if (v < 0) return 0;
  if (v > POTENCIA_MAX) return POTENCIA_MAX;
  return v;
}

void aplicar(bool encendido) {
  estadoSSR = encendido;
  digitalWrite(PIN_SSR, encendido ? HIGH : LOW);
  digitalWrite(PIN_LED, encendido ? HIGH : LOW);
}

void setup() {
  pinMode(PIN_SSR, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  aplicar(false);                 // arranque seguro, siempre apagado primero

  Serial.begin(115200);
  delay(400);

  Serial.println();
  Serial.println("=== PRUEBA DE BANCO :: ESP32 + rele de estado solido ===");
  Serial.print  ("Techo de potencia: "); Serial.print(POTENCIA_MAX); Serial.println(" %");
  Serial.println();

  if (PULSOS_DE_ARRANQUE) {
    Serial.println("Autoprueba: 3 pulsos de 1 s a plena potencia...");
    for (int i = 0; i < 3; i++) {
      aplicar(true);  delay(1000);
      aplicar(false); delay(1000);
    }
    Serial.println("Si viste parpadear el rele, el cableado esta bien.");
    Serial.println();
  }

  modoAuto = ARRANCAR_EN_AUTO;
  potencia = 0;

  Serial.print("Arranque en "); Serial.print(modoAuto ? "AUTO" : "MANUAL");
  Serial.println(" y 0 %. No calienta hasta que lo pidas.");
  Serial.println("Comandos: P<0-100> | AUTO | ON | OFF");
  Serial.println("-------------------------------------------");

  inicioVentana = millis();
  ultimoPaso    = millis();
}

void loop() {
  unsigned long ahora = millis();

  atenderSerial();

  // Barrido automatico de escalones
  if (modoAuto && ahora - ultimoPaso >= PASO_AUTO_MS) {
    ultimoPaso = ahora;
    pasoAuto   = (pasoAuto + 1) % N_ESCALONES;
    potencia   = limitar(ESCALONES[pasoAuto]);
    Serial.print(">> AUTO: escalon a ");
    Serial.print(potencia);
    Serial.println(" %");
  }

  // Ancho de pulso sobre la ventana — misma logica que el firmware real
  while (ahora - inicioVentana >= VENTANA_MS) {
    inicioVentana += VENTANA_MS;
  }
  unsigned long transcurrido = ahora - inicioVentana;
  unsigned long tiempoON     = (unsigned long)((potencia / 100.0) * VENTANA_MS);

  aplicar(tiempoON > transcurrido);

  logPeriodico(ahora, transcurrido);
}

void logPeriodico(unsigned long ahora, unsigned long transcurrido) {
  if (ahora - ultimoLog < PERIODO_LOG) return;
  ultimoLog = ahora;

  // Barra de 20 casillas que representa la ventana de 2 s
  int casillas = (potencia * 20) / 100;
  Serial.print("[");
  for (int i = 0; i < 20; i++) Serial.print(i < casillas ? '#' : '.');
  Serial.print("] ");

  Serial.print(potencia);          Serial.print(" %  |  SSR ");
  Serial.print(estadoSSR ? "ON " : "OFF");
  Serial.print("  |  ventana ");
  Serial.print(transcurrido);      Serial.print("/");
  Serial.print(VENTANA_MS);        Serial.print(" ms  |  ");
  Serial.print(modoAuto ? "AUTO" : "MANUAL");
  Serial.print("  |  Vdc esperado ~");
  Serial.print(potencia * 3.3 / 100.0, 2);
  Serial.println(" V");
}

void atenderSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (cmd == "AUTO") {
    modoAuto = true;
    ultimoPaso = millis();
    Serial.print(">> Modo AUTO, con techo de "); Serial.print(POTENCIA_MAX); Serial.println(" %");
  } else if (cmd == "ON") {
    modoAuto = false; potencia = POTENCIA_MAX;
    Serial.print(">> Potencia al maximo permitido: "); Serial.print(potencia); Serial.println(" %");
  } else if (cmd == "OFF") {
    modoAuto = false; potencia = 0;
    Serial.println(">> 0 % — el rele queda fijo apagado");
  } else if (cmd.charAt(0) == 'P') {
    int v = cmd.substring(1).toInt();
    if (v < 0 || v > 100) {
      Serial.println(">> Fuera de rango. Usa P0 a P100.");
    } else {
      modoAuto = false;
      potencia = limitar(v);
      Serial.print(">> Potencia manual: "); Serial.print(potencia); Serial.println(" %");
      if (potencia < v) {
        Serial.print("   (recortado por POTENCIA_MAX = "); Serial.print(POTENCIA_MAX); Serial.println(" %)");
      }
    }
  } else {
    Serial.println(">> Comando no reconocido. Usa P<0-100>, AUTO, ON u OFF.");
  }
}

/*
 * ----------------------------------------------------------------------------
 *  SI EL RELE NO ENCIENDE NUNCA
 *
 *  1. Medí con el multimetro en VDC entre GPIO23 y GND con el comando ON.
 *     - Si lee ~3,3 V, el ESP32 hace su trabajo y el problema esta del otro lado.
 *     - Si lee ~0 V, revisá que cargaste este sketch y que el pin es el 23.
 *
 *  2. Polaridad: en el rele comercial, terminal 3 es (+) y 4 es (−). En el
 *     construido, el pin 1 del MOC3063 es el anodo y el 2 el catodo.
 *
 *  3. Con el rele construido: si el triac no engancha, revisá que la carga
 *     tenga corriente suficiente. El BTA41 pide hasta 120 mA de mantenimiento,
 *     o sea unos 14 W minimo a 120 V.
 * ----------------------------------------------------------------------------
 */
