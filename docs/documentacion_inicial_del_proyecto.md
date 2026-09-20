# Guía de Modificación y Control de Temperatura para Horno Basado en Palomitera AC

Este documento contiene las especificaciones técnicas, el análisis del circuito original y el manual de conexiones para transformar una máquina de palomitas de aire caliente en un horno regulado con precisión mediante un microcontrolador **ESP32**, un sensor de temperatura **MAX6675 / Termopar Tipo K** y un **Relé de Estado Sólido (SSR)**.

---

## 1. Componentes del Proyecto

| Componente | Función en el Proyecto |
| :--- | :--- |
| **ESP32** | Microcontrolador principal. Procesa el algoritmo PID y controla el pulso de encendido del SSR. |
| **Módulo HW-550 (MAX6675)** | Interfaz SPI que digitaliza la señal en microvoltios del termopar y la envía al ESP32. |
| **Sonda Termopar Tipo K** | Sensor de temperatura para medir hasta $400^\circ\text{C}$ dentro del cilindro de aluminio. |
| **SSR-25 DA + Disipador** | Relé de Estado Sólido para conmutar la potencia en AC de la resistencia mediante señal DC de $3.3\text{V}$. |
| **Placa de la Palomitera** | Cilindro de aluminio, resistencias de calor, motor de aire, termostato bimetálico y fusible térmico ($15\text{ A}$). |

---

## 2. Análisis del Circuito Original de la Palomitera

La palomitera comercial funciona bajo el siguiente esquema de distribución eléctrica:

1. **Protección Térmica (Bimetálico):** Un termostato bimetálico está adosado mecánicamente al cilindro de aluminio como protección general de sobrecalentamiento.
2. **División de Resistencias:**
   * **Resistencia Principal (Grande):** Produce la mayor cantidad de calor para elevar la temperatura.
   * **Resistencia Auxiliar (Pequeña):** Actúa como un divisor de tensión (caída de voltaje) para alimentar el motor sin necesidad de un transformador pesado.
3. **Placa Rectificadora del Motor:** Una placa PCB pequeña montada en la parte trasera del motor contiene un puente rectificador de 4 diodos (D1 a D4). Recibe la tensión reducida en AC desde la resistencia pequeña y la convierte a DC para alimentar el motor del ventilador.
4. **Fusible de Seguridad ($15\text{ A}$):** Ubicado en la placa de cartón micarta en serie con la salida hacia la red AC.

---

## 3. Esquema de Conexiones (Cableado)

Para evitar que el motor se apague cuando la resistencia principal se regule, el circuito debe separarse estratégicamente.

```
       [ Línea AC (Fase) ]
                │
        [ Interruptor AC ]
                │
     [ Termostato Bimetálico ]
                │
                ├───> [ Resistencia Pequeña ] ──> [ Puente Diodos ] ──> [ Motor DC ]
                │                                       │
                │                                [ Retorno AC / Neutro ]
                │
                └───> [ Resistencia Grande ]
                            │
                      [ SSR Terminal 1 ]
                      [ SSR Terminal 2 ]
                            │
                   [ Fusible Térmico 15A ]
                            │
               [ Línea AC (Neutro) ]
```

### A. Conexión de Control de Bajo Voltaje (DC)

* **HW-550 (MAX6675) a ESP32:**
  * `VCC` $\rightarrow$ Pin $3.3\text{V}$ o $5\text{V}$ del ESP32.
  * `GND` $\rightarrow$ Pin `GND` del ESP32.
  * `SCK` $\rightarrow$ Pin `GPIO 18` del ESP32.
  * `CS`  $\rightarrow$ Pin `GPIO 5` del ESP32.
  * `SO`  $\rightarrow$ Pin `GPIO 19` del ESP32.
  * `T+` / `T-` $\rightarrow$ Conectados a la terminal roja y azul/negra de la sonda termopar Tipo K respectivamente.

* **ESP32 a SSR-25 DA:**
  * `GPIO 23` del ESP32 $\rightarrow$ Entrada **`(+)` (Terminal 3)** del SSR.
  * `GND` del ESP32 $\rightarrow$ Entrada **`(-)` (Terminal 4)** del SSR.

### B. Conexión de Potencia de Alto Voltaje (AC)

* **Relé de Estado Sólido (SSR):**
  * **Terminal 1:** Conectado a la salida de la **Resistencia Principal (Grande)**.
  * **Terminal 2:** Conectado a la entrada del **Fusible Térmico de $15\text{ A}$** (que retorna a la línea Neutra de AC).

> ⚠️ **Nota Importante:** El circuito del motor y la resistencia pequeña se mantienen conectados directamente al bimetálico de entrada para recibir corriente continua fija mientras el interruptor esté encendido.

---

## 4. Ensamblaje Mecánico y Consideraciones de Temperatura

1. **Prohibición de Soldadura de Estaño en la Zona Caliente:**
   * La soldadura de estaño se funde/ablanda entre $180^\circ\text{C}$ y $230^\circ\text{C}$.
   * En la zona interna del plato de resistencias y fusible, **NO se debe usar estaño**.
   * Usar únicamente **remaches metálicos, clemas cerámicas, terminales de grimpado/crimpado (Faston/ojillo) o conectores a presión de latón/cobre**.
2. **Ubicación de Sensores:**
   * **Placa azul MAX6675:** Debe montarse **fuera** de la zona de calor, junto a la electrónica del ESP32.
   * **Punta del Termopar:** Insertar mediante un agujero con tuerca en el lateral del cilindro de aluminio para medir el ambiente del horno sin tocar directamente el alambre al rojo vivo de las resistencias.
3. **Carcasa Impresa en 3D (PETG / PLA / ABS):**
   * **PLA / PETG:** No deben tocar el cilindro metálico directo ($T_g$ de PETG es $\sim 75\text{--}80^\circ\text{C}$). Usar estos materiales únicamente para la base externa de la electrónica.
   * **Zona del Núcleo:** Conservar los soportes y aislamientos plásticos o de mica originales, o usar separadores de teflón/silicona de alta temperatura.
   * **Disipador del SSR:** Montar dejando una luz/separación de $2\text{--}5\text{ mm}$ del plástico mediante rejillas de ventilación.

---

## 5. Código de Control PID para ESP32 (Arduino IDE)

Para ejecutar este control, instala las librerías `MAX6675` y `PID_v1` en el entorno Arduino IDE:

```cpp
#include <max6675.h>
#include <PID_v1.h>

// Pines para módulo MAX6675
const int pinSO  = 19;
const int pinCS  = 5;
const int pinSCK = 18;

// Pin para activación del SSR
const int pinSSR = 23;

MAX6675 thermocouple(pinSCK, pinCS, pinSO);

// Variables de Control PID
double tempActual = 0.0;
double salidaPID  = 0.0;
double tempDeseada = 200.0; // Ajustar temperatura objetivo en °C

// Sintonización inicial PID (Kp, Ki, Kd)
double Kp = 2.0, Ki = 0.5, Kd = 1.0;
PID myPID(&tempActual, &salidaPID, &tempDeseada, Kp, Ki, Kd, DIRECT);

// Ventana proporcional de tiempo para SSR (2 segundos)
unsigned long tamañoVentana = 2000; 
unsigned long inicioVentana;

void setup() {
  Serial.begin(115200);
  pinMode(pinSSR, OUTPUT);
  digitalWrite(pinSSR, LOW);

  inicioVentana = millis();
  
  myPID.SetOutputLimits(0, tamañoVentana);
  myPID.SetMode(AUTOMATIC);
}

void loop() {
  // Lectura del termopar cada 250 ms
  static unsigned long ultimaLectura = 0;
  if (millis() - ultimaLectura >= 250) {
    ultimaLectura = millis();
    tempActual = thermocouple.readCelsius();
    
    Serial.print("Temp Actual: ");
    Serial.print(tempActual);
    Serial.print(" °C | Setpoint: ");
    Serial.print(tempDeseada);
    Serial.print(" °C | SSR Tiempo ON (ms): ");
    Serial.println(salidaPID);
  }

  // Cálculo de algoritmo PID
  myPID.Compute();

  // Control por ancho de pulso en ventana de tiempo para el SSR
  unsigned long ahora = millis();
  if (ahora - inicioVentana >= tamañoVentana) {
    inicioVentana += tamañoVentana;
  }

  if (salidaPID > (ahora - inicioVentana)) {
    digitalWrite(pinSSR, HIGH); // Encender resistencia
  } else {
    digitalWrite(pinSSR, LOW);  // Apagar resistencia
  }
}
```