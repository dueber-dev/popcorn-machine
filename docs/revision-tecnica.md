# Revisión técnica de la documentación inicial

Auditoría de [`documentacion_inicial_del_proyecto.md`](documentacion_inicial_del_proyecto.md).
Ordenada por severidad. Lo marcado como **BLOQUEANTE** hay que resolverlo antes de
energizar nada con AC; lo marcado como **RECOMENDADO** es buena práctica que conviene
hacer igual.

---

## A. Fallos en el cableado / diseño eléctrico

### A1. El fusible térmico quedó fuera del ramal del motor — **BLOQUEANTE**

La sección 2 dice que el fusible de 15 A está *"en serie con la salida hacia la red AC"*,
es decir, protege **toda** la máquina. Pero el diagrama de la sección 3 lo coloca
**después del SSR**, en el ramal de la resistencia grande únicamente.

Con ese cableado, la resistencia pequeña, el puente de diodos y el motor quedan
conectados a la red **sin ninguna protección de sobrecorriente ni térmica**. Además,
un fusible térmico es un dispositivo de un solo disparo que actúa por temperatura de
la placa: si lo sacás del camino común, deja de cumplir su función original.

**Corrección:** el fusible térmico va en el tronco común, entre el interruptor/bimetálico
y el nodo donde se bifurcan los dos ramales:

```
[Fase] → [Interruptor] → [Fusible térmico 15 A] → [Bimetálico] → ┬─→ [Resistencia pequeña] → [Puente de diodos] → [Motor] → [Neutro]
                                                                 └─→ [Resistencia grande] → [Relé de estado sólido] → [Neutro]
```

---

### A2. Hay que verificar si las dos resistencias son realmente independientes — **RESUELTO**

> **Resuelto** por la topología de [`guia-de-armado.md`](guia-de-armado.md): el motor se
> alimenta con fuente DC propia y deja de depender de las resistencias, así que el
> escenario de abajo ya no puede darse. Se conserva el análisis porque la medición de
> las resistencias sigue siendo necesaria, ahora por otro motivo: confirmar que el
> paralelo de ambas no excede la corriente del SSR y del fusible.

Este es el error clásico de esta modificación y el documento lo daba por resuelto sin
comprobarlo.

El diagrama asume dos ramales **en paralelo** desde el mismo nodo. Eso solo es válido
si la "resistencia pequeña" es un *dropper* de dos terminales dedicado al motor
(típicamente entre 400 Ω y 1 kΩ, disipando 15–25 W).

Pero en muchas palomiteras de aire el motor **no** cuelga de una resistencia aparte:
cuelga de una **derivación (tap) de la misma bobina de nicrom**. En ese caso la
resistencia forma un único elemento de 3 terminales, y la tensión que ve el motor
depende de la corriente que circula por la sección principal. Si cortás la sección
principal con el SSR, la tensión en el tap **se desploma y el motor se para** —
exactamente lo que el documento dice querer evitar. Y un horno con la resistencia
ciclando y el ventilador parado es justo el escenario peligroso.

**Cómo verificarlo** (con la máquina desenchufada, multímetro en ohmios):

1. Desconectá todos los terminales del plato de resistencias.
2. Medí cada elemento por separado.
   - **R grande:** decenas de ohmios (≈ 14–20 Ω para ~1000 W a 120 V, o ≈ 50–60 Ω a 230 V).
   - **R pequeña:** si mide **cientos de ohmios** → es un dropper independiente, el
     diagrama del documento es correcto.
   - Si la R pequeña mide **decenas de ohmios** y comparte un terminal físico con la
     grande → es un tap: el diagrama no sirve.
3. Contá los terminales del conjunto: **4 terminales** = elementos independientes;
   **3 terminales** = elemento derivado.

**Si resulta ser un tap:** hay que independizar el motor. Lo más limpio es sacar el
motor del circuito AC y alimentarlo con una fuente DC pequeña (12 V / 1 A), quitando
el puente de diodos y la resistencia pequeña del camino. Esa fuente puede ir en el
mismo punto que alimenta el ESP32.

---

### A3. El MAX6675 a 5 V destruye el GPIO19 del ESP32 — **BLOQUEANTE**

La sección 3A dice `VCC → Pin 3.3V o 5V`. El MAX6675 acepta 3,0–5,5 V, pero su pin `SO`
saca lógica **al nivel de su VCC**. Los GPIO del ESP32 **no son tolerantes a 5 V**.

**Corrección:** alimentar el módulo HW-550 exclusivamente a **3,3 V**. No es opcional.

---

### A4. Falta el pulldown en la línea de disparo del SSR — **RECOMENDADO**

> **Corrección de calificación.** Esto se marcó primero como bloqueante y no lo es para
> el montaje concreto de este proyecto. El análisis honesto está abajo.

Durante el reset, el boot y el flasheo, el GPIO23 queda en alta impedancia.

**Por qué con este relé no es crítico:** la entrada del SSR-25 DA es un LED con
resistencia en serie, con el cátodo al GND del ESP32. Si el GPIO queda en alta
impedancia, sencillamente **no hay camino de corriente** que encienda ese LED: no hay
fuente que lo alimente. El GPIO23 tampoco tiene pull-up interno activo al reset, así que
no se va a ir solo a nivel alto. En la práctica, un pin flotante deja este relé apagado.

**Por qué ponerlo igual:**

- Cuesta centavos y es práctica estándar en cualquier salida que maneje potencia.
- Deja el pin en un estado **definido** en vez de indeterminado. Un pin flotante es
  susceptible a ESD y a acoplamiento, y "en la práctica no pasa" no es lo mismo que "no
  puede pasar".
- Si algún día cambiás el SSR por uno de esos módulos chinos en placa con transistor de
  entrada, ahí sí una entrada flotante puede significar **encendido**. El pulldown te
  cubre ese cambio sin tener que acordarte.
- Es verificable: con el ESP32 sin alimentar, medís ≈ 10 kΩ entre GPIO23 y GND y sabés
  que está puesto.

**Corrección:** resistencia de **10 kΩ entre GPIO23 y GND**, física, en la placa. El
firmware pone el pin en LOW como primera instrucción del `setup()`, pero eso solo cubre
desde que arranca el programa.

Para una prueba de banco sin nada conectado a la red, es prescindible. Lo que sí protege
de verdad contra un disparo no comandado con la resistencia cableada son el bimetálico y
el fusible térmico, que no dependen de ningún componente de señal.

---

### A5. El bimetálico original va a cortar el ventilador junto con la resistencia — **RESUELTO PARCIALMENTE**

> La topología de [`guia-de-armado.md`](guia-de-armado.md) saca el ventilador del tronco
> común, así que el bimetálico ya no lo corta. Lo que sigue vigente es lo otro: el
> bimetálico probablemente abre cerca del setpoint de trabajo, y hay que medir a qué
> temperatura lo hace para elegir el setpoint máximo útil.

El documento conserva el bimetálico en el tronco común (bien, es un backstop
independiente), pero no menciona la consecuencia: el bimetálico de una palomitera
suele abrir alrededor de **200–250 °C** en el cilindro. Si el setpoint del horno es
200 °C medidos por el termopar, es muy probable que el bimetálico abra en operación
normal — y al estar en el tronco común, **corta también el motor**, dejando el cilindro
caliente sin circulación de aire.

**Qué hacer:**
- Medir a qué temperatura abre realmente (registrando la lectura del termopar cuando
  el sistema se corta solo).
- Trabajar con un setpoint claramente por debajo de ese punto, o sustituir el bimetálico
  por uno de corte más alto **manteniéndolo siempre por debajo del fusible térmico**.
- No puentearlo nunca. Es la única protección que no depende del firmware.

---

### A6. Usar sonda de junta aislada (ungrounded)

En el MAX6675, `T-` está referenciado internamente a `GND`, que es el mismo GND del
ESP32. Si la punta del termopar es de **junta puesta a masa** (grounded junction) y
queda en contacto eléctrico con el cilindro de aluminio, cualquier fuga de la
resistencia hacia el chasis llega directo a la electrónica de control y al USB.

**Corrección:** sonda tipo K de **junta aislada**, y aun así montarla con pasamuros
aislante. Cablear el termopar separado del cableado AC (cruces a 90°, nunca paralelos)
para que el ruido de conmutación no ensucie la lectura.

---

### A7. No está especificada la alimentación del ESP32

El documento no dice de dónde sale la alimentación del microcontrolador. Si se alimenta
por USB desde una laptop durante la operación, la laptop queda referenciada al mismo
GND que la electrónica montada junto a un circuito de red.

**Corrección:** fuente AC-DC aislada (módulo Hi-Link HLK-PM01 5 V o similar) dentro de la
carcasa, tomada **antes** del SSR pero **después** del fusible. Usar el USB solo para
flashear, idealmente con la potencia AC desconectada.

---

### A8. Margen de disparo del SSR-25DA a 3,3 V

La entrada del SSR-25DA está especificada típicamente como 3–32 VDC. A 3,3 V estás en el
borde inferior del rango; muchos ejemplares funcionan, otros disparan de forma errática
según la temperatura.

**Recomendación:** verificar en la etiqueta del ejemplar concreto. Si indica 4 V mínimo,
o si se ve comportamiento intermitente, intercalar un MOSFET de señal (2N7000) o un
transistor NPN con la entrada del SSR alimentada a 5 V. Dejar el pulldown de A4 del lado
del GPIO en cualquier caso.

---

### A9. Detalle menor: el disipador del SSR

El SSR-25DA disipa aproximadamente 1 W por amperio conmutado. Con una resistencia de
~1000 W a 120 V son ~8 A → ~8 W. El documento pide 2–5 mm de separación del plástico,
pero no menciona **pasta térmica** entre SSR y disipador, que es lo que realmente
determina si el disipador sirve.

---

## B. Fallos en el código PID de la sección 5

### B1. Las ganancias están mal escaladas — el controlador casi no actúa

Es el fallo funcional más grande del código original.

```cpp
myPID.SetOutputLimits(0, tamañoVentana);  // 0 a 2000
double Kp = 2.0;
```

La salida del PID está en **milisegundos de una ventana de 2000 ms**, pero `Kp = 2.0`
significa *2 ms de encendido por cada grado de error*. Es decir: **0,1 % de potencia por
grado**. Para pedir potencia completa harían falta 1000 °C de error.

En la práctica el término proporcional es irrelevante y todo el trabajo recae sobre el
integrador, que es lentísimo: el horno tarda muchísimo en llegar al setpoint y luego se
pasa de largo.

**Corrección aplicada en el firmware nuevo:** la salida del PID se expresa en
**porcentaje (0–100)** y el ancho de pulso se calcula aparte. Así las ganancias no
dependen del tamaño de la ventana y `Kp` tiene un significado físico directo:
`Kp = 100 / banda_proporcional`.

---

### B2. `readCelsius()` devuelve `NAN` ante termopar abierto — y el original lo mete al PID

```cpp
tempActual = thermocouple.readCelsius();   // puede ser NAN
myPID.Compute();                           // aritmética con NAN
```

Si el termopar se desconecta, se rompe o se afloja el conector, la librería devuelve
`NAN`. Al propagarse por el PID, la salida queda indefinida y la comparación
`salidaPID > (ahora - inicioVentana)` puede evaluar de forma que **el SSR se quede
encendido permanentemente**. Es el modo de fallo más peligroso del montaje: horno a
plena potencia y sin sensor.

**Corrección:** validación de cada lectura (`isnan` + rango plausible), contador de
lecturas malas consecutivas, y estado `FALLO` enclavado que abre el SSR.

---

### B3. No hay límite duro de temperatura ni detección de fuga térmica

El código original no tiene ningún corte por sobretemperatura: si el PID se desintoniza
o el termopar se sale del cilindro, la resistencia sigue funcionando hasta que actúe el
bimetálico.

**Corrección:** `TEMP_MAX_ABSOLUTA` (corte inmediato) más un vigilante que aborta si el
sistema lleva 90 s pidiendo el 100 % de potencia y la temperatura no ha subido ni 5 °C
(síntoma de sensor descolgado, SSR muerto, resistencia abierta o bimetálico abierto).

---

### B4. El PID calcula más rápido de lo que el sensor se actualiza

`PID_v1` tiene un `SampleTime` por defecto de **100 ms**, pero el termopar se lee cada
250 ms. El PID hace ~2,5 cálculos por cada dato nuevo, integrando repetidamente el mismo
error y derivando sobre una señal escalonada.

**Corrección:** `myPID.SetSampleTime(1000)`, coherente con la dinámica térmica del
sistema, y lectura cada 300 ms (el MAX6675 necesita ≥ 220 ms de conversión, así que
250 ms era un margen demasiado ajustado).

---

### B5. `tamañoVentana` usa un identificador no ASCII

La `ñ` en un identificador C++ solo compila con GCC ≥ 10 y según la configuración del
core ESP32. Con toolchains más viejos es error de compilación directo. Renombrado a
`VENTANA_SSR_MS`.

---

### B6. La ventana se reajusta con `if` en lugar de `while`

```cpp
if (ahora - inicioVentana >= tamañoVentana) inicioVentana += tamañoVentana;
```

Si el loop se bloquea más de una ventana (una reconexión WiFi, una escritura a flash),
solo se recupera una ventana por iteración y el ciclo queda desalineado. Cambiado a
`while`.

---

### B7. Sin arranque seguro ni forma de parar

El original pone `digitalWrite(pinSSR, LOW)` después de `Serial.begin()`, y no ofrece
ninguna manera de detener el calentamiento ni de cambiar el setpoint sin recompilar.

**Corrección:** el SSR se fuerza a LOW como primerísima instrucción, se valida el sensor
antes de habilitar el control, y hay comandos por puerto serie (`S<valor>`, `ON`, `OFF`).

---

## C. Sintonización recomendada (método práctico)

Con el firmware nuevo, `Kp = 4.0, Ki = 0.05, Kd = 20.0` es un punto de partida
conservador (banda proporcional de 25 °C, Ti = 80 s, Td = 5 s).

Para afinar sin matemáticas, método de ciclo límite:

1. Poné `Ki = 0` y `Kd = 0`.
2. Subí `Kp` hasta que la temperatura oscile de forma sostenida alrededor del setpoint.
   Anotá esa ganancia como `Ku` y el periodo de oscilación como `Tu` (en segundos).
3. Aplicá Ziegler-Nichols "sin sobreimpulso", que es lo adecuado para un horno:
   - `Kp = 0.2 · Ku`
   - `Ki = Kp / (0.5 · Tu)`
   - `Kd = Kp · (Tu / 3)`
4. Si sigue habiendo sobreimpulso al llegar al setpoint, bajá `Ki` a la mitad.

Hacé la sintonización **con la tapa puesta y el ventilador funcionando**, porque el
flujo de aire cambia por completo la respuesta térmica.

---

## D. Checklist antes del primer encendido con AC

- [ ] Fusible térmico en el tronco común (A1).
- [ ] Verificado con multímetro si las resistencias son independientes o derivadas (A2).
- [ ] MAX6675 alimentado a 3,3 V (A3).
- [ ] Pulldown de 10 kΩ en GPIO23 (A4).
- [ ] Bimetálico intacto, temperatura de apertura conocida (A5).
- [ ] Sonda de junta aislada, cableado separado del AC (A6).
- [ ] Alimentación aislada para el ESP32 (A7).
- [ ] Pasta térmica entre SSR y disipador (A9).
- [ ] Nada de soldadura de estaño en la zona caliente (ya estaba en el doc original).
- [ ] Primera prueba con la resistencia **desconectada**: verificar que la telemetría
      lee bien el termopar y que el LED del SSR sigue el ciclo esperado.
- [ ] Segunda prueba con setpoint bajo (80 °C) antes de ir a temperatura de horno.
