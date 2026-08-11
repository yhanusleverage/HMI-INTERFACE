# WiFi setup — HIDRO HMI

## Objetivo

Scan → SSID + clave (+ opcionales SoftAP) → **Master** (`wifi_config` UART) + **display** (`WifiConfig` NVS / STA / NTP). SoftAP del Master = fallback sin display.

## Orden setup (1ª vez)

```
Bienvenida → Idioma → Reservorio → Zona horaria → WiFi (4/5) → Perfil cloud (5/5) → Listo → Monitoring
```

En wizard: **Continuar** / **Saltar** (o **OK** del teclado = Continuar). Ver [`ONBOARDING.md`](ONBOARDING.md) y [`CLOUD_REGISTER.md`](CLOUD_REGISTER.md).

Confirmar red (Continuar u OK del teclado) en 4/5 o Ajuste aplica `WifiConfig::save` / UART y `WiFi.begin`; 5/5 solo añade perfil cloud.

## UX (landscape 480×320)

1. Header: wizard = Continuar/Skip/Buscar; Ajuste = Atras + Continuar/Buscar
2. Status + lista RSSI / panel red elegida
3. Clave **visible** (fuente grande); email/nome/location **solo** wizard 5/5
4. Teclado dark al tocar la clave; **OK** = misma acción que Continuar (Cancel solo cierra teclado)

## Código

| Pieza | Rol |
|-------|-----|
| `WifiConfig` | NVS HMI + scan + STA / NTP |
| `MasterWifiScreen` | Wizard 4/5 + Ajuste vía `WifiIntroLayout` (solo red) |
| `WifiIntroLayout` | UI compartida scan / SSID / clave visible |
| `WifiSetupScreen` | legado — `ScreenId::WifiSetup` redirige a `MasterWifi` |
| `MasterLink::sendWifiConfig` | UART al Master |

## NVS `wifi_cfg` (HMI)

| Key | Rol |
|-----|-----|
| `ssid` | SSID |
| `pass` | Clave |
