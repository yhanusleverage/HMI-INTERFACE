# Display baseline — Definition of Done

**Estado:** baseline de producto = **landscape lógico 480×320** vía `Arduino_Canvas` rotación **1**, **`LCD_IPS=0`**, tema **dark validado**.  
Panel físico AXS15231B: **320×480**.

## Método único (no mezclar)

| Capa | Valor |
|------|--------|
| Panel | AXS15231B `PANEL_W×PANEL_H` = 320×480 |
| Canvas | `Arduino_Canvas(PANEL_W, PANEL_H, panel, 0, 0, 1)` → logical 480×320 |
| LVGL | `hor_res=480`, `ver_res=320` |
| IPS | **`LCD_IPS=0`** (con 1 los colores se invertían en esta placa) |
| Flush | `draw16bitRGBBitmap` + `flush()` last |
| Touch | remap: `lx=py`, `ly=(PANEL_W-1)-px` |

## Dark (cerrado — éxito)

1. Canvas `fillScreen(BLACK)` + `flush` antes de LVGL  
2. Draw buffer a cero + `full_refresh=1`  
3. `lv_scr_act` negro opaco antes de UI  
4. `UiKit::forceOpaqueBg(..., AppTheme::bg())` en raíz / plate / host / celdas  
5. `LCD_IPS=0`

## DoD display

Tras upload en JC3248W535:

1. Fondo **negro** estable  
2. Monitoring 2×2 landscape  
3. Menú + WiFi (SSID/clave NVS)  
4. Serial: `lvgl=480x320`, `canvas_rot=1`, `ips=0`

## Portrait (fallback histórico)

| Flag | Valor |
|------|--------|
| `LCD_H_RES` / `LCD_V_RES` | 320 / 480 |
| `LCD_LANDSCAPE` | 0 |
| `LCD_CANVAS_ROT` | 0 |
| Touch | nativo |

Usar solo si landscape regresa white/invert; revalidar dark.

## Freeze

No: `sw_rotate`, soft-rotate pixel-loop, Canvas 480×320 como tamaño de fb, `LCD_IPS=1` sin prueba Serial.
