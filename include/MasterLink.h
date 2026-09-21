#ifndef MASTER_LINK_H
#define MASTER_LINK_H

#include <stdint.h>

namespace MasterLink {
void begin();
void loop();
void sendSetpoint(const char *paramKey, float value);
void sendCalib(const char *paramKey, float point);
void sendDose(const char *channel, float ml);
void sendDoseStop(const char *channel);
void sendDoseHold(const char *channel, bool on);
/** Calibración caudal bomba → Master (NVS + Supabase). flowMlPerMin en UI. */
void sendPumpFlowCalib(const char *channel, float flowMlPerMin, float measuredMl, float durationSec);
/** Receta proporcional → master (HIDROWAVE updateNutrientProportions). */
void sendNutrientProportions();
/** Params malha fechada (volumen, pulsos, auto EC/pH). */
void sendLoopControl();
/** Pide inventory local + slaves ESP-NOW. */
void requestSlaves();
/** Pide device_id / cloud_ok al Master (solo lectura; registro = Master). */
void requestSysInfo();
/** Credenciales WiFi del Master (vía B; SoftAP = vía A). */
void sendWifiConfig(const char *ssid, const char *password, const char *deviceName = nullptr,
                    const char *email = nullptr, const char *location = nullptr);
/** Último wifi_config_ack: 0=ninguno 1=ok 2=fail. */
uint8_t wifiConfigAckState();
void clearWifiConfigAck();
/** Reinicia el Master (UART). No borra NVS. */
void sendMasterReboot();
/** Soft factory Master: limpia hydro_system WiFi/perfil y reinicia Master. */
void sendFactoryReset();
/** Relé PCF del master (0..7). state: "on"/"off". duration segundos (0=forever). */
void sendRelayLocal(uint8_t relay, const char *state, int durationSec);
/** Relé en slave ESP-NOW. mac "AA:BB:...". */
void sendRelaySlave(const char *mac, uint8_t relay, const char *state, int durationSec);
bool linkOk();
/** Último sys_info del Master (vacío si nunca llegó). */
const char *masterDeviceId();
bool masterCloudOk();
bool masterSysInfoValid();
/** true si el Master respondió cmd_ack de proceso (bridge dose/loop/relay activo). */
bool processBridgeOk();
/** 0=ninguno 1=ok 2=fail — último cmd_ack de proceso. */
uint8_t lastProcessAckState();
}

#endif
