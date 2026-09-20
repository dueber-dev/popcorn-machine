# Esquema de conexiones y lista de materiales

Documento de referencia para el armado. Diagrama visual:
[`esquema.svg`](esquema.svg).

Red asumida: **120 V / 60 Hz**.

> Antes de cablear el lado AC tenés que haber hecho la medición de la Etapa 0 de la
> [guía de armado](guia-de-armado.md) y confirmado que `R_paralelo ≥ 12 Ω`.

---

## 1. Lista de materiales

### 1.1 Se recupera de la palomitera

| # | Componente | Nota |
| :--- | :--- | :--- |
| R1 | Resistencia principal (grande) | Medir antes de reutilizar |
| R2 | Resistencia auxiliar (pequeña) | Solo si el cálculo del paralelo lo permite |
| FT1 | Fusible térmico 15 A | No reemplazar por uno de mayor corriente |
| BM1 | Termostato bimetálico | Se conserva como protección independiente |
| M1 | Motor del ventilador | Ahora alimentado en DC desde fuente propia |
| SW1 | Interruptor AC | Verificar que corte la fase |
| — | Cable de red con clavija | Reemplazar si el aislamiento está reseco |
| — | Cilindro de aluminio y soportes de mica | Conservar los aislantes originales |

**Se descarta:** la placa rectificadora del motor (puente de diodos D1–D4) y todo el
cableado que unía R2 con esa placa. Ya no cumplen función.

### 1.2 Electrónica de control

| # | Componente | Especificación | Cant. |
| :--- | :--- | :--- | ---: |
| U1 | ESP32 DevKit | v1, 30 o 38 pines | 1 |
| U2 | Módulo MAX6675 | HW-550 | 1 |
| TC1 | Sonda termopar tipo K | **Junta aislada (ungrounded)**, vaina de acero inoxidable, rango ≥ 400 °C, con rosca M6 u M8 | 1 |
| R3 | Resistencia 10 kΩ | 1/4 W, pulldown de GPIO23 | 1 |
| — | *(opcional)* MOSFET 2N7000 + resistencia 1 kΩ | Solo si el SSR no dispara bien a 3,3 V | 1 |

### 1.3 Potencia y alimentación

| # | Componente | Especificación | Cant. |
| :--- | :--- | :--- | ---: |
| SSR1 | Relé de estado sólido | SSR-25 DA — entrada 3–32 VDC, salida 24–380 VAC | 1 |
| — | Disipador para SSR | Aluminio con aletas, mínimo 50 × 50 × 30 mm | 1 |
| — | Pasta térmica | Para la interfaz SSR–disipador | 1 |
| PS1 | Fuente AC-DC aislada | 12 V, ≥ 1 A (HLK-10M12 o open-frame 12 V / 2 A) | 1 |
| PS2 | Convertidor buck | MP1584EN o LM2596 ajustable, salida fijada a 5,0 V | 1 |
| F1 | Portafusible + fusible | Vidrio 5 × 20 mm, 1 A / 250 V, para el ramal DC | 1 |

La corriente de PS1 se define en la Etapa 2, cuando midas el consumo real del motor.
Comprá al doble de esa corriente. Si el motor resulta ser de 24 V, PS1 de 24 V y el buck
igual a 5 V.

### 1.4 Cableado y conexión

| Componente | Especificación | Dónde |
| :--- | :--- | :--- |
| Cable de potencia | **AWG 14, aislamiento de silicona o fibra de vidrio, 200 °C** | Todo el ramal de calor |
| Cable de señal DC | AWG 22–24 | ESP32 ↔ MAX6675 ↔ SSR |
| Cable del motor | AWG 20 | PS1 → M1 |
| Terminales Faston hembra | Aislados, para crimpar, 6,3 mm | Resistencias, bimetálico, fusible |
| Terminales de ojillo | Para crimpar | Tornillos del SSR |
| Clemas cerámicas (porcelana) | 2 o 3 vías | Nodos N1, N2 y N3 en zona caliente |
| Termorretráctil | Varios diámetros | Solo zona fría |
| Prensaestopas / pasacables | Para el cable de red | Entrada a la carcasa |
| Pasamuros aislante | Cerámico o teflón | Entrada del termopar al cilindro |

> **El cable de PVC común no sirve en la zona caliente.** El PVC se degrada entre 70 y
> 105 °C y el interior de la máquina va a estar bastante por encima. Silicona o fibra de
> vidrio, 200 °C.

### 1.5 Herramientas

Multímetro (imprescindible), crimpadora para terminales, pinza amperimétrica (opcional,
pero es la forma más directa de confirmar que la corriente del ramal de calor es la que
calculaste).

---

## 2. Diagrama

Ver [`esquema.svg`](esquema.svg) para la versión gráfica. En texto:

```
  POTENCIA 120 V AC
  ─────────────────────────────────────────────────────────────────────────────

  [TOMA L] ──> [SW1 Interruptor] ──┬─────────────────────── riel FASE ─────────┐
                                   │                                           │
              (rung 1, ramal DC)   ├──> [F1 1 A] ──> [PS1 Fuente 12 V] ────────┤
                                   │                        │                  │
                                   │                   +12 V / GND             │
                                   │                   (va al panel DC)        │
                                   │                                           │
              (rung 2, calor)      ├──> [FT1 15 A] ──> [BM1] ──> [SSR1 1|2] ──>│
                                   │                                  │        │
                                   │                                  N2       │
                                   │                                  ├─ [R1] ─┤
                                   │                                  └─ [R2] ─┤
                                   │                                           │
  [TOMA N] ────────────────────────┴──────────────── riel NEUTRO ──────────────┘

  [TOMA PE] ──> chasis metálico


  CONTROL DC
  ─────────────────────────────────────────────────────────────────────────────

  +12 V ──┬──> [M1 Motor]
          └──> [PS2 Buck 12→5 V] ──> [U1 ESP32 VIN]

  [U1 ESP32] ──3V3/GND/GPIO18/GPIO5/GPIO19──> [U2 MAX6675] ──T+/T−──> [TC1]
             ──GND─────────────────────────> [SSR1 term. 4 (−)]
             ──GPIO23──┬──────────────────> [SSR1 term. 3 (+)]
                       └── [R3 10 kΩ] ──> GND

  GND ────────────────────────────────────── riel común DC
```

**Por qué este orden:**

- El ramal DC sale **antes** de FT1 y BM1. El ventilador sigue soplando aunque el
  bimetálico abra o el fusible térmico se funda, y puede enfriar el cilindro.
- BM1 va **antes** del SSR, para que el SSR ni reciba AC cuando el bimetálico esté
  abierto.
- El SSR va del lado de **fase**, no de neutro. Así, con el SSR abierto, las resistencias
  quedan al potencial de neutro y no energizadas.
- R1 y R2 en paralelo, ambas aguas abajo del SSR.

---

## 3. Tabla de conexiones

### 3.1 Lado AC — cable AWG 14 silicona, terminales crimpados

| # | Desde | Hasta |
| ---: | :--- | :--- |
| 1 | Clavija, conductor **L** (fase) | SW1 terminal 1 |
| 2 | SW1 terminal 2 | Nodo **N1** (clema cerámica de 3 vías) |
| 3 | N1 | F1 portafusible, terminal A |
| 4 | F1 terminal B | PS1 entrada **L** |
| 5 | N1 | FT1 fusible térmico, terminal A |
| 6 | FT1 terminal B | BM1 bimetálico, terminal A |
| 7 | BM1 terminal B | **SSR1 terminal 1** (carga AC) |
| 8 | **SSR1 terminal 2** | Nodo **N2** (clema cerámica de 3 vías) |
| 9 | N2 | R1 resistencia grande, terminal A |
| 10 | N2 | R2 resistencia pequeña, terminal A *(omitir si el cálculo no da)* |
| 11 | R1 terminal B | Nodo **N3** (clema cerámica de 3 vías) |
| 12 | R2 terminal B | N3 *(omitir junto con la 10)* |
| 13 | N3 | Clavija, conductor **N** (neutro) |
| 14 | PS1 entrada **N** | N3 |
| 15 | Clavija, conductor **PE** (tierra) | Chasis metálico, tornillo con arandela dentada |

### 3.2 Alimentación DC — cable AWG 20

| # | Desde | Hasta |
| ---: | :--- | :--- |
| 16 | PS1 salida **+V** (12 V) | Riel **+12 V** |
| 17 | PS1 salida **−V** | Riel **GND** |
| 18 | Riel +12 V | M1 motor, terminal **+** |
| 19 | Riel GND | M1 motor, terminal **−** |
| 20 | Riel +12 V | PS2 buck, **IN+** |
| 21 | Riel GND | PS2 buck, **IN−** |
| 22 | PS2 **OUT+** *(ajustado a 5,0 V antes de conectar)* | U1 ESP32, pin **`VIN`** / **`5V`** |
| 23 | PS2 **OUT−** | U1 ESP32, pin **`GND`** |

> Ajustá el buck a 5,0 V con el multímetro **antes** de conectarlo al ESP32. Sale de
> fábrica en cualquier valor.

### 3.3 Señal — cable AWG 22–24

| # | Desde | Hasta |
| ---: | :--- | :--- |
| 24 | U1 pin **`3V3`** | U2 MAX6675 **`VCC`** |
| 25 | U1 pin **`GND`** | U2 MAX6675 **`GND`** |
| 26 | U1 pin **`GPIO18`** | U2 MAX6675 **`SCK`** |
| 27 | U1 pin **`GPIO5`** | U2 MAX6675 **`CS`** |
| 28 | U1 pin **`GPIO19`** | U2 MAX6675 **`SO`** |
| 29 | U2 MAX6675 **`T+`** | TC1 conductor **positivo** (ver §4) |
| 30 | U2 MAX6675 **`T−`** | TC1 conductor **negativo** (ver §4) |
| 31 | U1 pin **`GPIO23`** | **SSR1 terminal 3 `(+)`** |
| 32 | U1 pin **`GND`** | **SSR1 terminal 4 `(−)`** |
| 33 | R3 (10 kΩ), una pata | Sobre el pin `GPIO23`, lo más cerca posible del ESP32 |
| 34 | R3, otra pata | `GND` del ESP32 |

**El `VCC` del MAX6675 va a `3V3`, nunca a `5V`.** Su pin `SO` saca lógica al nivel de
su alimentación y el GPIO19 del ESP32 no tolera 5 V.

---

## 4. Polaridad del termopar

El documento original decía "terminal roja y azul/negra". Eso es incorrecto y vale la
pena fijarlo, porque en los dos estándares vigentes **el rojo es el negativo**:

| Estándar | Positivo (`T+`) | Negativo (`T−`) |
| :--- | :--- | :--- |
| **ANSI / MC 96.1** (el común en América) | **Amarillo** (cromel) | **Rojo** (alumel) |
| **IEC 60584** | **Verde** | **Blanco** |

Si la sonda no trae colores reconocibles, la prueba definitiva es funcional: conectala,
calentá la punta con la mano o un encendedor a distancia, y mirá el monitor serie. **Si
la lectura baja en vez de subir, está invertida.**

---

## 5. Reglas de cableado

1. **Nada de soldadura de estaño en la zona caliente.** El estaño se ablanda entre 180 y
   230 °C. Terminales crimpados, clemas cerámicas o remaches.
2. **Separación física AC/DC.** Mantené al menos 10 mm entre cualquier conductor de red y
   el cableado de señal. Rutas distintas, no el mismo mazo.
3. **El termopar, trenzado y lejos del AC.** Si tiene que cruzar un cable de red, que lo
   haga a 90°, nunca en paralelo. Son microvoltios: el ruido de conmutación se le mete.
4. **Pasta térmica entre SSR y disipador**, y 2–5 mm de aire entre el disipador y
   cualquier plástico.
5. **El PETG/PLA no toca el cilindro.** Solo para la base externa de la electrónica.
6. **Descarga de tracción** en el cable de red: prensaestopas o nudo, para que un tirón
   no arranque una conexión de fase.

---

## 6. Verificaciones antes de enchufar

Con la clavija **fuera** de la pared, multímetro en continuidad:

| # | Qué medir | Resultado esperado |
| ---: | :--- | :--- |
| V1 | L contra N, interruptor **abierto** | Sin continuidad |
| V2 | L contra N, interruptor **cerrado** | Solo el consumo de PS1 (alto o abierto, el SSR está en reposo) |
| V3 | Entre N2 y N3, puenteando el SSR | Debe dar el `R_paralelo` que calculaste |
| V4 | Cualquier terminal AC contra el chasis | **Sin continuidad.** Si hay, parás |
| V5 | PE de la clavija contra el chasis | Continuidad franca, < 1 Ω |
| V6 | `GND` del ESP32 contra el chasis | **Sin continuidad** — confirma que TC1 es de junta aislada |
| V7 | `GPIO23` contra `GND`, ESP32 sin alimentar | ≈ 10 kΩ — confirma el pulldown R3 |
| V8 | Salida del buck PS2, alimentado solo desde PS1 | 5,0 V ± 0,1 V, sin el ESP32 conectado |

Recién con V1 a V8 en verde pasás a la Etapa 4 de la
[guía de armado](guia-de-armado.md).
