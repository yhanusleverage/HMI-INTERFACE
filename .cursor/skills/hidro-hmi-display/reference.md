# Referencia — HIDRO HMI Display (ISA-101 / HPHMI)

Resumen operativo para el skill `hidro-hmi-display`. No sustituye el estándar de pago ANSI/ISA-101.01; captura principios públicos aplicables a panel hidro 3.5".

## Estándares y lecturas

| Fuente | Uso |
|--------|-----|
| ANSI/ISA-101.01 (HMI Design) | Jerarquía de displays, color disciplinado, lifecycle |
| High Performance HMI (ASM / Hollifield) | Normal calmado; anormal imposible de ignorar |
| [ISA-101 cheat sheet (Industriant)](https://industriant.com/Industriant_ISA-101_Cheat_Sheet.pdf) | Checklist campo |
| [High Performance HMI overview](https://plcprogramming.io/blog/high-performance-hmi-isa-101) | Principios color / gray field |
| [Nuravine Aurora](https://nuravine.com/products/aurora) | Layout L1 3.5" (inspiración, no marca) |
| [docs.nuravine.com](https://docs.nuravine.com/) | Flujos calib / setup |

## Principios HPHMI → HIDRO

1. **Detectar anormal rápido** — color saturado solo en desviación.
2. **Normal visualmente quieto** — negro/navy + texto claro; sin “semáforo” permanente.
3. **Color ≠ identidad de proceso** — EC/pH no llevan color de marca; el rojo es BAJO/ALTO.
4. **Información > datos crudos** — valor + unidad + borde de estado; setpoint en L3.
5. **Consistencia** — misma nav, mismos botones, mismos tokens en todas las pantallas.
6. **Sin clutter** — sin 3D, gradientes, sombras, stickers, stats strips en el epicentro.

## Jerarquía L1–L4 (mapa)

```
L1 Monitoring 2x2
  ├─ [≡] L2 Menú
  │     ├─ Dosificacion → L3 canal (Dose/Hold/Stop)
  │     ├─ WiFi inicial (stub)
  │     ├─ Calibracion → L3 wizard
  │     ├─ Niveles → L3 ParamDetail (editable)
  │     ├─ Configuracion → Fecha/hora | Idioma
  │     └─ L4 Sistema
  └─ tap celda → L3 ParamDetail (solo lectura) → Calib
```

### Wireframe L1 (landscape 480×320)

```
| Monitoring          22.0 C      [≡] |
| EC [uS]          | pH               |
| 470              | 5.8              |
| ORP [mV]         | DO [mg/L]        |
| 362              | 9.1              |
```

- Temp solo en header (°C).
- OK: borde `gridLine` cyan; alarma: borde `alarm` + badge.

## Paleta (tokens código)

Ver `include/theme/AppTheme.h`:

| Token | Hex típico | Rol |
|-------|------------|-----|
| `bg` | `#000000` | Fondo estructural |
| `bgMid` | `#050D14` | Navy casi negro |
| `surface` | `#0A1218` | Filas menú |
| `surfaceAlt` | `#101C24` | Back / nav / hamburger |
| `text` | `#E8EEF2` | Valores / títulos |
| `muted` | `#6A858E` | Hints / labels |
| `gridLine` | `#70A9BE` | Borde celda OK |
| `accent` | `#2EC4B6` | CTA |
| `warn` | `#E8B84A` | Sistema |
| `alarm` | `#E85D5D` | Fuera de banda |

## Hardware / resolución

| Capa | Valor |
|------|-------|
| Panel | JC3248W535 AXS15231B **320×480** |
| UI LVGL | **320×480** portrait (**baseline**) |
| Stack | LVGL 8 + Arduino_GFX Canvas rot0 + PSRAM |
| DoD / freeze | `docs/DISPLAY_BASELINE.md` |
| Código display | `src/display/DisplayHal.cpp`, `TouchHal.cpp` |
| Pins | `include/BoardPins.h` (`LCD_H_RES/V_RES`, `LCD_CANVAS_ROT=0`) |

### Criterio de aceptación display

- Fondo negro completo (sin blanco residual).
- Cuatro celdas L1 siempre visibles y alineadas.
- Touch nativo; Serial `lvgl=320x480`, `canvas_rot=0`.
- Landscape software **congelado** hasta epic de producto.

## Archivos clave del repo

| Área | Path |
|------|------|
| Diseño humano | `docs/HMI_DESIGN.md` |
| UART / placa | `docs/HMI_UART.md` |
| Tema | `include/theme/AppTheme.h` |
| Semántica | `include/theme/HmiSemantics.h` |
| Controles | `include/theme/UiKit.h`, `src/ui/theme/UiKit.cpp` |
| Nav | `src/ui/NavShell.cpp` |
| Epicentro | `src/ui/screens/CentralParamsScreen.cpp` |
| Menú | `src/ui/screens/MenuScreen.cpp` |
