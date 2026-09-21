# Esquema de conexiones y lista de materiales

Documento de referencia para el armado. Diagrama visual:
[`esquema.svg`](esquema.svg).

Red asumida: **120 V / 60 Hz**.

> Antes de cablear el lado AC tenés que haber medido las dos resistencias y confirmado
> que su valor combinado en paralelo es de **12 Ω o más**. El cálculo está en la
> [guía de armado](guia-de-armado.md).

---

## 1. Lista de materiales

### 1.1 Se recupera de la palomitera

| Componente | Nota |
| :--- | :--- |
| Resistencia grande | Medir antes de reutilizar |
| Resistencia pequeña | Solo si el cálculo del paralelo lo permite |
| Fusible térmico de 15 A | No reemplazar por uno de mayor corriente |
| Termostato bimetálico | Se conserva como protección independiente |
| Motor del ventilador | Ahora alimentado en DC desde fuente propia |
| Interruptor general | Verificar que corte la fase |
| Cable de red con clavija | Reemplazar si el aislamiento está reseco |
| Cilindro de aluminio y soportes de mica | Conservar los aislantes originales |

**Se descarta:** la placa rectificadora del motor (el puente de cuatro diodos) y todo el
cableado que unía la resistencia pequeña con esa placa. Ya no cumplen función.

### 1.2 Electrónica de control

| Componente | Especificación | Cant. |
| :--- | :--- | ---: |
| ESP32 DevKit | v1, 30 o 38 pines | 1 |
| Módulo MAX6675 | También vendido como HW-550 | 1 |
| Sonda termopar tipo K | **Junta aislada (ungrounded)**, vaina de acero inoxidable, rango ≥ 400 °C, con rosca M6 u M8 | 1 |
| Resistencia de 10 kΩ | 1/4 W, pulldown del pin de disparo | 1 |
| *(opcional)* MOSFET 2N7000 + resistencia de 1 kΩ | Solo si el relé de estado sólido no dispara bien a 3,3 V | 1 |

### 1.3 Potencia y alimentación

| Componente | Especificación | Cant. |
| :--- | :--- | ---: |
| Relé de estado sólido SSR-25 DA | Entrada 3–32 VDC, salida 24–380 VAC | 1 |
| Disipador para el relé | Aluminio con aletas, mínimo 50 × 50 × 30 mm | 1 |
| Pasta térmica | Entre relé y disipador | 1 |
| Fuente AC-DC aislada de 12 V | ≥ 1 A (HLK-10M12 u open-frame de 12 V / 2 A) | 1 |
| Convertidor buck 12 → 5 V | MP1584EN o LM2596 ajustable | 1 |
| Portafusible + fusible de 1 A / 250 V | Vidrio 5 × 20 mm, para el ramal DC | 1 |
| Condensador cerámico de 100 nF | 50 V, en las terminales del motor | 1 |
| Condensador electrolítico de 470–1000 µF | 25 V, en la línea de 12 V | 1 |

**Los dos condensadores son supresión de ruido, no filtrado de alimentación.** La fuente
de 12 V y el buck ya entregan DC limpia por su cuenta. Lo que hay que apagar es el
chisporroteo de las escobillas del motor: el MAX6675 amplifica **41 µV por grado**, y ese
ruido, acoplado por el cableado, se ve como saltos en la lectura de temperatura. El
cerámico va soldado lo más cerca posible de las terminales del motor; el electrolítico en
la línea de 12 V, cerca del buck, respetando la polaridad.

La corriente de la fuente de 12 V se define cuando midas el consumo real del motor
(Etapa 2 de la guía de armado). Comprá al doble de esa corriente. Si el motor resulta ser
de 24 V, fuente de 24 V y el buck igual a 5 V.

### 1.4 Cableado y conexión

| Componente | Especificación | Dónde |
| :--- | :--- | :--- |
| Cable de potencia | **AWG 14, aislamiento de silicona o fibra de vidrio, 200 °C** | Todo el ramal de calor |
| Cable de señal DC | AWG 22–24 | ESP32 ↔ MAX6675 ↔ relé |
| Cable del motor | AWG 20 | Fuente de 12 V → motor |
| Terminales Faston hembra | Aislados, para crimpar, 6,3 mm | Resistencias, bimetálico, fusible |
| Terminales de ojillo | Para crimpar | Tornillos del relé |
| Clemas cerámicas (porcelana) | 2 o 3 vías | Los tres puntos de reparto en zona caliente |
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

  [Clavija, fase] ──> [Interruptor] ──┬──────────────── línea de FASE ─────────┐
                                      │                                        │
       (ramal de alimentación)        ├──> [Fusible 1 A] ──> [Fuente 12 V] ────┤
                                      │                           │            │
                                      │                    +12 V / tierra      │
                                      │                    (va al panel DC)    │
                                      │                                        │
       (ramal de calor)               ├──> [Fusible térmico 15 A]              │
                                      │            │                           │
                                      │            v                           │
                                      │       [Bimetálico]                     │
                                      │            │                           │
                                      │            v                           │
                                      │   [Relé de estado sólido]              │
                                      │            │                           │
                                      │   Clema de resistencias                │
                                      │            ├──> [Resistencia grande] ──┤
                                      │            └──> [Resistencia pequeña] ─┤
                                      │                                        │
  [Clavija, neutro] ──────────────────┴─────────── línea de NEUTRO ────────────┘

  [Clavija, tierra] ──> chasis metálico


  CONTROL DC
  ─────────────────────────────────────────────────────────────────────────────

  +12 V ──┬──> [Motor del ventilador]
          └──> [Buck 12 → 5 V] ──> [ESP32, pin VIN]

  [ESP32] ──3V3 / GND / GPIO18 / GPIO5 / GPIO19──> [MAX6675] ──T+ / T−──> [Termopar]
          ──GND──────────────────────────────────> [Relé, terminal 4 (−)]
          ──GPIO23──┬────────────────────────────> [Relé, terminal 3 (+)]
                    └── [Resistencia 10 kΩ] ──> tierra

  tierra ─────────────────────────────────────── línea común DC
```

**Por qué este orden:**

- El ramal de alimentación sale **antes** del fusible térmico y del bimetálico. El
  ventilador sigue soplando aunque el bimetálico abra o el fusible térmico se funda, y
  puede enfriar el cilindro.
- El bimetálico va **antes** del relé, para que el relé ni reciba AC cuando el bimetálico
  esté abierto.
- El relé va del lado de **fase**, no de neutro. Así, con el relé abierto, las
  resistencias quedan al potencial de neutro y no energizadas.
- Las dos resistencias en paralelo, ambas aguas abajo del relé.

---

## 3. Tabla de conexiones

Los tres puntos de reparto se arman con clemas cerámicas y se nombran así:

| Nombre | Dónde queda |
| :--- | :--- |
| **Clema de reparto** | Justo después del interruptor. De aquí salen los dos ramales |
| **Clema de resistencias** | Entre la salida del relé y las dos resistencias |
| **Clema de neutro** | Donde se juntan los retornos antes de ir al neutro de la clavija |

### 3.1 Lado AC — cable AWG 14 silicona, terminales crimpados

| # | Desde | Hasta |
| ---: | :--- | :--- |
| 1 | Clavija, conductor de **fase** | Interruptor, terminal 1 |
| 2 | Interruptor, terminal 2 | **Clema de reparto** |
| 3 | Clema de reparto | Portafusible de 1 A, terminal A |
| 4 | Portafusible de 1 A, terminal B | Fuente de 12 V, entrada **L** |
| 5 | Clema de reparto | Fusible térmico de 15 A, terminal A |
| 6 | Fusible térmico, terminal B | Bimetálico, terminal A |
| 7 | Bimetálico, terminal B | **Relé de estado sólido, terminal 1** (carga AC) |
| 8 | **Relé de estado sólido, terminal 2** | **Clema de resistencias** |
| 9 | Clema de resistencias | Resistencia grande, terminal A |
| 10 | Clema de resistencias | Resistencia pequeña, terminal A *(omitir si el cálculo no da)* |
| 11 | Resistencia grande, terminal B | **Clema de neutro** |
| 12 | Resistencia pequeña, terminal B | Clema de neutro *(omitir junto con la 10)* |
| 13 | Clema de neutro | Clavija, conductor de **neutro** |
| 14 | Fuente de 12 V, entrada **N** | Clema de neutro |
| 15 | Clavija, conductor de **tierra** | Chasis metálico, tornillo con arandela dentada |

### 3.2 Alimentación DC — cable AWG 20

| # | Desde | Hasta |
| ---: | :--- | :--- |
| 16 | Fuente de 12 V, salida **+V** | Línea de **+12 V** |
| 17 | Fuente de 12 V, salida **−V** | Línea de **tierra** |
| 18 | Línea de +12 V | Motor, terminal **+** |
| 19 | Línea de tierra | Motor, terminal **−** |
| 20 | Línea de +12 V | Buck, entrada **IN+** |
| 21 | Línea de tierra | Buck, entrada **IN−** |
| 22 | Buck, salida **OUT+** *(ajustada a 5,0 V antes de conectar)* | ESP32, pin **`VIN`** / **`5V`** |
| 23 | Buck, salida **OUT−** | ESP32, pin **`GND`** |
| 24 | Condensador cerámico 100 nF, una pata | Motor, terminal **+**, soldado en la terminal misma |
| 25 | Condensador cerámico 100 nF, otra pata | Motor, terminal **−** |
| 26 | Condensador electrolítico, pata **+** | Línea de +12 V, cerca del buck |
| 27 | Condensador electrolítico, pata **−** | Línea de tierra |

> Ajustá el buck a 5,0 V con el multímetro **antes** de conectarlo al ESP32. Sale de
> fábrica en cualquier valor.

### 3.3 Señal — cable AWG 22–24

| # | Desde | Hasta |
| ---: | :--- | :--- |
| 28 | ESP32, pin **`3V3`** | MAX6675, **`VCC`** |
| 29 | ESP32, pin **`GND`** | MAX6675, **`GND`** |
| 30 | ESP32, pin **`GPIO18`** | MAX6675, **`SCK`** |
| 31 | ESP32, pin **`GPIO5`** | MAX6675, **`CS`** |
| 32 | ESP32, pin **`GPIO19`** | MAX6675, **`SO`** |
| 33 | MAX6675, **`T+`** | Termopar, conductor **positivo** (ver §4) |
| 34 | MAX6675, **`T−`** | Termopar, conductor **negativo** (ver §4) |
| 35 | ESP32, pin **`GPIO23`** | **Relé de estado sólido, terminal 3 `(+)`** |
| 36 | ESP32, pin **`GND`** | **Relé de estado sólido, terminal 4 `(−)`** |
| 37 | Resistencia de 10 kΩ, una pata | Sobre el pin `GPIO23`, lo más cerca posible del ESP32 |
| 38 | Resistencia de 10 kΩ, otra pata | `GND` del ESP32 |

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
4. **Pasta térmica entre el relé y el disipador**, y 2–5 mm de aire entre el disipador y
   cualquier plástico.
5. **El PETG/PLA no toca el cilindro.** Solo para la base externa de la electrónica.
6. **Descarga de tracción** en el cable de red: prensaestopas o nudo, para que un tirón
   no arranque una conexión de fase.

---

## 6. Verificaciones antes de enchufar

Con la clavija **fuera** de la pared, multímetro en continuidad:

| Qué medir | Resultado esperado |
| :--- | :--- |
| Fase contra neutro, interruptor **abierto** | Sin continuidad |
| Fase contra neutro, interruptor **cerrado** | Solo el consumo de la fuente de 12 V (valor alto o abierto: el relé está en reposo) |
| Entre la clema de resistencias y la clema de neutro, puenteando el relé | El valor del paralelo que calculaste |
| Cualquier terminal AC contra el chasis | **Sin continuidad.** Si hay, parás |
| Tierra de la clavija contra el chasis | Continuidad franca, < 1 Ω |
| `GND` del ESP32 contra el chasis | **Sin continuidad** — confirma que el termopar es de junta aislada |
| `GPIO23` contra `GND`, ESP32 sin alimentar | ≈ 10 kΩ — confirma la resistencia de pulldown |
| Salida del buck, alimentado solo desde la fuente de 12 V | 5,0 V ± 0,1 V, sin el ESP32 conectado |

Recién con las ocho en verde pasás a la Etapa 4 de la
[guía de armado](guia-de-armado.md).

---

## 7. Por qué se descarta el puente rectificador original

La placa del motor trae cuatro diodos y **ningún condensador**. Es normal y está bien
para lo que hacía, pero hay dos razones independientes para no reutilizarla, y la
segunda es la importante.

### 7.1 Sin condensador, la salida es DC pulsante

Un puente de onda completa sin filtro entrega el valor absoluto de la senoidal: una
señal que **cae a cero 120 veces por segundo**.

A un motor de escobillas eso no le importa. Su inductancia y su inercia mecánica hacen
de filtro: la corriente no alcanza a caer a cero entre pulsos y el rotor ni se entera.
Por eso el fabricante se ahorró el condensador — no le hacía falta.

A un microcontrolador sí le importaría. El ESP32 necesita una alimentación estable;
alimentado con eso se reiniciaría 120 veces por segundo. Para usarlo habría que agregar
condensador de filtro y regulador.

### 7.2 No está aislado de la red — y esta es la razón de fondo

Las entradas AC del puente vienen una de la fase (a través de la resistencia-divisor) y
otra del neutro. Eso significa que el **negativo de ese puente está conectado a la red a
través de un diodo**, no aislado.

Si alimentaras el ESP32 desde ahí, el GND de tu microcontrolador quedaría a potencial de
red. Con dos consecuencias:

- El conector USB, la laptop conectada a él y el termopar (cuyo `T−` está referenciado al
  GND del MAX6675) quedarían todos a potencial de red.
- Tocar cualquier pin del ESP32 sería tocar los 120 V.

Ningún condensador arregla eso. Por eso el proyecto usa una **fuente conmutada aislada**:
tiene un transformador adentro que separa galvánicamente el lado de red del lado de 12 V.
Esa barrera, sumada al optoacoplador interno del relé de estado sólido, es lo que
mantiene toda la electrónica de control fuera del alcance de la red.

### 7.3 Lo que sí hace falta comprar por el motor

Nada relacionado con el puente, pero sí los **dos condensadores** de §1.3. El motor de
escobillas es una fuente de ruido eléctrico a centímetros de un amplificador que mide
**41 µV por grado**. Sin el cerámico en las terminales del motor, es probable que veas
saltos erráticos en la lectura de temperatura. Con el diseño original ese ruido no
molestaba a nadie porque no había nada midiendo microvoltios cerca.
