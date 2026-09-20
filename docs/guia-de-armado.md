# Guía de armado — topología con motor independiente

Esta es la arquitectura **decidida** del proyecto, y reemplaza el esquema de cableado
de la [documentación inicial](documentacion_inicial_del_proyecto.md).

**Cambio respecto al diseño original:** el motor del ventilador se saca por completo del
circuito AC. Se eliminan la resistencia-divisor y el puente de diodos de su camino, y se
alimenta con una fuente DC propia. Las dos resistencias quedan en paralelo y el SSR
conmuta el tronco común de ambas.

Se asume red de **120 V / 60 Hz**. Con 230 V hay que rehacer todos los cálculos de
corriente de este documento.

---

## Por qué este cambio es el correcto

| Problema del diseño anterior | Cómo lo resuelve |
| :--- | :--- |
| **A2** — Si las resistencias eran una bobina derivada, el ventilador se paraba al regular | Desaparece. El motor ya no depende de la resistencia para nada |
| **A5** — El bimetálico cortaba el ventilador junto con la resistencia | Desaparece. El bimetálico queda solo en el ramal del calor; el ventilador sigue soplando y enfría el cilindro |
| Tensión del motor variable según la corriente de la resistencia | Desaparece. Tensión DC fija |

Además, el ventilador ahora sobrevive incluso a que se funda el fusible térmico, que es
exactamente lo que querés cuando el cilindro está a 200 °C.

---

## El cálculo que hay que hacer ANTES de cablear

Al poner las dos resistencias en paralelo, el SSR y el fusible pasan a ver la **suma** de
las dos corrientes. Hay que confirmar que eso cabe.

Con la máquina desenchufada y las resistencias desconectadas de todo, medí cada una con
el multímetro en ohmios y calculá:

```
R_paralelo = (R_grande × R_pequeña) / (R_grande + R_pequeña)

I = 120 / R_paralelo          P = 120² / R_paralelo
```

| R_paralelo | Corriente | Potencia | Veredicto |
| ---: | ---: | ---: | :--- |
| 20 Ω | 6,0 A | 720 W | OK |
| 15 Ω | 8,0 A | 960 W | OK |
| 14 Ω | 8,6 A | 1029 W | OK — caso típico |
| 12 Ω | 10,0 A | 1200 W | OK, límite cómodo |
| 10 Ω | 12,0 A | 1440 W | Aceptable, disipador generoso |
| 8 Ω | 15,0 A | 1800 W | **Funde el fusible de 15 A** |
| 6 Ω | 20,0 A | 2400 W | **No. Destruye el SSR** |

**Regla:** `R_paralelo ≥ 12 Ω`. Si te da menos, no conectés la resistencia pequeña en
paralelo — dejala fuera.

### Y revisá la resistencia pequeña por separado

Antes, la resistencia pequeña tenía el motor en serie, así que veía menos de 120 V.
Ahora va directa a la red y disipa más:

```
P_nueva = 120² / R_pequeña
```

Si era un *dropper* de ~600 Ω, pasa de ~21 W a ~24 W: intrascendente. Si mide mucho
menos, el elemento va a trabajar bastante por encima de su punto de diseño.

**Criterio de decisión:**

| R_pequeña medida | Qué hacer |
| :--- | :--- |
| Cientos de ohmios (300 Ω – 1 kΩ) | Aporta ~15–50 W. Ponela en paralelo si está físicamente ubicada de forma que reparta mejor el calor en el cilindro; si no, es indiferente |
| Decenas de ohmios | Recalculá `R_paralelo` con la tabla de arriba antes de decidir |
| Comparte terminal físico con la grande (3 terminales en total) | **No la conectés en paralelo.** Es una derivación de la misma bobina; ponerla directa a 120 V es un cortocircuito parcial |

Dejarla afuera no es ninguna pérdida: son unos 24 W sobre ~1000 W.

---

## Esquema eléctrico

```
[Fase]
   │
[Interruptor general]
   │
   ├──────────────────────────────────────────────┐
   │                                              │
[Fusible térmico 15 A]                      [Fusible 1 A]
   │                                              │
[Bimetálico]                          [Fuente AC-DC aislada 12 V]
   │                                              │
[SSR terminal 1]                                  ├──> [Motor ventilador 12 V]
[SSR terminal 2]                                  │
   │                                              └──> [Buck 12→5 V] ──> [ESP32]
   ├──> [R grande]  ──┐                                                     │
   ├──> [R pequeña] ──┤  (en paralelo, si el cálculo lo permite)           │
   │                  │                                                     │
[Neutro] <────────────┘                                          [GND común DC]
```

**Puntos clave del orden:**

- El ramal DC se toma **antes** del fusible térmico y del bimetálico, con su propio
  fusible de 1 A. Así el ventilador y el controlador siguen vivos aunque se abra el
  bimetálico o se funda el fusible térmico — y pueden enfriar el cilindro.
- El fusible de 15 A y el bimetálico quedan dedicados al ramal de calor, que es donde
  tienen sentido.
- El bimetálico va **antes** del SSR, para que el SSR ni siquiera reciba AC cuando esté
  abierto.
- El SSR no lleva el ramal DC: solo conmuta las resistencias.

---

## Alimentación: todo de una sola clavija, sin baterías

La "fuente DC propia" del motor **no es una batería ni una fuente externa**. Es un módulo
AC-DC que va montado dentro de la máquina y se alimenta del mismo cable de red. El
aparato terminado tiene **un solo cable a la pared**.

| Qué | Componente | Notas |
| :--- | :--- | :--- |
| 12 V para el motor | Módulo AC-DC aislado de 12 V, ≥ 1 A (HLK-10M12, o cualquier fuente open-frame de 12 V / 2 A) | Dimensionar al doble de la corriente medida en la Etapa 2. Si el motor resulta ser de 24 V, fuente de 24 V |
| 5 V para el ESP32 | Convertidor buck MP1584 o LM2596 colgado de los 12 V | Entra al pin `5V`/`VIN`, no al `3V3` |
| 3,3 V para el MAX6675 | Regulador de la propia placa ESP32 | Pin `3V3` (ver A3) |
| Protección | Fusible de 1 A en el ramal DC | El de 15 A nunca protegería un consumo de < 1 A |

### Por qué la conmutación del SSR no afecta a la electrónica

El ramal DC cuelga de la red **en paralelo** con el ramal de calor, no en serie. El
módulo AC-DC trabaja en un rango de entrada de 85–265 V y tiene regulación propia, así
que la conmutación de ~8 A del SSR no se refleja en sus 12 V de salida. El motor ve
tensión fija todo el tiempo, independientemente de lo que haga el PID.

Este desacople es justamente lo que se ganó al sacar el motor del divisor de tensión. En
el diseño original los dos ramales estaban acoplados, y ese era el problema A2.

### La resistencia nunca ve tensión variable

No hace falta regular la tensión de la resistencia para regular la temperatura. La
resistencia solo ve **120 V o 0 V**. Lo que el PID controla es la fracción de tiempo
encendida dentro de la ventana de 2 s: 70 % de potencia = 1,4 s conduciendo, 0,6 s
apagado. Para una masa térmica con constante de tiempo de minutos, eso equivale a
aplicarle el 70 % de la potencia de forma continua.

Variar la tensión de verdad exigiría control por ángulo de fase con TRIAC y detección de
cruce por cero — más componentes, mucho más ruido eléctrico, y ningún beneficio para una
carga térmica. Además, el SSR-25DA es de disparo en cruce por cero y físicamente no
puede hacer control de fase.

> **Al flashear:** no alimentés el ESP32 por USB y por el buck al mismo tiempo. Desconectá
> el ramal DC (o la máquina de la pared) antes de conectar el USB.

---

### Control (sin cambios respecto al README)

| MAX6675 (HW-550) | ESP32 |
| :--- | :--- |
| `VCC` | **3,3 V** (nunca 5 V — ver A3) |
| `GND` | `GND` |
| `SCK` / `CS` / `SO` | `GPIO18` / `GPIO5` / `GPIO19` |

| SSR-25DA | ESP32 |
| :--- | :--- |
| Terminal 3 `(+)` | `GPIO23` **+ pulldown 10 kΩ a GND** (ver A4) |
| Terminal 4 `(-)` | `GND` |

---

## Orden de armado

### Etapa 0 — Medir y documentar (máquina desenchufada)

1. **Fotografiá el cableado original completo** antes de desconectar nada. Vas a
   necesitarlo.
2. Medí `R_grande` y `R_pequeña` por separado. Anotá los valores.
3. Contá los terminales del plato de resistencias: 4 = independientes, 3 = derivadas.
4. Aplicá la tabla de arriba y decidí si la pequeña entra en paralelo o se queda fuera.
5. Identificá los terminales del bimetálico y del fusible térmico.
6. **Averiguá la tensión del motor.** Si la máquina todavía funciona, medí en DC entre
   las salidas del puente de diodos con ella encendida. Si ya la desarmaste, anotá lo
   que diga la etiqueta del motor; si no dice nada, Etapa 2.

> Criterio de parada: si `R_paralelo` te da menos de 12 Ω con las dos en paralelo, parás
> acá y replanteás. No cableés "a ver qué pasa".

### Etapa 1 — Control en banco, sin nada de AC

Armá esto en protoboard, con el ESP32 alimentado por USB y **el SSR sin ningún cable de
red conectado**:

1. ESP32 + MAX6675 + termopar.
2. Pulldown de 10 kΩ entre GPIO23 y GND.
3. Terminales 3 y 4 del SSR al ESP32. Nada en los terminales 1 y 2.
4. Cargá [`popcorn_oven.ino`](../firmware/popcorn_oven/popcorn_oven.ino) y abrí el monitor
   serie a 115200.

**Qué tenés que ver:**

- Temperatura ambiente plausible en el log.
- **Validación del termopar:** metelo en agua hirviendo (100 °C a nivel del mar, restá
  ~1 °C por cada 300 m de altitud) y en agua con hielo (0 °C). Si los dos puntos cuadran
  dentro de 2–3 °C, la sonda y el módulo están bien.
- **Prueba de polaridad:** si al calentar la sonda la lectura *baja*, tenés `T+` y `T-`
  invertidos.
- El LED del SSR debe encender y apagar siguiendo el porcentaje del log: al 100 % queda
  fijo, al 50 % alterna un segundo sí y un segundo no.
- Desconectá el termopar en caliente: el firmware tiene que entrar en `FALLO` en ~1,5 s.
  **Probá esto explícitamente.** Es la seguridad que más te va a importar.

No pasés a la Etapa 3 hasta que esta etapa esté limpia.

### Etapa 2 — Motor con fuente DC

1. Con una fuente variable, subí desde 6 V hasta que el flujo de aire se parezca al
   original. Anotá tensión y corriente.
2. Elegí la fuente definitiva con al menos el doble de la corriente medida.
3. Si el motor pide más de 12 V, usá fuente de 24 V y un buck para los 5 V del ESP32.

Dejalo corriendo 10 minutos y tocá el motor: si se calienta de más, bajá la tensión.

### Etapa 3 — Potencia AC, verificación en seco

Con el enchufe **fuera** de la pared todo el tiempo:

1. Cableá según el esquema. Terminales de crimpado, clemas cerámicas o remaches — nunca
   estaño en la zona caliente.
2. Con el multímetro en continuidad, verificá:
   - Fase a Neutro con el interruptor abierto: **sin continuidad**.
   - Fase a Neutro con el interruptor cerrado: debe dar `R_paralelo` (el SSR en reposo
     tiene fuga, pero medís a través de las resistencias si puenteás el SSR; si no,
     medí cada tramo por separado).
   - Cualquier terminal AC contra el chasis metálico: **sin continuidad**. Si hay,
     parás.
   - GND del ESP32 contra el chasis: sin continuidad (esto verifica que la sonda es de
     junta aislada, A6).
3. Revisá que el disipador del SSR tenga pasta térmica y 2–5 mm de aire respecto de
   cualquier plástico.

### Etapa 4 — Primer encendido con AC

1. Conectá a un tomacorriente con **protección diferencial (GFCI)**.
2. Setpoint bajo: mandá `S80` por el monitor serie antes de enchufar.
3. Ventilador encendido primero, siempre.
4. Quedate presente, con la mano en el interruptor. No lo dejés solo.
5. Verificá que la temperatura suba y se estabilice cerca de 80 °C.
6. Subí de 20 en 20 grados: `S100`, `S120`… anotando dónde aparece sobreimpulso.
7. **Anotá a qué temperatura abre el bimetálico** (vas a verlo como una meseta con el
   PID pidiendo 100 % y la temperatura cayendo; el firmware lo va a marcar como
   `Sin respuesta térmica al 100% de potencia`). Ese valor define tu setpoint máximo
   útil.

### Etapa 5 — Sintonización

Con la tapa puesta y el ventilador corriendo, seguí el procedimiento de ciclo límite de
la [sección C de la revisión técnica](revision-tecnica.md).

---

## Qué cambia en el firmware

**Nada.** El lazo de control es idéntico: un solo actuador (el SSR) sobre una carga
puramente resistiva. Los valores de `Kp/Ki/Kd` sí van a cambiar con la sintonización,
porque la constante térmica del sistema cambia al tener caudal de aire constante.

Un detalle que mejora solo: como el ventilador ya no depende del bimetálico, cuando el
bimetálico abra vas a ver el `FALLO` de "sin respuesta térmica" con el cilindro
enfriándose de forma controlada, en vez de con el aire parado.
