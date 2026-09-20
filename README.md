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
| Firmware de control | Primera versión, sin probar en hardware |
| Correcciones de cableado (A1–A4) | Pendientes |
| Sintonización PID | Pendiente |

---

## Contenido

```
docs/
  documentacion_inicial_del_proyecto.md   Especificación original: componentes,
                                          análisis del circuito, cableado, mecánica.
  revision-tecnica.md                     Auditoría de esa especificación:
                                          4 fallos bloqueantes + 7 bugs del código.
firmware/
  popcorn_oven/popcorn_oven.ino           Firmware de control (Arduino IDE).
```

---

## Antes de conectar nada a la red

Hay **cuatro** correcciones bloqueantes respecto al documento inicial. Están detalladas
en [`docs/revision-tecnica.md`](docs/revision-tecnica.md):

1. **A1** — El fusible térmico de 15 A va en el tronco común, no después del SSR.
2. **A2** — Verificar con multímetro si las dos resistencias son independientes o si son
   una sola bobina derivada. Si son derivadas, el ventilador se para al regular.
3. **A3** — El MAX6675 se alimenta a **3,3 V**. A 5 V daña el GPIO19 del ESP32.
4. **A4** — Pulldown de 10 kΩ entre GPIO23 y GND, o el SSR puede dispararse durante el
   boot del ESP32.

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

| SSR-25DA | ESP32 |
| :--- | :--- |
| Terminal 3 `(+)` | `GPIO23` *(+ pulldown 10 kΩ a GND)* |
| Terminal 4 `(-)` | `GND` |

### Potencia (AC)

```
[Fase] → [Interruptor] → [Fusible térmico 15 A] → [Bimetálico] → ┬─→ [R pequeña] → [Puente diodos] → [Motor] → [Neutro]
                                                                 └─→ [R grande] → [SSR 1|2] → [Neutro]
```

El ramal del motor queda permanentemente alimentado mientras el interruptor esté
cerrado, de modo que el ventilador no se detiene cuando el PID recorta la resistencia.
**Esto depende de que se cumpla A2.**

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
