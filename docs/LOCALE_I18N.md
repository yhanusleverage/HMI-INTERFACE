# Locale e i18n — HIDRO HMI

## Reloj = ciudad/region + NTP

La hora del HMI sale de **ciudad o region** (offset fijo) + **NTP** tras WiFi:

| API | Rol |
|-----|-----|
| `tzPresetLabel` / `tzPresetMin` | Lista LatAm-first (Brasilia, Manaus, Mexico City…) |
| `tzOffsetMin` / `setTzOffsetMin` | Offset en NVS `tz_min` (interno) |
| `formatTzLabel` | Nombre de ciudad para Sistema / UI |
| `formatNowClock` | Header Monitoring `HH:MM` |

El usuario **no** ve `UTC-3`: ve “Brasilia / Sao Paulo”. Sin DST automatico (offset fijo).

Configuración → **Ajuste** → **Ciudad o región** + idioma.

## Idiomas (ES / EN / PT)

NVS `lang` en `hidro_hmi`. Al cambiar: `setLanguage` + `NavShell::reloadUi()`.

Catálogo: `AppStrings.h` / `AppStrings.cpp` (nunca `Strings.h` en Windows).

## Wizard 1ª vez

`!setupDone` → Welcome → Language → WiFi → **Reservoir** → TimeZone (ciudad) → Ready → Central.

Ver [`ONBOARDING.md`](ONBOARDING.md).

## NVS `hidro_hmi`

| Key | Tipo | Default |
|-----|------|---------|
| `lang` | u8 | ES |
| `tz_min` | i32 | -180 (Brasilia) |
| `setup_done` | bool | false |
