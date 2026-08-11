# Controle / Atlas — modelo Nuravine (HMI)

**Rules SI→ENTONCES→TIEMPOS** no están en el menú ni se evalúan (`RulesEngine::tick` off). Reglas de carga complejas → **web**.

**Master** = peer UART (bombas UI bomba1–6 / `dose` channel wire `R1`–`R6`). **Atlas** = slave ESP-NOW de cargas (`relay_slave`).

---

## 1. SETTINGS

| Fila | Rol |
|------|-----|
| **Controle** | **Alvo** (setpoint + banda muerta) · Auto EC/pH · Reservorio |
| **Atlas** | relé1…8 → Nombre \| Accionamiento → ON/OFF · Ciclo · Timer |
| **Dosificación** | Nutrientes \| Manual \| pH Up \| pH Down (Master) |

---

## 2. Atlas

- Solo ESP-NOW (`!local`). Menú **Atlas**.
- Lista UI: **relé1…relé8** (solo tag). Nombre = pantalla aparte.
- Sin ESP-NOW en inventario: placeholder MAC `"atlas"` · Offline · 8 slots (nombres NVS). Al conectar Atlas, alias migran al MAC real.
- Clave `(mac, relayIndex 0..7)` · NVS `ra_*` / ciclo `rc_*`.
- ON/OFF → `relay_slave` solo con MAC real online. Ciclo no dispara con placeholder.
- **No** Master `local` R1–R8 aquí.
- `DATA_SOURCE_SIM` no inventa Atlas (solo sensores).

No mezclar con Dosificación (`dose`) ni Controle (SP / Auto).

---

## 3. Rules (web / legacy)

Sin UI Rules. `RulesConfig` / `RulesEngine` pueden quedar por NVS; sin tick. L1–L4 sump → web.

UART: [`HMI_UART.md`](HMI_UART.md).

---

## 4. Checklist

1. Atlas → reléN → Nombre (offline OK) · Accionamiento → ON/OFF si online.  
2. Controle → Alvo + Auto + Reservorio.  
3. Dosificación → Nutrientes / Manual / pH Up / pH Down.  
4. Reglas complejas → web.
