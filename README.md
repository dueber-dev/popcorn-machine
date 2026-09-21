# Popcorn Machine → Horno de convección controlado

Conversión de una palomitera de aire caliente en un horno de convección con control de
temperatura en lazo cerrado.

**ESP32** + **MAX6675 / termopar tipo K** + **SSR-25DA**, con PID por ancho de pulso en
ventana de tiempo.

Proyecto académico de la materia de Circuitos.

---

## Estado

| Etapa | Estado |
| :--- | :--- |
| Análisis del circuito original | Hecho |
| Revisión técnica del diseño | Hecho — ver [revisión](docs/revision-tecnica.md) |
| Topología definida (motor con DC propia) | Hecho — ver [guía de armado](docs/guia-de-armado.md) |
| Firmware de control | Primera versión, sin probar en hardware |
| Prueba de banco del relé | Sketch listo — ver [`prueba_ssr`](firmware/prueba_ssr/prueba_ssr.ino) |
| Medición de resistencias | Pendiente — bloquea el cableado AC |
| Armado | Pendiente |
| Sintonización PID | Pendiente |

---

## Contenido

```
docs/
  documentacion_inicial_del_proyecto.md   Especificación original: componentes,
                                          análisis del circuito, cableado, mecánica.
  revision-tecnica.md                     Auditoría de esa especificación:
                                          4 fallos bloqueantes + 7 bugs del código.
  esquema-de-conexiones.md                Lista de materiales, tabla de conexiones
                                          terminal por terminal y verificaciones.
  esquema.svg                             Diagrama para imprimir.
  guia-de-armado.md                       Topología vigente (motor con fuente DC
                                          propia), cálculos y orden de armado.
firmware/
  popcorn_oven/popcorn_oven.ino           Firmware de control (Arduino IDE).
  prueba_ssr/prueba_ssr.ino               Prueba de banco del relé, sin termopar
                                          ni conexión a la red.
```

El esquema de cableado vigente es el de [`esquema-de-conexiones.md`](docs/esquema-de-conexiones.md).
El de la documentación inicial quedó superado.

---

## Antes de conectar nada a la red

Correcciones respecto al documento inicial. Están detalladas en
[`docs/revision-tecnica.md`](docs/revision-tecnica.md):

1. **A1** — El fusible térmico de 15 A va en serie con el ramal de calor; el ramal DC
   lleva su propio fusible de 1 A.
2. **A2** — *Resuelto* por la topología nueva: el motor ya no depende de las
   resistencias. En su lugar hay que medirlas y comprobar que su paralelo da 12 Ω o más.
3. **A3** — El MAX6675 se alimenta a **3,3 V**. A 5 V daña el GPIO19 del ESP32.
4. **A4** *(recomendado, no bloqueante)* — Pulldown de 10 kΩ entre GPIO23 y GND. Deja
   el pin en estado definido durante el boot; ver el análisis en la revisión.

El checklist completo de primer encendido está al final de la revisión técnica.

> Esto conmuta tensión de red con una resistencia de ~1 kW. Trabajá siempre con el
> equipo desenchufado y verificá continuidad antes de energizar.

---

## Conexiones

### Control (DC)

| MAX6675 (HW-550) | ESP32 |
| :--- | :--- |
| `VCC` | **3,3 V** (nunca 5 V) |
| `GND` | `GND` |
| `SCK` | `GPIO18` |
| `CS`  | `GPIO5` |
| `SO`  | `GPIO19` |
| `T+` / `T-` | Termopar tipo K, **junta aislada** |

| Relé de estado sólido | ESP32 |
| :--- | :--- |
| Terminal 3 `(+)` | `GPIO23` *(+ pulldown 10 kΩ a GND)* |
| Terminal 4 `(-)` | `GND` |

### Potencia (AC)

El motor sale por completo del circuito AC: se eliminan la resistencia-divisor y el
puente de diodos de su camino y se alimenta con fuente DC propia. Las dos resistencias
quedan en paralelo y el relé de estado sólido conmuta el tronco común.

```
[Fase] → [Interruptor] → ┬─→ [Fusible 1 A] → [Fuente 12 V] ─┬─→ [Motor del ventilador]
                         │                                   └─→ [Buck 5 V] → [ESP32]
                         │
                         └─→ [Fusible térmico 15 A] → [Bimetálico] → [Relé de estado sólido] → [Resistencia grande ∥ Resistencia pequeña] → [Neutro]
```

El ramal DC se toma antes del fusible térmico y del bimetálico, así que el ventilador
sigue soplando —y enfriando el cilindro— aunque cualquiera de los dos corte.

> **Antes de cablear:** medí ambas resistencias y verificá que
> el paralelo `(grande × pequeña) / (grande + pequeña)` da **12 Ω o más**. Por debajo de
> eso el relé ve más de 10 A. El cálculo está en la [guía de armado](docs/guia-de-armado.md).

---

## Firmware

### Librerías

Arduino IDE → Gestor de librerías:

- `MAX6675 library` (Adafruit)
- `PID` (Brett Beauregard, PID_v1)

Placa: *ESP32 Dev Module*.

### Comandos por puerto serie (115200 baudios)

| Comando | Efecto |
| :--- | :--- |
| `S220` | Fija el setpoint en 220 °C (rango 40–240) |
| `OFF`  | Detiene el calentamiento, sigue midiendo |
| `ON`   | Reanuda el control |

### Telemetría

```
[CALENTANDO] T=148.2 C  SP=200.0 C  Pot=72.4 %
```

### Seguridades en software

| Mecanismo | Acción |
| :--- | :--- |
| Termopar abierto o lectura fuera de rango (5 seguidas) | FALLO enclavado |
| `T ≥ 270 °C` | FALLO enclavado |
| 90 s al 100 % de potencia sin subir 5 °C | FALLO enclavado |
| Arranque | SSR en LOW antes que cualquier otra cosa; se valida el sensor antes de habilitar el PID |

Del estado FALLO solo se sale con reset físico del ESP32, a propósito.

Estas capas son **complementarias**, no sustitutas, del bimetálico y el fusible térmico.
Ninguna protección que dependa de firmware debe ser la única protección.

---

## Sintonización PID

Valores de partida: `Kp = 4.0`, `Ki = 0.05`, `Kd = 20.0`
(banda proporcional 25 °C, Ti = 80 s, Td = 5 s).

El procedimiento de ajuste por ciclo límite está en la
[sección C de la revisión técnica](docs/revision-tecnica.md).
