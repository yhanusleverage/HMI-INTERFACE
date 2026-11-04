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
/** Ganho químico pH: ml / |ΔpH|. acid=true es pH−. No lleva litros del sistema. */
void sendPhDeltaGain(bool acid, float mlPerPh);
/** Receta proporcional → master (HIDROWAVE updateNutrientProportions). */
void sendNutrientProportions();
/** Params de lazo. autoEc/autoPh solo si el usuario tocó ese interruptor. */
void sendLoopControl();
/** Emergencia: el Master corta la dosis de malla y deja el candado INACTIVO. */
void sendMeshStop();
/** normal | carrera | indep. Mismo interruptor que la web. */
void sendLevelInterlock(const char *mode);
/** Ultimo modo visto por telemetria o confirmado por cmd_ack. */
const char *levelInterlockMode();
void rememberLevelInterlock(const char *mode);
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
/** Relé en slave ESP-NOW. mac "AA:BB:...". mode "cycle" usa cycleOffSec (segundos OFF). */
void sendRelaySlave(const char *mac, uint8_t relay, const char *state, int durationSec,
                    int cycleOffSec = 0, const char *mode = nullptr);
bool linkOk();
/** El usuario dejó el WiFi del Master apagado. false hasta la primera telemetría. */
bool wifiUserKnown();
bool wifiUserOffline();
void rememberWifiUserOffline(bool offline);
/** Auto EC o Auto pH en fase dosing. false hasta la primera telemetría. */
bool autoDosing();
bool autoDosingEc();
bool autoDosingPh();
bool autoHomoEc();
bool autoHomoPh();
/** ml reales de la dosis en curso. false si no está dosificando. */
bool hasDoseMlEc();
bool hasDoseMlPh();
float doseMlEc();
float doseMlPh();
/** offline=true: el Master no busca red. El cable sigue. */
void sendWifiUserMode(bool offline);

/** Ventana de 24 h que el Master manda en telemetría. */
struct DayWindow {
    bool hasRef;
    float ref;
    uint32_t elapsedSec;
    bool hasDeriv;
    float deriv;
};
const DayWindow &consumoEc();
const DayWindow &consumoPh();
/** Último sys_info del Master (vacío si nunca llegó). */
const char *masterDeviceId();
bool masterCloudOk();
bool masterSysInfoValid();
/** Snapshot sys_info (debug Sistema). */
bool masterWifiConnected();
bool masterHasWifi();
const char *masterSsid();
const char *masterDeviceName();
const char *masterLocation();
/** Último cmd_ack: action + 0=ninguno 1=ok 2=fail. */
const char *lastCmdAction();
uint8_t lastCmdAckState();
/** true si el Master respondió cmd_ack de proceso (bridge dose/loop/relay activo). */
bool processBridgeOk();
/** 0=ninguno 1=ok 2=fail — último cmd_ack de proceso. */
uint8_t lastProcessAckState();
/** Vacía TX UART (antes de reboot HMI). */
void flush();
/** Limpia último cmd_ack (antes de esperar master_reboot). */
void clearLastCmdAck();

/** Pide al Master la lista de reglas y el catálogo de nombres. */
void requestRules();
void sendRuleSet(const char *ruleId, bool enabled);
void sendRuleDelete(const char *ruleId);
/** until: vazio|baixo|medio|medio_alto|alto. id vacío crea; con id, actualiza esa regla. */
void sendRuleCreate(const char *name, const char *mac, uint8_t relay, const char *until,
                    int relay2 = -1, const char *until2 = nullptr, const char *id = nullptr);
/** Pide al Master guardar el nombre del relé Atlas en Supabase. */
void sendRelayRename(const char *mac, uint8_t relay, const char *name);
/** Actualiza el catálogo en RAM para que Atlas muestre el nombre al volver. */
void setRelayDisplayName(const char *mac, uint8_t relay, const char *name);

struct RuleView {
    bool used;
    bool enabled;
    char id[48];
    char name[48];
    char level[32];
    char steps[4][64];
    char macs[4][18];
    char untils[4][16];
    int relays[4];
    uint8_t stepCount;
};

uint8_t ruleViewCount();
const RuleView *ruleViewAt(uint8_t index);
uint32_t ruleViewGeneration();
void setRuleViewEnabled(uint8_t index, bool enabled);
void dropRuleView(uint8_t index);

uint32_t relayCatalogGeneration();
uint8_t relayCatalogCount();
const char *relayCatalogMac(uint8_t slot);
const char *relayCatalogName(uint8_t slot, uint8_t relay);
/** device_name de la web para esa MAC. Vacío si no hay. */
const char *relayCatalogPlace(const char *mac);
/** Nombre de Supabase para esa MAC y ese relé 0-7. Vacío si no hay. */
const char *relayDisplayName(const char *mac, uint8_t relay);
}

#endif
