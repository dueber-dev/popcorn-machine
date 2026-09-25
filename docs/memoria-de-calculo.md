# Memoria de cálculo

Justificación numérica de cada valor del proyecto. Cada apartado sigue la misma
estructura: qué restringe el valor, la fórmula, la sustitución y el veredicto.

Datos de partida comunes:

| Magnitud | Valor |
| :--- | :--- |
| Tensión de red | 120 V eficaces, 60 Hz |
| Tensión de pico | `120 × √2 = 169,7 ≈ 170 V` |
| Semiperíodo | `1 / (2 × 60) = 8,33 ms` |
| Tensión del GPIO del ESP32 | 3,3 V |
| Carga (resistencias en paralelo) | ~1000 W, ~8,3 A |

---

## 1. Lado de control

### 1.1 Resistencia de entrada del optoacoplador — 330 Ω

**Qué la restringe:** tiene que entregar al LED del MOC3063 al menos la corriente de
disparo garantizada, sin exceder lo que el GPIO puede dar.

Datos del MOC3063:

| Parámetro | Valor |
| :--- | :--- |
| Tensión directa del LED, `V_F` | 1,25 V típico / 1,5 V máximo |
| Corriente de disparo, `I_FT` | 5 mA máximo |

```
R = (V_GPIO − V_F) / I
```

Con R = 330 Ω y `V_F` típica:

```
I = (3,3 − 1,25) / 330 = 6,2 mA
```

Caso peor, con `V_F` máxima:

```
I = (3,3 − 1,5) / 330 = 5,5 mA
```

**Veredicto:** 5,5 mA > 5 mA incluso en el peor caso. ✓

**Verificación de disipación:**

```
P = I² × R = (6,2 mA)² × 330 = 12,7 mW
```

Una resistencia de 1/4 W trabaja al 5 % de su capacidad. ✓

**Verificación del GPIO:** el ESP32 admite 40 mA absolutos por pin y se recomienda no
pasar de 20 mA. Con 6,2 mA estamos al 31 % del valor recomendado. ✓

**Alternativa de 220 Ω:** `I = (3,3 − 1,25)/220 = 9,3 mA`. También válida, con más
margen sobre el disparo y todavía holgada para el GPIO.

---

### 1.2 Resistencia de pulldown — 10 kΩ

**Qué la restringe:** tiene que ser lo bastante baja para imponer el nivel frente a
corrientes de fuga y ruido, y lo bastante alta para no desperdiciar corriente ni cargar
al GPIO cuando está en alto.

Corriente desperdiciada con el pin en alto:

```
I = 3,3 / 10 000 = 0,33 mA
P = 3,3 × 0,33 mA = 1,1 mW
```

| Valor | Consecuencia |
| :--- | :--- |
| 1 kΩ | Desperdicia 3,3 mA y carga el pin sin necesidad |
| **10 kΩ** | **0,33 mA, despreciable, y nivel firme** |
| 100 kΩ | Demasiado débil frente a fugas y acoplamiento capacitivo |

10 kΩ es el compromiso estándar en el rango de 1 a 100 kΩ.

---

## 2. Relé de estado sólido construido

### 2.1 Resistencia de puerta — 220 Ω

Es el cálculo central del módulo, y tiene **dos restricciones que empujan en sentidos
opuestos**.

#### Restricción superior: no destruir el optoacoplador

Cuando el fototriac del MOC3063 conduce, lo único que limita su corriente es esta
resistencia contra la tensión instantánea de red. En el peor caso, el pico:

| Parámetro del MOC3063 | Valor |
| :--- | :--- |
| Corriente de pico no repetitiva, `I_TSM` | 1 A |

```
R_mínima = V_pico / I_TSM = 170 / 1 = 170 Ω
```

Por eso **nunca por debajo de 180 Ω**, que es el valor comercial inmediato superior.

Comprobación para cada candidato:

| R | `I_pico = 170 / R` | % del límite | Veredicto |
| ---: | ---: | ---: | :--- |
| 100 Ω | 1,70 A | 170 % | **Destruye el optoacoplador** |
| 180 Ω | 0,94 A | 94 % | Válido, sin margen |
| **220 Ω** | **0,77 A** | **77 %** | **Elegido** |
| 470 Ω | 0,36 A | 36 % | Ver restricción inferior |

#### Restricción inferior: disparar el triac

La corriente disponible para la puerta tiene que superar la corriente de disparo del
BTA41 en el instante en que el optoacoplador conduce.

El MOC3063 inhibe el disparo mientras la tensión instantánea supera su **tensión de
inhibición** `V_IH = 20 V máximo`. O sea que el peor caso es disparar con 20 V
instantáneos en la línea:

```
I_puerta = (V_instantánea − V_TM_opto − V_GT_triac) / R
         = (20 − 3 − 1,3) / 220
         = 71 mA
```

| Parámetro del BTA41-600B | Valor |
| :--- | :--- |
| Corriente de disparo de puerta, `I_GT` | 50 mA máximo (cuadrantes I, II, III) |

**Veredicto:** 71 mA > 50 mA en el peor caso. ✓ Y conforme la senoidal sigue subiendo,
la corriente disponible crece hasta `(170 − 4,3)/220 = 753 mA`, aunque el triac engancha
mucho antes.

**Conclusión:** 220 Ω satisface ambas restricciones con margen por los dos lados. Es el
valor óptimo, no un apaño por disponibilidad.

#### Por qué 1 W y no 1/4 W

En operación normal la resistencia conduce solo desde el disparo hasta que el triac
engancha: microsegundos por semiciclo. La potencia media es inferior a 0,1 W.

El dimensionado responde al **caso de falla**: si el triac no engancha, el optoacoplador
conduciría el semiciclo completo y la resistencia vería

```
P = V_ef² / R = 120² / 220 = 65 W
```

Una de 1 W sobrevive ese transitorio el tiempo suficiente para que actúe la protección;
una de 1/4 W se abre de inmediato. Es margen ante falla, no disipación de régimen.

---

### 2.2 Snubber — 100 Ω y 100 nF

**Qué resuelve:** limitar la velocidad de subida de tensión (`dV/dt`) entre A1 y A2, para
que el triac no se dispare solo por acoplamiento capacitivo interno.

**Constante de tiempo:**

```
τ = R × C = 100 Ω × 100 nF = 10 µs
```

#### Corriente de fuga con el triac apagado

El condensador conduce permanentemente, aunque el triac esté abierto:

```
X_C = 1 / (2π f C) = 1 / (2π × 60 × 100·10⁻⁹) = 26,5 kΩ
I_fuga = 120 / 26 500 = 4,5 mA
```

**Consecuencia práctica:** la carga recibe 4,5 mA incluso "apagada". Sobre una resistencia
de 14,4 Ω eso son 0,065 V — irrelevante. Pero con una lámpara LED o un neón como carga,
se vería un brillo tenue. Es el motivo por el que las pruebas se hacen con incandescente.

#### Disipación de la resistencia del snubber

En régimen, con el triac apagado:

```
P = I² × R = (4,5 mA)² × 100 = 2 mW
```

Al dispararse el triac, el condensador —cargado hasta 170 V— se descarga a través de los
100 Ω:

```
I_pico = 170 / 100 = 1,7 A
E = ½ C V² = 0,5 × 100·10⁻⁹ × 170² = 1,45 mJ por descarga
```

A 120 descargas por segundo (dos por ciclo, 60 Hz):

```
P_media = 1,45 mJ × 120 = 0,17 W
```

**Veredicto:** la media es 0,17 W, pero los pulsos son de 1,7 A. Por eso 2 W y
preferentemente de carbón o película, **no bobinada** — una resistencia bobinada aporta
inductancia justo donde estamos tratando de amortiguar.

#### Por qué el condensador debe ser clase X2

Está conectado permanentemente entre línea y neutro. Un condensador común que falle en
cortocircuito pone un corto franco sobre la red. Los X2 están construidos para fallar en
**circuito abierto** y están ensayados para picos de 2,5 kV. No es una preferencia: es la
clasificación de seguridad que corresponde a esa posición en el circuito.

---

### 2.3 Varistor — 130 a 150 V eficaces

**Restricción inferior:** no debe conducir en operación normal.

```
V_varistor (a 1 mA) ≈ 200 V  para un MOV de 130 VAC
V_pico normal = 170 V
```

200 V > 170 V ✓ — no conduce durante la senoidal normal.

**Restricción superior:** su tensión de recorte debe quedar por debajo de la tensión que
soporta el triac.

```
V_recorte ≈ 340 V  (típico a 50 A de descarga)
V_DRM del BTA41-600B = 600 V
```

340 V < 600 V ✓ — el triac queda protegido.

---

### 2.4 Elección del triac — BTA41-600B

| Criterio | Exigencia | BTA41-600B | Margen |
| :--- | ---: | ---: | ---: |
| Corriente eficaz | 8,3 A | 40 A | **4,8×** |
| Tensión de pico repetitiva | 170 V | 600 V | **3,5×** |

Ambos márgenes superan el criterio habitual de diseño (2× en corriente, 2× en tensión).

**Carga mínima, por corriente de mantenimiento:**

```
I_H del BTA41 ≈ 100 mA  (corriente de enganche I_L hasta 120 mA)
P_mínima = I_L × V = 0,12 × 120 = 14,4 W
```

Por debajo de ~14 W el triac se dispara en cada cruce por cero y se suelta enseguida. Por
eso las pruebas con lámpara exigen **40 W como mínimo**: 14 W es el límite teórico, 40 W
da margen.

**Por qué BTA y no BTB:** en el BTB el tab metálico está unido internamente a A2, es decir
a la red. En el BTA el tab está aislado (2500 V eficaces de rigidez). Con BTB el disipador
quedaría energizado.

---

### 2.5 Disipación del triac y dimensionado del disipador

El triac no se comporta como resistencia sino como juntura: la caída es casi independiente
de la corriente.

```
V_T ≈ 1,2 V a 10 A
P = V_T × I = 1,2 × 10 = 12 W
```

**Resistencia térmica total admisible:**

```
θ_total = (T_j,máx − T_ambiente) / P = (125 − 40) / 12 = 7,08 °C/W
```

**Descontando las etapas fijas:**

| Etapa | Valor |
| :--- | ---: |
| Juntura a cápsula, `θ_JC` (tab aislado) | 1,1 °C/W |
| Cápsula a disipador con pasta, `θ_CS` | 0,5 °C/W |
| **Disponible para el disipador, `θ_SA`** | **5,5 °C/W** |

**Se especifica 3 °C/W**, no 5,5, porque la temperatura ambiente dentro de la máquina será
bastante superior a 40 °C. Rehaciendo el cálculo con 60 °C de ambiente:

```
θ_total = (125 − 60) / 12 = 5,4 °C/W  →  θ_SA = 3,8 °C/W
```

3 °C/W cubre ese escenario. ✓

**Nota sobre el control por ancho de pulso:** con un ciclo de trabajo `D`, la disipación
media es `12 × D` vatios. Al 30 % son 3,6 W. El disipador se dimensiona para el peor caso
(100 %), pero en régimen trabajará muy por debajo.

---

## 3. Lado de red de la máquina

### 3.1 Resistencias en paralelo

```
R_combinada = (R_grande × R_pequeña) / (R_grande + R_pequeña)
I = 120 / R_combinada
P = 120² / R_combinada
```

**Restricción:** el fusible es de 15 A, y se busca margen.

| R combinada | Corriente | Potencia | Veredicto |
| ---: | ---: | ---: | :--- |
| 20 Ω | 6,0 A | 720 W | Holgado |
| 14 Ω | 8,6 A | 1029 W | Caso típico |
| **12 Ω** | **10,0 A** | **1200 W** | **Límite de diseño** |
| 8 Ω | 15,0 A | 1800 W | Funde el fusible |
| 6 Ω | 20,0 A | 2400 W | Destruye el triac |

El límite de 12 Ω deja la corriente en 10 A, un 33 % por debajo del fusible de 15 A.

---

### 3.2 Ancho de pista de PCB — norma IPC-2221

Fórmula para conductor externo:

```
I = k × ΔT^0,44 × A^0,725       con k = 0,048 para capa externa
```

Despejando el área:

```
A = ( I / (k × ΔT^0,44) )^(1/0,725)
```

Para 10 A con 10 °C de aumento:

```
k × ΔT^0,44 = 0,048 × 10^0,44 = 0,048 × 2,754 = 0,1322
A = (10 / 0,1322)^1,379 = 75,6^1,379 = 391 mils²
```

Convirtiendo a ancho, con espesor de 1 oz = 1,37 mils:

```
ancho = 391 / 1,37 = 285 mils = 7,2 mm
```

| Cobre | ΔT = 10 °C | ΔT = 20 °C |
| :--- | ---: | ---: |
| 1 oz (35 µm) | **7,2 mm** | 4,7 mm |
| 2 oz (70 µm) | 3,6 mm | 2,4 mm |

De aquí sale la decisión de diseño de **no hacer pasar los 10 A por la placa**, sino llevar
los cables de potencia directo a los terminales del triac.

---

### 3.3 Distancias de aislamiento

| Entre | Mínimo | Adoptado | Fundamento |
| :--- | ---: | ---: | :--- |
| Red ↔ control | 6,4 mm | **8 mm** | Aislamiento reforzado red-SELV |
| Pistas de red entre sí | 1,5 mm | **3 mm** | IPC-2221 para 150 V, con margen |

La ranura fresada bajo el optoacoplador es necesaria porque el encapsulado DIP-6 mide
solo ~7,6 mm entre filas de pines: sin la ranura, la distancia de fuga por la superficie
de la placa sería inferior a la exigida.

---

## 4. Control por firmware

### 4.1 Ventana de tiempo — 2 segundos

**Restricción inferior: resolución.** El relé dispara en cruce por cero, así que el
incremento mínimo de energía es un semiciclo:

```
cuanto mínimo = 8,33 ms
resolución = 8,33 / 2000 = 0,42 %   →  240 niveles discretos
```

**Restricción superior: la constante térmica.** La ventana tiene que ser mucho más rápida
que el sistema que controla, para que la masa de aluminio integre los pulsos y "vea" el
promedio.

```
constante térmica del cilindro ≈ decenas de segundos
ventana / constante ≈ 2 / 40 = 5 %
```

| Ventana | Resolución | Problema |
| :--- | ---: | :--- |
| 200 ms | 24 niveles (4 %) | Control grueso |
| **2 s** | **240 niveles (0,42 %)** | **Elegida** |
| 10 s | 1200 niveles | La carga ve escalones, no un promedio |

---

### 4.2 Período de lectura del termopar — 300 ms

**Restricción:** el tiempo de conversión del MAX6675.

| Parámetro | Valor |
| :--- | :--- |
| Tiempo de conversión | 0,17 s típico / 0,22 s máximo |

Leer más rápido devuelve el dato anterior. Con 300 ms hay un 36 % de margen sobre el
máximo. ✓

**Resolución del convertidor:**

```
12 bits sobre 0–1024 °C  →  1024 / 4096 = 0,25 °C por cuenta
```

**Sensibilidad del termopar tipo K:** ≈ 41 µV/°C. Esa cifra es la que justifica los
condensadores de supresión en el motor: el ruido de escobillas es del mismo orden que la
señal útil.

---

### 4.3 Tiempo de muestreo del PID — 1000 ms

Dos restricciones:

1. **No puede ser menor que el período del sensor** (300 ms), o el PID integraría
   repetidamente el mismo dato.
2. **Regla práctica para procesos térmicos:** `T_s ≈ τ/10` a `τ/20`. Con τ ≈ 40 s da entre
   2 y 4 s; 1 s queda del lado conservador y sigue siendo cómodo.

El defecto de la librería PID_v1 es 100 ms, que violaría la primera restricción. De ahí la
llamada explícita a `SetSampleTime(1000)`.

---

### 4.4 Ganancias del PID

La salida se expresa en **porcentaje de potencia (0–100)**, no en milisegundos, para que
las ganancias no dependan del tamaño de la ventana.

```
Kp = 100 / banda_proporcional
```

| Ganancia | Valor | Significado físico |
| :--- | ---: | :--- |
| `Kp = 4,0` | — | Banda proporcional de **25 °C**: a 25 grados del objetivo pide el 100 % |
| `Ki = 0,05` | `Ti = Kp/Ki = 80 s` | Tiempo integral |
| `Kd = 20,0` | `Td = Kd/Kp = 5 s` | Tiempo derivativo |

**Sintonización por ciclo límite (Ziegler-Nichols sin sobreimpulso):**

```
Kp = 0,2 × Ku
Ki = Kp / (0,5 × Tu)
Kd = Kp × (Tu / 3)
```

donde `Ku` es la ganancia a la que aparece oscilación sostenida y `Tu` su período.

**Por qué la variante "sin sobreimpulso" y no la clásica:** en un horno, pasarse de
temperatura arruina el producto y no se puede "deshacer" — el sistema solo puede calentar,
el enfriamiento depende del ventilador. Se prefiere llegar lento a llegar rápido y
rebotar.

---

### 4.5 Umbrales de seguridad

| Umbral | Valor | Fundamento |
| :--- | ---: | :--- |
| Corte por sobretemperatura | 260–270 °C | Por encima de cualquier consigna útil, por debajo del punto de reblandecimiento del estaño (180–230 °C) que impone el límite constructivo |
| Lecturas inválidas consecutivas | 5 | A 300 ms cada una, son 1,5 s de reacción. Suficientemente rápido, y tolera un fallo aislado de comunicación |
| Ventana de fuga térmica | 90 s sin subir 5 °C al 100 % | Un sistema sano al 100 % sube decenas de °C/min; 5 °C en 90 s es inequívocamente anómalo |

---

## 5. Alimentación

### 5.1 Fuente de 12 V

```
I_fuente ≥ 2 × I_motor_medida
```

El factor 2 cubre la corriente de arranque del motor, que es varias veces la nominal
durante unos milisegundos, y evita que la fuente trabaje al límite de forma permanente.

### 5.2 Fusible del ramal DC — 1 A

El consumo del ramal es inferior a 1 A. El fusible de 15 A de la máquina jamás actuaría
ante una falla en este ramal: 15 A sobre un circuito que consume 0,5 A no es protección.
De ahí el fusible propio.

### 5.3 Condensadores de supresión del motor

| Componente | Valor | Función |
| :--- | :--- | :--- |
| Cerámico | 100 nF | Cortocircuita a masa el ruido de conmutación de escobillas, que es de alta frecuencia |
| Electrolítico | 470–1000 µF | Sostiene la línea de 12 V frente a los picos de corriente del motor |

El fundamento es la sensibilidad del termopar: **41 µV/°C**. Un ruido de pocos
milivoltios acoplado al cableado se traduce en decenas de grados de error aparente.
