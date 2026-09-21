/*
 * ============================================================================
 *  PRUEBA DE BANCO — ESP32 + relé de estado sólido SSR-25 DA
 *  SIN termopar, SIN resistencias, SIN nada conectado a la red.
 * ----------------------------------------------------------------------------
 *  Qué hace:
 *    Reproduce exactamente el mecanismo de control del firmware real —ancho de
 *    pulso sobre una ventana de 2 segundos— pero con la potencia puesta a mano
 *    en vez de calculada por el PID. Sirve para verificar tres cosas:
 *
 *      1. Que el cableado GPIO23 / GND al relé está bien.
 *      2. Que tu ejemplar de SSR-25 DA dispara con los 3,3 V del ESP32.
 *         (Su entrada está especificada 3-32 VDC; 3,3 V es el borde inferior,
 *         y hay ejemplares que piden 4 V. Esta prueba te lo dice.)
 *      3. Que la lógica de la ventana de tiempo hace lo que debe.
 *
 *  CONEXIONES (solo tres cables):
 *    ESP32 GPIO23  →  relé, terminal 3 (+)
 *    ESP32 GND     →  relé, terminal 4 (−)
 *    ESP32 USB     →  computadora
 *
 *    Terminales 1 y 2 del relé: VACÍOS. Nada de red. Nada de resistencias.
 *
 *  CÓMO VERIFICAR:
 *    a) El LED indicador del relé sigue el ciclo (si tu ejemplar trae LED).
 *    b) El LED de la placa ESP32 (GPIO2) hace lo mismo, en espejo.
 *    c) Multímetro en VOLTIOS DC entre GPIO23 y GND: como el multímetro
 *       promedia, deberías leer aproximadamente potencia% × 3,3 V.
 *         100 %  →  ~3,3 V       50 %  →  ~1,65 V
 *          75 %  →  ~2,5 V       25 %  →  ~0,83 V
 *           0 %  →  ~0 V
 *       Esta es la verificación más sólida: mide el ciclo de trabajo real.
 *
 *  COMANDOS (monitor serie a 115200 baudios):
 *    P50    fija la potencia en 50 %   (P0 a P100)
 *    AUTO   barrido automático: 0, 25, 50, 75, 100 %, 8 s cada escalón
 *    ON     100 %
 *    OFF    0 %
 * ============================================================================
 */

const int PIN_SSR = 23;   // al terminal 3 (+) del relé
const int PIN_LED = 2;    // LED de la placa ESP32 DevKit v1

const unsigned long VENTANA_MS   = 2000;  // misma ventana que el firmware real
const unsigned long PASO_AUTO_MS = 8000;  // duración de cada escalón en AUTO
const unsigned long PERIODO_LOG  = 500;

int  potencia    = 0;       // 0-100 %
bool modoAuto    = true;
int  pasoAuto    = 0;
const int ESCALONES[] = {0, 25, 50, 75, 100};
const int N_ESCALONES = 5;

unsigned long inicioVentana = 0;
unsigned long ultimoPaso    = 0;
unsigned long ultimoLog     = 0;
bool estadoSSR = false;

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
  Serial.println("=== PRUEBA DE BANCO :: ESP32 + SSR-25 DA ===");
  Serial.println("Terminales 1 y 2 del rele deben estar VACIOS.");
  Serial.println();

  // Tres pulsos lentos de un segundo: confirmacion visual de cableado.
  Serial.println("Autoprueba: 3 pulsos de 1 s...");
  for (int i = 0; i < 3; i++) {
    aplicar(true);  delay(1000);
    aplicar(false); delay(1000);
  }
  Serial.println("Si viste parpadear el LED del rele, el cableado esta bien.");
  Serial.println("Si no parpadeo, ver la nota sobre los 3,3 V al final.");
  Serial.println();
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
    potencia   = ESCALONES[pasoAuto];
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
    Serial.println(">> Modo AUTO: barrido 0-25-50-75-100 %");
  } else if (cmd == "ON") {
    modoAuto = false; potencia = 100;
    Serial.println(">> 100 % — el rele deberia quedar fijo encendido");
  } else if (cmd == "OFF") {
    modoAuto = false; potencia = 0;
    Serial.println(">> 0 % — el rele deberia quedar fijo apagado");
  } else if (cmd.charAt(0) == 'P') {
    int v = cmd.substring(1).toInt();
    if (v < 0 || v > 100) {
      Serial.println(">> Fuera de rango. Usa P0 a P100.");
    } else {
      modoAuto = false; potencia = v;
      Serial.print(">> Potencia manual: "); Serial.print(potencia); Serial.println(" %");
    }
  } else {
    Serial.println(">> Comando no reconocido. Usa P<0-100>, AUTO, ON u OFF.");
  }
}

/*
 * ----------------------------------------------------------------------------
 *  SI EL RELE NO ENCIENDE NUNCA
 *
 *  Antes de sospechar del codigo, descartá lo facil:
 *
 *  1. Medí con el multimetro en VDC entre GPIO23 y GND con el comando ON.
 *     - Si lee ~3,3 V, el ESP32 esta haciendo su trabajo y el problema es el
 *       margen de disparo del rele.
 *     - Si lee ~0 V, revisá que cargaste este sketch y que el pin es el 23.
 *
 *  2. Polaridad: el terminal 3 es (+) y el 4 es (−). Invertidos no enciende.
 *
 *  3. Margen de 3,3 V. Mirá la etiqueta del rele. Si dice "4-32 VDC" en vez de
 *     "3-32 VDC", tu ejemplar no dispara con el ESP32 directo. Solucion:
 *     un MOSFET 2N7000 como intermediario, con la entrada del rele a 5 V.
 *
 *       GPIO23 ──[1 kΩ]── gate del 2N7000
 *       source del 2N7000 ── GND
 *       drain  del 2N7000 ── terminal 4 (−) del rele
 *       terminal 3 (+) del rele ── 5 V del ESP32
 *       (y dejá la resistencia de 10 kΩ entre gate y GND)
 *
 *     Es mejor descubrir esto ahora, en el banco, que con la resistencia de
 *     1000 W ya cableada.
 * ----------------------------------------------------------------------------
 */
