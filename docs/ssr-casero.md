# Relé de estado sólido construido en PCB

Sustituto del SSR-25 DA comercial, para cumplir con el requisito de construir el módulo.
Reproduce lo mismo que hace el comercial: optoacoplador con detección de cruce por cero
disparando un triac de potencia.

Diagrama: [`esquema-ssr-casero.svg`](esquema-ssr-casero.svg).

Red asumida: **120 V / 60 Hz**, carga resistiva de ~10 A.

---

## 1. Por qué la protoboard no sirve

Dos razones independientes, y cada una alcanza por sí sola.

### 1.1 La corriente

Los contactos de una protoboard son láminas de bronce fosforado con un área de contacto
diminuta. El fabricante los especifica típicamente en **1 A**, algunos en 2 A.

Con una resistencia de contacto realista de 30 mΩ y 10 A circulando:

```
P = I² × R = 10² × 0,03 = 3 W
```

Tres vatios disipados en un contacto del tamaño de un grano de arroz, encapsulado en ABS
que reblandece alrededor de los 100 °C. No es que se vaya a calentar: **se derrite**, y
antes de derretirse aumenta su resistencia, lo que aumenta la disipación, que aumenta la
temperatura. Es realimentación positiva.

Estás **10 veces por encima** del valor nominal. No es un caso límite.

### 1.2 La tensión

Aunque circularan 100 mA, seguiría sin ir en protoboard. La separación entre filas es de
2,54 mm, el plástico de las protoboards baratas no tiene clasificación de retardo a la
llama, y los contactos quedan expuestos al dedo. Son 120 V que pueden matar.

### 1.3 Qué sí va en protoboard

El **lado de control**: la resistencia de entrada y los cables al ESP32. Baja tensión y
miliamperios.

Ojo con una trampa: **el optoacoplador no es "el lado de control"**. Sus pines 4 y 6
están a potencial de red. Si el circuito está conectado a 120 V, el MOC3063 tampoco puede
estar en la protoboard.

La excepción es la prueba a 24 V de la Etapa C: ahí **todo** puede ir en protoboard,
porque no hay red en ninguna parte.

Con 120 V, el lado de potencia va en placa soldada —perforada alcanza para probar— o
cableado directo a los terminales del triac. Nunca en protoboard.

---

## 2. Cómo funciona el circuito

El diagrama del SSR comercial que encontraste tiene cuatro bloques. Los reproducimos uno
por uno:

| Bloque del comercial | Nuestro equivalente |
| :--- | :--- |
| `INPUT CIRCUIT` + LED | Resistencia de 330 Ω + el LED interno del optoacoplador |
| Fototriac + `ZERO CROSS CIRCUIT` | **MOC3063**, que trae el detector de cruce por cero integrado |
| Triac de potencia | **BTA41-600B** sobre disipador |
| Red RC en paralelo | Snubber de 100 Ω + 100 nF X2 |

La secuencia: el GPIO23 enciende el LED del optoacoplador. El fototriac que está del otro
lado conduce, **pero solo cuando la senoidal pasa por cero** — eso lo garantiza el
circuito de cruce por cero interno del MOC3063. Ese pulso llega a la puerta del triac
grande, que se engancha y conduce el resto del semiciclo. Al siguiente cruce por cero el
triac se apaga solo, y si el LED sigue encendido vuelve a dispararse.

**No hay conexión eléctrica entre el ESP32 y los 120 V.** La única unión es óptica: luz
atravesando el encapsulado del MOC3063. Eso es lo que hace segura toda la arquitectura, y
es el punto que no se puede comprometer al diseñar la placa.

Como el disparo es en cruce por cero igual que el comercial, **el firmware no cambia
nada**. La ventana de 2 segundos funciona idéntico.

---

## 3. Lista de materiales

### 3.1 Componentes activos

| Componente | Especificación | Por qué ese |
| :--- | :--- | :--- |
| **Optoacoplador MOC3063** | DIP-6, salida fototriac 600 V, con cruce por cero, corriente de disparo 5 mA | Los 5 mA los entrega el GPIO del ESP32 con holgura. El MOC3041 también sirve pero pide 15 mA |
| **Triac BTA41-600B** | 40 A, 600 V, encapsulado TOP-3 con **tab aislado** | El sufijo **BTA** significa tab aislado de la red (2500 V). El **BTB** no lo está: su tab está unido a MT2, o sea a los 120 V |

> **Comprá BTA, no BTB.** Si conseguís solo BTB, el disipador queda a potencial de red y
> necesitás aislador de mica más arandela de hombro, y aun así el disipador no se puede
> tocar. No vale la pena el riesgo.

Alternativas al BTA41 si no lo conseguís: **BTA24-600B** (25 A) o **BTA16-600B** (16 A).
Los tres aguantan 10 A; el más grande simplemente corre más frío.

### 3.1.1 Pinout del MOC3063

Según el datasheet de Fairchild (MOC306XM):

| Pin | Nombre | Se conecta a |
| ---: | :--- | :--- |
| 1 | `ANODE` | Resistencia de 330 Ω desde `GPIO23` |
| 2 | `CATHODE` | `GND` del ESP32 |
| 3 | `N/C` | Nada |
| 4 | `MAIN TERM.` | Puerta del triac |
| 5 | `NC*` — **sustrato del triac** | **Nada, nunca** |
| 6 | `MAIN TERM.` | Resistencia de 220 Ω hacia A2 del triac |

El pin 1 se identifica por la muesca del encapsulado.

> **El pin 5 no es un pin libre.** El datasheet lo marca *"DO NOT CONNECT (TRIAC
> SUBSTRATE)"*. Usarlo de puente en la protoboard daña el dispositivo. Al aire, igual
> que el 3.

Los pines 4 y 6 son los dos terminales del fototriac, que es bidireccional. La
orientación indicada es la de los circuitos de aplicación del datasheet.

### 3.2 Componentes pasivos

| Componente | Valor | Función |
| :--- | :--- | :--- |
| Resistencia de entrada | **330 Ω**, 1/4 W | Limita la corriente del LED del optoacoplador a ~6 mA. Una de 220 Ω también sirve: da 9 mA |
| Resistencia de puerta | **220 Ω**, 1 W | Va entre el pin 6 del optoacoplador y A2. **Nunca por debajo de 180 Ω** |
| Resistencia de puerta a A1 *(opcional)* | 1 kΩ, 1/4 W | Mejora la inmunidad al ruido, evita disparos falsos |
| Resistencia del snubber | **100 Ω**, 2 W | Amortigua el dV/dt |
| Condensador del snubber | **100 nF, clase X2, 275 VAC** | Va conectado a la red: **tiene que ser X2**. Un cerámico común no sirve |
| Varistor (MOV) | 14 mm, 130–150 VAC de operación (S14K130 o 14D201K) | Absorbe picos de la red |

**La resistencia de puerta no es un valor arbitrario.** Es lo único que limita la
corriente del fototriac del MOC3063, que aguanta **1 A de pico**, contra los 170 V de
pico de la senoidal:

| Resistencia | Corriente pico a 120 V | Veredicto |
| ---: | ---: | :--- |
| 100 Ω | 1,70 A | **Destruye el optoacoplador** |
| 180 Ω | 0,94 A | Justo en el límite |
| **220 Ω** | **0,77 A** | Buen margen, y es el valor más fácil de conseguir |

Con 220 Ω quedan hasta 750 mA disponibles para la puerta a 120 V, y ~134 mA en la prueba
a 24 V. El BTA41 necesita 50 mA, así que sobra en los dos casos: el mismo valor sirve
para la prueba y para el montaje final.

**El condensador X2 tampoco es negociable.** Un capacitor conectado entre línea y neutro
que falle en cortocircuito provoca un incendio. Los X2 están diseñados para fallar en
circuito abierto.

### 3.3 Mecánica y conexión

| Componente | Especificación |
| :--- | :--- |
| Disipador para el triac | **≥ 3 °C/W**, con aletas, aproximadamente 100 × 60 × 25 mm |
| Pasta térmica | Aunque el tab sea aislado |
| Tornillo M3 + arandela | Para fijar el triac al disipador |
| Placa PCB | FR4 de 1,6 mm, cobre de **2 oz** si conseguís; si no, 1 oz reforzado con estaño |
| Borneras de tornillo | Paso 7,62 mm, ≥ 16 A, para el lado AC |
| Bornera o tira de pines | Paso 2,54 mm, para el lado de control |
| Cable AC | **AWG 14, silicona 200 °C** (el mismo de la lista general) |
| Cable de control | AWG 22 |
| Zócalo DIP-6 | Opcional, para no soldar el optoacoplador directo |

---

## 4. Disipación y disipador

El triac cae aproximadamente **1,2 V** cuando conduce, independientemente de la corriente
(es una juntura, no una resistencia). A 10 A:

```
P = V_T × I = 1,2 × 10 = 12 W
```

Doce vatios continuos. Eso es más de lo que parece: es una bombilla halógena pequeña.

Para mantener la juntura por debajo de 125 °C con 40 °C de ambiente:

```
resistencia térmica total permitida = (125 − 40) / 12 = 7,1 °C/W
menos la de juntura a cápsula (~1 °C/W) y la de cápsula a disipador (~0,5 °C/W)
→ el disipador necesita 5,6 °C/W o menos
```

Pedí **3 °C/W** para tener margen, porque el ambiente dentro de la máquina no va a ser
40 °C. Y montá el disipador **fuera de la zona caliente del cilindro**, con ventilación.

> Dato útil: como el control es por ancho de pulso, el triac solo disipa esos 12 W cuando
> el PID pide el 100 %. En régimen estacionario, con el horno mantenido, la potencia
> media es mucho menor. Pero el disipador se dimensiona para el peor caso.

---

## 5. Reglas de la PCB

Esta es la parte donde se gana o se pierde la seguridad del montaje.

### 5.1 Ancho de pista para 10 A

Según IPC-2221, pista externa, para 10 A:

| Cobre | Aumento de 10 °C | Aumento de 20 °C |
| :--- | ---: | ---: |
| 1 oz (35 µm) | **7,2 mm** | 4,7 mm |
| 2 oz (70 µm) | **3,6 mm** | 2,4 mm |

Son pistas anchísimas. Dos formas de resolverlo:

**Opción A — reforzar con estaño.** Dibujás la pista lo más ancha que te quepa (5 mm
mínimo), dejás esa zona **sin máscara antisoldante**, acostás encima un alambre de cobre
desnudo de 1,5–2,5 mm² y lo inundás de estaño. Multiplica la sección efectiva. Es lo que
se hace en la práctica y es perfectamente válido.

**Opción B — no pasar los 10 A por la placa.** Es lo que hacen los SSR comerciales por
dentro: la PCB lleva **solo el circuito de disparo**, y los dos cables de potencia llegan
directamente a las patas MT1 y MT2 del triac, soldados ahí o con terminales de ojillo. El
triac va atornillado al disipador, no a la placa.

**La opción B es la que recomiendo.** Es más segura, más fácil de fabricar y sigue siendo
un módulo en PCB. La placa hace su trabajo: aislar y disparar.

### 5.2 Separación entre el lado de control y el lado de red

Esta es la cota crítica del diseño.

| Entre qué | Mínimo | Recomendado |
| :--- | ---: | ---: |
| Lado de control ↔ lado de red | 6,4 mm | **8 mm** |
| Pistas de red de distinto potencial (fase ↔ neutro/carga) | 1,5 mm | **3 mm** |

Y el truco que usan todos los SSR comerciales: **una ranura fresada en la placa, debajo
del cuerpo del optoacoplador**, entre sus pines 1-2-3 y sus pines 4-5-6. El encapsulado
DIP-6 solo mide unos 7,6 mm de fila a fila, así que la ranura es lo que te da la
distancia de fuga real. Si tu fabricante no hace fresados, se puede cortar a mano con una
sierra de calar fina o con un Dremel.

### 5.3 Las otras reglas

- **Agrupá todo el lado de red en un extremo** de la placa. Una línea imaginaria divide
  la placa en dos, y nada de control cruza a la zona de red ni al revés.
- **Serigrafía la zona peligrosa.** Un recuadro con `120 V` impreso o dibujado con
  marcador. Vos sabés dónde está; el que agarre la placa dentro de seis meses, no.
- **Limpiá el flux.** El residuo de flux es higroscópico y conductivo: te arruina la
  distancia de fuga que tanto cuidaste. Alcohol isopropílico y cepillo.
- **Barniz protector** sobre la zona de red, si conseguís. Después de limpiar, nunca
  antes.
- **Agujeros de montaje** separados de cualquier pista de red, y con tornillos aislantes
  o con distancia suficiente.
- **Descarga de tracción** para los cables de red: un agujero por donde pase el cable y
  se le haga un nudo, o una abrazadera. Un tirón nunca debe llegar a la soldadura.

---

## 6. Orden de construcción y prueba

### Etapa A — Lado de control, en protoboard

Solo el optoacoplador y su resistencia. Nada de red en ningún lado.

```
GPIO23 ──[330 Ω]── pin 1 (MOC3063)
GND    ─────────── pin 2 (MOC3063)
```

Cargá el sketch `prueba_ssr` y medí con el multímetro en **miliamperios DC** en serie con
la resistencia, con el comando `ON`. Deberías leer **5 a 7 mA**. Si lees menos de 5 mA, el
optoacoplador no va a disparar confiablemente: bajá la resistencia a 270 Ω.

### Etapa B — Montaje de la placa, sin energizar

Soldá todo. Después, con el multímetro en continuidad, verificá:

| Qué medir | Resultado esperado |
| :--- | :--- |
| Pin 1 o 2 del optoacoplador contra cualquier punto del lado de red | **Sin continuidad.** Si hay, parás y revisás |
| Terminal de fase contra terminal de carga, en reposo | Sin continuidad (el triac está abierto) |
| Tab del triac contra los terminales de red | Sin continuidad, si usaste BTA |
| Visual: distancia mínima entre cobre de control y cobre de red | ≥ 6,4 mm en todos los puntos |

### Etapa C — Primera prueba con carga, pero **no** con la resistencia

#### La carga mínima no es libre: la fija el triac

Un triac necesita una **corriente de mantenimiento** para quedarse enganchado después de
que la puerta lo dispara. El BTA41 pide hasta unos **100–120 mA**. Por debajo de eso se
dispara en cada cruce por cero y se suelta enseguida: parpadeo errático, o nada.

```
carga mínima a 120 V = 0,12 A × 120 V ≈ 14 W
```

Con margen: **40 W como mínimo, 60 W cómodo**. Una bombilla de 1 W consume 8 mA y **no
funciona** — y el síntoma parece una falla del circuito cuando en realidad es la carga.

Y tiene que ser **incandescente**. Las LED y las ahorradoras traen fuente conmutada
adentro: consumen poco, su entrada es capacitiva y se comportan mal con triacs.

#### Opción segura: probarlo entero a 24 V

Antes de tocar los 120 V, se puede validar el circuito completo con un **transformador de
24 V AC** (el de timbre de puerta) y una **lámpara de 24 V / 10 W**. Nada supera los 34 V
de pico, así que esta versión **sí se puede armar en protoboard**, y a 24 V el detector de
cruce por cero del MOC3063 sigue funcionando de verdad, porque su tensión de inhibición
ronda los 20 V.

Es la forma de descubrir un cableado invertido con 24 V en lugar de con 120.

Para esta prueba se usan **solo cuatro componentes**: el optoacoplador, el triac, la
resistencia de 330 Ω y la de 220 Ω. El snubber, el varistor y el disipador **no van
todavía** — el snubber y el varistor protegen de la red, que acá no existe, y a 0,42 A el
triac disipa medio vatio.

> **El BTA41 no entra en la protoboard.** Viene en encapsulado TOP-3, con patas bastante
> más gruesas que un TO-220. Forzarlas abre los contactos de forma permanente y después
> esa fila ya no hace contacto con nada. Soldale a cada pata un tramo de alambre rígido
> AWG 22, o usá caimanes. El MOC3063 en DIP-6 sí entra bien, a caballo del canal central.

**No se une ningún GND entre los dos lados.** Ni referencia, ni masa común. Lo único que
vincula el lado del ESP32 con el del transformador es la luz dentro del optoacoplador.
Cerrar ese circuito con un cable anula la razón de ser del diseño.

#### Identificación de las patas del triac

ST nombra las patas del BTA41 como **A1, A2 y G**. Es la misma cosa que MT1, MT2 y
puerta; solo cambia la nomenclatura.

**Para el BTA41-600B en TOP-3**, sosteniéndolo con la cara marcada hacia el observador,
el tab metálico arriba y las patas hacia abajo:

| Pata | Nombre |
| :--- | :--- |
| **Izquierda** | **A1** |
| **Del medio** | **A2** |
| **Derecha** | **G** (puerta) |
| Tab metálico | Aislado, no es una conexión |

Los códigos impresos en el plástico (`PHL`, la fecha de lote) son del fabricante: **no
identifican las patas**. Ningún triac las rotula; la identificación es por posición.

**Confirmación en 30 segundos.** El orden cambia entre encapsulados, así que conviene
verificarlo. Multímetro en ohmios, escala 200 Ω, triac desconectado de todo:

| Par | Lectura esperada |
| :--- | :--- |
| Izquierda – derecha | **Decenas de ohmios** (es el par G–A1) |
| Izquierda – medio | Abierto |
| Medio – derecha | Abierto |

Si da eso, la tabla de arriba es correcta. Si el par de lectura baja resulta ser otro, el
criterio general es el siguiente.

| Par | Lectura esperada |
| :--- | :--- |
| **G – A1** | **Baja**: decenas de ohmios, típico 10–100 Ω |
| G – A2 | Abierto |
| A1 – A2 | Abierto |

Solo un par da lectura baja: ese par son **la puerta y A1**, y **la pata que sobra es
A2**. Esa es la certeza que hace falta, porque A2 es donde llegan el transformador y la
resistencia de 220 Ω.

Para separar G de A1 entre las dos restantes, se prueba una y si la lámpara no enciende
se invierten. A 24 V no se daña nada: la resistencia de 220 Ω limita la corriente por el
optoacoplador a 134 mA, muy por debajo del 1 A que aguanta. Es otra razón para hacer esta
prueba a 24 V y no a 120.

Como contraste, en el BTA41 en TOP-3 lo habitual es **A1, A2, G** de izquierda a derecha,
con la cara marcada hacia el observador y las patas hacia abajo. La medición manda sobre
eso.

#### Verificar que el triac es realmente un BTA

Medir continuidad entre el **tab metálico y cada una de las tres patas**: las tres deben
dar **abierto**. Si el tab tiene continuidad con alguna pata, es un **BTB**, no un BTA, y
en el montaje final el disipador quedaría a potencial de red.

El pin 1 del MOC3063 se identifica por la muesca del encapsulado.

#### Después, a 120 V

Usá una **bombilla incandescente de 40 o 60 W** como carga en lugar de las resistencias
de la palomitera. Razones:

- Consume menos de 1 A, así que un error no funde nada.
- **Ves el ciclo de trabajo directamente en el brillo.** Al 25 % parpadea notoriamente, al
  100 % queda fija. Es la mejor herramienta de diagnóstico que vas a tener.
- Si el triac se queda enganchado (falla común), lo ves de inmediato: la bombilla no se
  apaga con el comando `OFF`.

Corré el barrido `AUTO` del sketch de prueba y observá. Tomacorriente con diferencial, y
presente todo el tiempo.

### Etapa D — Carga real

Recién ahora las resistencias. Con `P100` sostenido durante 10 minutos, tocá el disipador:
debería estar caliente pero **soportable al tacto por un segundo**. Si no lo podés tocar,
el disipador es chico.

Mejor todavía: medí con el termopar apoyado en el disipador. Por encima de 80 °C, agrandá.

---

## 7. Si algo sale mal

| Síntoma | Causa probable |
| :--- | :--- |
| La carga nunca enciende | Corriente insuficiente en el LED del optoacoplador. Medí los mA. O el optoacoplador está al revés: el pin 1 se identifica por la muesca del encapsulado |
| La carga **nunca se apaga** | El triac se destruyó en cortocircuito, casi siempre por falta de snubber o por sobretemperatura. Desconectá de la red **ya**. Un triac en corto es el modo de falla peligroso: el ESP32 pierde todo control y solo te queda el bimetálico |
| Enciende de forma errática | Falta el snubber, o ruido en la puerta. Agregá la resistencia de 1 kΩ entre puerta y MT1 |
| El disipador quema | Subdimensionado, o falta pasta térmica, o el tornillo está flojo |
| Se dispara el diferencial | Fuga a tierra. Revisá si usaste BTB en lugar de BTA, y el aislamiento de todo el lado de red |

> **El modo de falla que importa:** un triac muerto queda en cortocircuito, no en
> circuito abierto. Es decir, la resistencia queda encendida permanentemente y el
> firmware no puede hacer nada. Por eso el bimetálico y el fusible térmico siguen siendo
> imprescindibles: son las únicas protecciones que no dependen del semiconductor.
