# Onboarding — primera vez HIDRO HMI

Registro cloud del Master: wizard HMI (vía B preferida con display) o SoftAP (vía A). Ver [`CLOUD_REGISTER.md`](CLOUD_REGISTER.md).

## Primera vez (`setup_done=false`)

```
Energía
  → Bienvenida (~2 s o Continuar)
  → Idioma
  → Reservorio           volumen / mezcla (base malha)
  → Ciudad o región      (fuso; Skip OK)
  → WiFi                 (4/5; Skip OK)
  → Dispositivo          (5/5 email/nome/local; Skip OK)
  → Listo
  → Monitoring           (setup_done=true)
```

Fuso por **ciudad/región**, no por UTC.

**WiFi wizard 4/5:** SSID + clave en draft HMI (sin UART). **5/5 Continuar/Skip:** un solo `commitProvision` → `wifi_config` UART con red + email/nome/location → Master NVS + reboot. Skip en 4/5 = sin red en draft → 5/5 no manda UART (SoftAP más tarde: SSID `ESP32_Hidropônico` / clave `hidrosetup`). Email/nome/location = solo paso 5/5.

## Reinicios

Sin wizard → Monitoring. Idioma / WiFi / TZ en NVS; WiFi + NTP en background.

## Edición posterior

SETTINGS → Ajuste (WiFi · Idioma · TZ · Retroiluminacion · Reset fábrica) · Controle → Reservorio · System.

WiFi en Ajuste = mismo `WifiIntroLayout` que el wizard (solo SSID/clave; perfil cloud solo en paso 5/5).

**Reset fábrica:** borra NVS HMI (`hidro_hmi` + `wifi_cfg`) **y** manda `factory_reset` al Master (limpia `hydro_system`). Firmware/SPIFFS intactos. Wizard de nuevo.

## Código

| Pieza | Rol |
|-------|-----|
| `Welcome` / `Ready` | Extremos del wizard |
| `MasterWifi` | Paso WiFi 4/5 + Ajuste (solo red, `WifiIntroLayout`) |
| `MasterWifiProfile` | Paso dispositivo 5/5 (email/nome/local; solo wizard) |
| `Reservoir` | Paso malha en 1ª vez + Controle |
| NVS `setup_done` | Fin del sendero |
| NVS `tz_min` | Offset interno |
