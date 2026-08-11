# HMI Design — ISA-101 / High Performance HMI (estilo Nuravine)

Guía interna del equipo para pantallas del **HIDRO HMI** (JC3248W535 3.5", UI lógica **landscape 480×320**, panel físico 320×480, LVGL 8).

**Fuente de verdad operativa del agente:** skill [`.cursor/skills/hidro-hmi-display/`](../.cursor/skills/hidro-hmi-display/SKILL.md).  
**Baseline display / DoD:** [`docs/DISPLAY_BASELINE.md`](DISPLAY_BASELINE.md).  
**Fundamentos ISA-101 / validación situacional:** [`HMI_SITUATIONAL_FOUNDATIONS.md`](HMI_SITUATIONAL_FOUNDATIONS.md).

Referencias de producto (inspiración layout, no clon de marca):
- [Nuravine Aurora](https://nuravine.com/products/aurora) — snapshot pH / EC / temp / DO / ORP en 3.5"
- [docs.nuravine.com](https://docs.nuravine.com/) — monitoring, calibración, setup
- Videos útiles: “Nuravine Setup Tutorial: Aurora Menu”, “Nuravine Sensor Calibration Tutorial”

Referencias HMI: **ISA-101** + High Performance HMI (ASM / Hollifield) — detalle y mapa de validación en [`HMI_SITUATIONAL_FOUNDATIONS.md`](HMI_SITUATIONAL_FOUNDATIONS.md).  
Código: `include/theme/AppTheme.h`, `HmiSemantics.h`, `UiKit.h`.

**Fondo estructural:** `bgPlate` permanente en `NavShell` (`#000000`). Las pantallas usan fondo opaco vía `UiKit::styleScreen`; el chrome (header/filas/botones) se pinta encima vía `UiKit`.

---

## 1. Objetivo

La UI existe para **conciencia situacional**, no para verse “tipo app”.

En ≤ 3 segundos el operador debe responder:

1. ¿El proceso está OK?
2. ¿Qué parámetro está mal?
3. ¿Qué acción tomo?

---

## 2. Principios (no negociables)

| Principio | Regla práctica |
|-----------|----------------|
| Color = semántica | Color solo para estado / precaución. No decorar. |
| OK es neutro | Proceso normal → texto blanco / muted. No pintar todo de verde. |
| Alarma destaca | BAJO/ALTO → rojo en **borde de celda** (y badge en detalle). |
| Una pregunta por pantalla | Monitoring / menú / detalle / acción / sistema. |
| Targets táctiles | Altura mínima ~36 px en landscape; botones primarios ~40 px. |
| Navegación predecible | Hamburguesa → menú; siempre **Atras**; CTA primaria abajo. |
| Modos separados | SIM / LIVE / UART ≠ alarma de sensor. |
| Consistencia | Usar `UiKit` + `AppTheme`; no inventar colores por pantalla. |

---

## 3. Paleta — dark negro + navy + verde agua

### A) Superficie / tipografía

| Token | Hex | Uso |
|-------|-----|-----|
| `bg` | `#000000` | Fondo (negro real) |
| `bgMid` | `#050D14` | Fondo (navy casi negro) |
| `surface` | `#0A1218` | Filas de menú |
| `surfaceAlt` | `#101C24` | Nav, back |
| `text` | `#E8EEF2` | Valores y títulos |
| `muted` | `#6A858E` | Labels |
| `gridLine` | `#70A9BE` | Bordes celdas OK |
| `accent` | `#2EC4B6` | CTA / verde agua |

UI lógica **landscape 480×320** (Canvas rot 1; ver `DISPLAY_BASELINE.md`). Fondo negro real vía `bgPlate`. **Sin badge SIM.**

### C) Estado de proceso

| Token | Hex | Cuándo |
|-------|-----|--------|
| `warn` | `#E8B84A` | Precaución de sistema (UART down, reset calib) |
| `alarm` | `#E85D5D` | Parámetro fuera de banda (BAJO/ALTO) |

### D) Modo / enlace

| Token | Hex | Uso |
|-------|-----|-----|
| `simBadge` | `#5B8DEF` | Fuente SIM |
| `liveBadge` | `#7FA8B0` | (solo espejo Serial; no en UI) |

---

## 4. Epicentro Central (wireframe)

```
+----------------------------------+
| 22°C · [INACTIVO] · HH:MM · [≡] |
+----------------+-----------------+
| EC [uS]   BAJO | pH              |
| 470            | 5.8             |
+----------------+-----------------+
| ORP [mV]       | DO [mg/L]       |
| 362            | 9.1             |
+----------------+-----------------+
```

- **Sin título** de pantalla: flag header = `INACTIVO` \| `ACTIVO` (solo lectura de `dosingArmed`). Armar/desarmar en Controle → Auto. Cadeado UI aparte (`autoUiLocked`).
- **Temp agua** solo en el header (°C).
- Tap **EC/pH** → `DosingInfo` (solo lectura). ORP/DO sin tap. Edición de setpoint en **Controle → Alvo**.
- Hamburguesa (3 líneas) → **Menú**.
- BAJO/ALTO: borde rojo + badge en celda; OK: borde cyan `#70A9BE`.
- **Valores** EC/pH/ORP/DO: fuente bitmap `lv_font_montserrat_num_60` (solo `0123456789.-`). **Prohibido** `transform_zoom` en el epicentro (clip / valores que “desaparecen”).

---

## 5. Mapa de menús navegables

**Esquemático completo:** [`HMI_NAV_MAP.md`](HMI_NAV_MAP.md).

```
Monitoring 2x2
  ├─ [≡] SETTINGS
  │     ├─ Controle
  │     │     ├─ Alvo            ← Setpoint + banda muerta (±)
  │     │     ├─ Auto EC/pH · Consumo EC 24h · Consumo pH 24h
  │     │     └─ Reservorio      ← volumen · homo · Batch|Recirc · gaps
  │     ├─ Atlas                 ← relé1…8 → Nombre · Accionamiento
  │     ├─ Dosificación          ← Nutrientes | Manual | pH Up/Down → PhPumpActions (Relé + hub bomba)
  │     ├─ Ajuste                ← WiFi · Idioma · TZ · Retroiluminacion · Reset
  │     ├─ Sistema
  │     └─ Sensors               ← Calib · Display · Units (sin Alvo)
  └─ tap EC/pH → DosingInfo (solo lectura)
```

| Pantalla | Contenido |
|----------|-----------|
| **Central** | Header (temp · arm · clock · ≡) + grid 2×2; alarma en borde |
| **Controle** | Alvo · Auto EC/pH · Reservorio |
| **Alvo** | Lista parámetros; setpoint + banda muerta |
| **DosingInfo** | Tap EC/pH en L1: rangos + total ml/L |
| **ParamDetail** | Desde Alvo: edición setpoint/banda |
| **Ajuste** | WiFi · idioma · zona horaria · retroiluminacion · reset fabrica |
| **Sistema** | FW, resolución, UART, uptime |

**Regla:** setpoints / banda muerta **solo** se editan en `SETTINGS → Controle → Alvo`.

---

## 6. Jerarquía tipográfica

1. **Valor** (epicentro L1) — Montserrat numérico **60** (`lv_font_montserrat_num_60`), `text` (también `--`); sin zoom
2. **Título / header** — Montserrat 16, `text`
3. **Temp header** — Montserrat 20 (sin cambios)
4. **Label de celda** — Montserrat 16, `muted` (`EC [uS]`, etc.)
5. **Hints** — Montserrat 12–14, `muted`

---

## 7. Layout y toque

Constantes en `AppTheme` (viewport landscape 480×320 compacto):

- `HEADER_H` / `BACK_H` 44, `BACK_W` 100, `PAD` 8, `MENU_LIST_TOP` = `BACK_H + PAD`  
- `BTN_PRIMARY_H` 48  
- `TOUCH_MIN_H` 44, `TOUCH_MIN_W` 48, `MENU_ROW_H` 48  
- Menú: tipografía Montserrat **20**; títulos **20**; botones nav **16**; chevron `LV_SYMBOL_RIGHT`  
- `CARD_RADIUS` 0 (bordes rectos tipo panel industrial)  
- Borde idle 1 px; alerta 2 px  

`accent` solo en CTA primaria. Selección / OK nunca usan verde semáforo ni accent como “estado bueno”.

---

## 8. Checklist al tocar UI

- [ ] ¿Usé `AppTheme` / `UiKit` / `HmiSemantics`?
- [ ] ¿El OK queda neutro?
- [ ] ¿Alarma solo en borde/badge?
- [ ] ¿Degradé dark/navy + accent verde agua visible?
- [ ] ¿Monitoring 2×2 legible en landscape 480×320 (DoD `DISPLAY_BASELINE.md`)?
- [ ] ¿Touch abre Menú desde `[=]`?
- [ ] ¿Sin badge SIM en pantallas de operador?

---

## 9. Fuera de alcance v1

- Dosificación **automática** por setpoint/PID (manual sí está en menú)
- Firmware bombas en el master (solo contrato UART)
- Cloud Nuravine  
- WiFiManager real (solo stub NVS)  

---

## 10. Extender el sistema

1. Nuevo color → `AppTheme` con canal (superficie / CTA / proceso / modo).
2. Nueva regla de estado → `HmiSemantics`.
3. Nuevo control → `UiKit`.
4. Actualizar **esta** doc en el mismo cambio.
