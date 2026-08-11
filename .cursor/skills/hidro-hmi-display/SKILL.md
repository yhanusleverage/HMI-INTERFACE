---
name: hidro-hmi-display
description: >-
  HIDRO HMI display specialist for LVGL situational dark UI on JC3248W535
  (portrait baseline 320x480). Applies ISA-101 / High Performance HMI: calm normal
  state, alarms only via color semantics, Level 1-4 menu hierarchy, AppTheme
  / UiKit / HmiSemantics only. Display DoD in docs/DISPLAY_BASELINE.md — landscape
  software frozen. Use when working on Monitoring, menus, theme, NavShell,
  DisplayHal, touch, calibration/dosing screens, epicentro 2x2, or dark theme.
---

# HIDRO HMI Display — agente situacional dark

Leé primero [`docs/HMI_DESIGN.md`](../../docs/HMI_DESIGN.md), [`docs/HMI_SITUATIONAL_FOUNDATIONS.md`](../../docs/HMI_SITUATIONAL_FOUNDATIONS.md) (ISA-101 / por qué la UI está organizada así) y [`reference.md`](reference.md).  
Código de verdad: `include/theme/AppTheme.h`, `HmiSemantics.h`, `theme/UiKit.*`, `src/ui/NavShell.cpp`, `CentralParamsScreen.cpp`.

## Misión (≤ 3 segundos)

La UI es **conciencia situacional**, no app consumer. El operador debe responder:

1. ¿El proceso está OK?
2. ¿Qué parámetro está mal?
3. ¿Qué acción tomo?

**Una pregunta por pantalla.** No mezclar Monitoring + setup + diagnóstico en la misma vista.

## Jerarquía ISA-101 → HIDRO

| Nivel | Pantalla | Contenido |
|-------|----------|-----------|
| L1 | Monitoring 2×2 | Epicentro EC \| pH / ORP \| DO; temp en header |
| L2 | Menú | Dosificación, WiFi, Calibración, Niveles, Configuración, Sistema |
| L3 | Detalle / Dose / Calib | Una acción o un parámetro |
| L4 | Sistema | FW, resolución, UART, uptime |

Navegación fija: hamburguesa → Menú; siempre **Atras** (`UiKit::styleHeader` / `makeBackButton`).

## Color = semántica (no negociable)

- **OK / normal** → neutro (`AppTheme::text` / `muted`). Nunca pintar “todo verde”.
- **Alarma proceso** (BAJO/ALTO) → `alarm` solo en **borde de celda** y badge (`HmiSemantics`).
- **Warn sistema** (UART down, reset) → `warn` ámbar; canal distinto a alarma de sensor.
- **CTA** → `accent` aqua; texto sobre CTA oscuro.
- **SIM / LIVE** → no badge SIM en pantallas de operador.
- Prohibido: colores ad-hoc por pantalla, glow, púrpura-on-white, cards decorativas, 3D, sombras.

Todo color/layout nuevo → `AppTheme` + `HmiSemantics` + `UiKit`. No inventar hex en screens.

## Dark industrial (panel embebido)

- Fondo estructural `#000000` (`bgPlate` en `NavShell` + `UiKit::styleScreen` **opaco** `LV_OPA_COVER`).
- Surfaces navy (`surface` / `surfaceAlt`) para filas y nav.
- Bordes grid cyan (`gridLine`, p.ej. `#70A9BE`) en celdas OK.
- Tipografía: valor Montserrat 28; título/header 16; labels 12–14 muted/text.

## Menús situacionales

- Listas: solo `UiKit::makeMenuRow` + `styleHeader(title, onBack)`.
- Targets táctiles ≥ ~44 px (`TOUCH_MIN_H` / `BTN_PRIMARY_H`).
- CTA primaria abajo; Hold/Stop/Dose = acciones claras, una por control.

## Display pipeline (baseline — no romper el epicentro)

- UI + panel **portrait 320×480** nativo (`LCD_CANVAS_ROT=0`). Ver `docs/DISPLAY_BASELINE.md`.
- Criterio de aceptación visual obligatorio (DoD):
  - Fondo negro en toda la pantalla
  - **4 celdas** visibles: EC \| pH / ORP \| DO
  - Header Monitoring + temp + menú; touch abre Menú
- **Landscape software congelado** — no reintroducir soft-rotate / sw_rotate / canvas rot≠0 sin epic aislado y DoD verde.
- Touch = coords nativas del panel.

## Flujo de trabajo al tocar UI

1. Confirmar nivel L1–L4 y la **una** pregunta de la pantalla.
2. Reusar `UiKit` / `AppTheme` / `HmiSemantics`; no clonar estilos.
3. Monitoring: grid 2×2 fijo sin solapes; labels con clip; header opaco.
4. Tras cambios de display: verificar criterio de las 4 celdas + dark.
5. Actualizar `docs/HMI_DESIGN.md` si cambia navegación, paleta o pipeline.

## Checklist (cerrar tarea solo si pasa)

- [ ] ¿Usé `AppTheme` / `UiKit` / `HmiSemantics`?
- [ ] ¿OK queda neutro? ¿Alarma solo borde/badge?
- [ ] ¿Dark real (negro), sin blanco residual?
- [ ] ¿Monitoring 2×2 con 4 parámetros legibles en 320×480 (DoD)?
- [ ] ¿Menú / Atras / hamburguesa predecibles?
- [ ] ¿Sin badge SIM en operador?
- [ ] ¿Sin decoración (glow, cards, purple theme)?

## Fuera de alcance de este skill

- Dosificación automática PID; cloud Nuravine; WiFiManager real.
- Clonar marca/logo Nuravine (solo layout y semántica).
