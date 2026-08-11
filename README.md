# HIDRO HMI — JC3248W535 (3.5")

Firmware de **solo display** para **ESP32-S3 + IPS 3.5" 320×480 (AXS15231B QSPI)**.  
Muestra **pH**, **EC** y **temp agua**; setpoints y calibración. Sensores/actuación en el ESP32 master (UART).

## Importante

- Placa objetivo: **JC3248W535** (3.5"), **no** JC4827W543 (4.3").
- **PSRAM OPI** obligatorio (Canvas).
- USB-C = flasheo + debug; enlace al master = UART.

## Build

```bash
pio run -e esp32-s3-hmi -t upload
pio device monitor
```

Puerto: `COM5` en `platformio.ini`.

## Docs

- [docs/HANDOFF.md](docs/HANDOFF.md) — estado HMI↔master
- [docs/HMI_NAV_MAP.md](docs/HMI_NAV_MAP.md) — menús / ScreenId
- [docs/HMI_OPERATOR_GUIDE.md](docs/HMI_OPERATOR_GUIDE.md) — operador
- [docs/HMI_UART.md](docs/HMI_UART.md) — JSON UART
- [docs/HMI_DESIGN.md](docs/HMI_DESIGN.md) — tema / Monitoring
- [docs/HMI_RULES.md](docs/HMI_RULES.md) — Controle · Relés Atlas
- [docs/ONBOARDING.md](docs/ONBOARDING.md) — wizard 1ª vez
- [docs/DISPLAY_BASELINE.md](docs/DISPLAY_BASELINE.md) — DoD display
