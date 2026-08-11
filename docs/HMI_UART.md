# HMI IPS — JC3248W535 3.5" + UART

## Placa

| Ítem | Valor |
|------|--------|
| Módulo | **Guition / JCZN JC3248W535** |
| Tamaño | **3.5" IPS** |
| Resolución | **UI landscape 480×320** (panel físico 320×480; ver [`DISPLAY_BASELINE.md`](DISPLAY_BASELINE.md)) |
| Driver | **AXS15231B** por **QSPI** (+ Canvas en PSRAM) |
| Backlight | GPIO **1** |
| QSPI | CS45 SCK47 D0=21 D1=48 D2=40 D3=39 |
| Board IDE | ESP32S3 Dev Module |
| Core | Arduino-ESP32 **~3.0.2** (pioarduino 51.03.04) |

**Nota:** el PDF **JC4827W543** es otro módulo (4.3" / 480×272 / NV3041A). No mezclar drivers.

**PSRAM:** obligatorio (Canvas panel 320×480×2 B → UI lógica 480×320). Sin PSRAM el display no inicia.

Demos PIC/MJPEG/MP3 del vendor pueden exigir TF card; este HMI LVGL **no**.

## USB-C

Flasheo + Serial CDC 115200 (`COM5`).

## UART al master (HIDROWAVE)

| Señal | S3 (HMI) | Master ESP32 |
|-------|----------|--------------|
| TX | GPIO 17 | RX GPIO 17 |
| RX | GPIO 18 | TX GPIO 18 |
| GND | GND | GND |

Baud **115200**. Verdad = master: [`ESP-HIDROWAVE-main/docs/HMI_BRIDGE.md`](../../ESP-HIDROWAVE-main/docs/HMI_BRIDGE.md).  
Display: `-D DATA_SOURCE_SIM=0`.

Telemetry (JSON por línea):

```json
{"t":"telemetry","ph":5.8,"ec":470,"temp_agua":22.0,"orp":362,"do":9.1}
```

Campos `orp` y `do` son opcionales; si faltan se conservan los valores actuales.

### Dosificación manual (display → master)

```json
{"t":"cmd","action":"dose","channel":"R1","ml":5.0}
{"t":"cmd","action":"dose","channel":"R2","ml":2.0}
{"t":"cmd","action":"dose_stop","channel":"R1"}
{"t":"cmd","action":"dose_hold","channel":"R3","on":1}
```

Canales wire: **`R1` … `R6`**. UI del display: **bomba1 … bomba6** (peristáltica; EN `pump1`…). El HMI solo envía; las bombas las ejecuta el master. También se refleja en USB Serial como `[UART TX] ...`.

### Nutrientes proporcionales + pH (display → master)

Lista **vacía por defecto**. Solo ítems que el usuario crea en Nutrientes (nombre + ml/L + relayNumber 1–6).  
`proportion = mlPerLiter / Σ mlPerLiter`.

```json
{"t":"cmd","action":"nutrient_proportions","totalMlPerLiter":12.0,"nutrients":[
  {"name":"Grow","relayNumber":1,"mlPerLiter":5.0,"proportion":0.4167,"active":true},
  {"name":"Micro","relayNumber":2,"mlPerLiter":5.0,"proportion":0.4167,"active":true},
  {"name":"Bloom","relayNumber":3,"mlPerLiter":2.0,"proportion":0.1667,"active":true}
]}
```

Sin nutrientes: `nutrients: []`, `totalMlPerLiter: 0`. Se reenvía al editar Nutrientes. El master debe mapear `action=nutrient_proportions` a `HydroControl::updateNutrientProportions`.

### Malha fechada / reservorio (display → master)

Parámetros de Controle → Reservorio / Auto (volumen, homogenización, gaps, pulsos, Auto EC/pH). Se emite al editar y en `begin()` tras `MasterLink::begin()`.

```json
{"t":"cmd","action":"loop_control",
 "volumeL":50.0,
 "homoSec":60,
 "nutrientGapSec":3,
 "pulseMl":2.0,
 "pulseGapSec":2,
 "dosingDelaySec":60,
 "dosingMode":"batch",
 "dosingArmed":false,
 "autoEc":false,"autoEcIntervalSec":30,
 "autoPh":false,"autoPhIntervalSec":30,
 "maxStepEc":0.5,"maxStepPh":0.5,
 "dosingConstEc":0,"dosingConstPh":0,
 "phUpRelay":0,"phDownRelay":0,
 "consumoDiario":false,
 "consumoPh24h":false,
 "ecLo":300,"ecHi":800,
 "phLo":5.5,"phHi":6.5}
```

`dosingMode`: `"batch"` \| `"recirc"`. Sin `dosingArmed`, `autoEc`/`autoPh` salen en false aunque estén ON en UI.  
`maxStepEc`/`maxStepPh` = **agresividad** Auto (fracción **0.0…1.0**). En UI: **0…100 %**, ±10 %, sin teclado; HMI envía `pct/100`. α de ajuste de periodo Consumo 24 h.  
`dosingConstEc`/`Ph` el HMI envía siempre **0** (default Master; no hay UI de constantes). El lazo corre en el master. **Calibration dose** queda pendiente en master+HMI.  
`phUpRelay` / `phDownRelay`: bomba Master **1…6** (UI bomba1–6) para Auto pH ( **0** = sin asignar). Se editan en Dosificación → **pH Up / pH Down** (no en Nutrientes).  
`ecLo`/`ecHi` · `phLo`/`phHi`: banda Alvo → **deadband de control** en Master (nunca 50 µS fijo como diseño). Se reenvía `loop_control` al Guardar Alvo EC **o** pH.  
`consumoDiario`: capa **Consumo EC 24h** sobre Auto EC (ventana `EC_t0` vs ahora → nutrir/diluir). No cambia `autoEcInterval`.  
`consumoPh24h`: capa **Consumo pH 24h** sobre Auto pH (ventana `pH_t0` vs ahora → Up/Down). Independiente de EC; no cambia `autoPhInterval`.  
Detalle Master: [`IMPLEMENTACAO_REAL_MASTERLINK_ESPHIDROWAVE.md`](IMPLEMENTACAO_REAL_MASTERLINK_ESPHIDROWAVE.md).
### Atlas — ESP-NOW (display → Master UART → Atlas)

`MasterLink` = UART HMI↔**Master**. **Atlas** = slave ESP-NOW (no peer `local` de bombas).

Inventario: botón **Actualizar** en Atlas (o al abrir pantallas que lo pidan). `RulesEngine::tick` **no** corre.

```json
{"t":"cmd","action":"slaves_req"}
```

Respuesta Master → HMI:

```json
{"t":"slaves","slaves":[
  {"mac":"local","name":"Master","local":true,"online":true,"numRelays":8},
  {"mac":"AA:BB:CC:DD:EE:FF","name":"Atlas","local":false,"online":true,"numRelays":8}
]}
```

UI Atlas: lista **relé1…relé8** (tag solo). Sin entradas ESP-NOW: placeholder MAC `"atlas"` offline (nombres NVS).

Accionamiento por relé (ON/OFF · Ciclo · Timer). `duration` en s; `0` = ON forever.

```json
{"t":"cmd","action":"relay_slave","mac":"AA:BB:CC:DD:EE:FF","relay":0,"state":"on","duration":5}
{"t":"cmd","action":"relay_slave","mac":"AA:BB:CC:DD:EE:FF","relay":0,"state":"off","duration":0}
```

`relay` 0..7. Bombas Master (UI **bomba1–6**; wire `dose` channel `R1`–`R6` / `nutrient_proportions`). Detalle: [`HMI_RULES.md`](HMI_RULES.md).

### sys_info — identidad cloud del Master (solo lectura)

El **registro Supabase es del Master** ([`CLOUD_REGISTER.md`](CLOUD_REGISTER.md)). El HMI solo muestra `device_id` / `cloud_ok` en System.

HMI → Master:

```json
{"t":"cmd","action":"sys_info_req"}
```

Master → HMI (cuando el bridge UART lo implemente):

```json
{"t":"sys_info","device_id":"ESP32_HIDRO_XXXXXX","cloud_ok":true,"process_bridge":true}
```

`cloud_ok` = Master con WiFi + última telemetría/registro cloud OK. Sin respuesta, System muestra `Master: --`.

### cmd_ack — confirmación de proceso

Tras `dose` / `loop_control` / `nutrient_proportions` / `relay_*` / `setpoint` / `calib` / `slaves_req`, el Master responde:

```json
{"t":"cmd_ack","action":"dose","ok":true,"commandId":42}
```

HMI: System muestra **Proceso UART: OK** cuando llega `cmd_ack` ok o `process_bridge` en sys_info. Password WiFi no se imprime en Serial (`***`).

SoftAP Master (vía A): SSID `ESP32_Hidropônico`, clave **`hidrosetup`**.

## Diagnóstico Serial

Selftest ROJO→VERDE→AZUL→BLANCO a pantalla completa. Comandos: `help`, `test`, `status`, `view`, `mirror on|off`, `menu`, `back`.

`status` debe mostrar `lvgl=480x320`, `canvas_rot=1` y `touch ready=1` si el I2C 0x3B responde (baseline producto — [`DISPLAY_BASELINE.md`](DISPLAY_BASELINE.md)).

## Pantalla partida / ruido RGB

Suele ser **driver/resolución incorrectos** (p. ej. firmware NV3041A 480×272 en esta 3.5"). Con AXS15231B + Canvas + `flush` en el último dirty rect de LVGL, la imagen debe cubrir toda la pantalla.
