#include "MasterLink.h"
#include "BoardPins.h"
#include "DataStore.h"
#include "MasterWifiDraft.h"
#include "NutrientConfig.h"
#include "PumpConfig.h"
#include "DoseChannel.h"
#include "ReservoirConfig.h"
#include "SlaveInventory.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <cstring>
#include <math.h>

static HardwareSerial MasterSerial(1);
/** t:slaves con Master+varios Atlas+8 relés supera 768 B (IncompleteInput). */
static char lineBuf[4608];
static size_t lineLen = 0;
static bool discardRestOfLine = false;
static unsigned long lastRxMs = 0;
static bool linked = false;
static char masterDeviceIdBuf[40] = {};
static bool masterCloudOkFlag = false;
static bool masterSysInfoValidFlag = false;
static uint8_t wifiConfigAckState_ = 0;
static bool processBridgeOk_ = false;
static uint8_t lastProcessAckState_ = 0;
static bool masterWifiConnectedFlag = false;
static bool masterHasWifiFlag = false;
static char masterSsidBuf[33] = {};
static char masterDeviceNameBuf[32] = {};
static char masterLocationBuf[32] = {};
static char lastCmdActionBuf[24] = {};
static uint8_t lastCmdAckState_ = 0;
#if UART_LINK_DEBUG
static uint32_t rxByteCount = 0;
static uint32_t rxLineCount = 0;
static unsigned long lastDbgMs = 0;
#endif

static void logLinePreview(const char *prefix, const char *line) {
    char preview[97];
    size_t n = strlen(line);
    if (n > 96) {
        n = 96;
    }
    memcpy(preview, line, n);
    preview[n] = '\0';
    Serial.printf("%s (len=%u): %s%s\n", prefix, static_cast<unsigned>(strlen(line)), preview,
                  strlen(line) > 96 ? "..." : "");
}

static void markLinkAlive() {
    lastRxMs = millis();
    linked = true;
    DataStore::instance().setLinkOk(true);
}

static void logRxSummary(const char *line, const char *t) {
#if !UART_LINK_DEBUG
    (void)line;
#endif
    if (strcmp(t, "telemetry") == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, line)) {
            return;
        }
        const float ec = doc["ec"] | NAN;
        const float ph = doc["ph"] | NAN;
        const float temp = doc["temp_agua"] | NAN;
        Serial.print("[UART RX] telemetry");
        if (!isnan(ec)) {
            Serial.printf(" ec=%.0f", ec);
        }
        if (!isnan(ph)) {
            Serial.printf(" ph=%.2f", ph);
        }
        if (!isnan(temp)) {
            Serial.printf(" temp=%.1f", temp);
        }
        Serial.println(" → LIVE");
        return;
    }
    if (strcmp(t, "cmd_ack") == 0) {
        JsonDocument doc;
        if (deserializeJson(doc, line)) {
            return;
        }
        // ArduinoJson: bool true con "| 0" cae al default 0 — usar bool
        const bool ok = doc["ok"] | false;
        Serial.printf("[UART RX] cmd_ack action=%s ok=%d\n", doc["action"] | "", ok ? 1 : 0);
        return;
    }
    if (strcmp(t, "sys_info") == 0) {
        Serial.println("[UART RX] sys_info");
        return;
    }
    if (strcmp(t, "slaves") == 0) {
        Serial.println("[UART RX] slaves");
        return;
    }
    if (strcmp(t, "wifi_config_ack") == 0) {
        Serial.println("[UART RX] wifi_config_ack");
        return;
    }
#if UART_LINK_DEBUG
    logLinePreview("[UART RX]", line);
#endif
}

static void emitJson(JsonDocument &doc) {
    serializeJson(doc, MasterSerial);
    MasterSerial.print('\n');
    MasterSerial.flush();
    /* No volcar password WiFi al Serial. */
    const char *action = doc["action"] | "";
    if (strcmp(action, "wifi_config") == 0) {
        Serial.printf("[UART TX] {\"t\":\"cmd\",\"action\":\"wifi_config\",\"ssid\":\"%s\",\"password\":\"***\"}\n",
                      doc["ssid"] | "");
        return;
    }
    Serial.print("[UART TX] ");
    serializeJson(doc, Serial);
    Serial.println();
}

void MasterLink::begin() {
    /* El JSON t:slaves cabe en varios KB; el RX por defecto (256 B) lo parte. */
    MasterSerial.setRxBufferSize(8192);
    MasterSerial.begin(MASTER_UART_BAUD, SERIAL_8N1, MASTER_UART_RX, MASTER_UART_TX);
    lineLen = 0;
    linked = false;
    lastRxMs = 0;
    processBridgeOk_ = false;
    lastProcessAckState_ = 0;
    DataStore::instance().setLinkOk(false);
    SlaveInventory::begin();
    Serial.printf("[UART] master link RX=%d TX=%d baud=%d\n",
                  MASTER_UART_RX, MASTER_UART_TX, MASTER_UART_BAUD);
#if UART_LINK_DEBUG
    Serial.println("[UART DBG] cable: HMI TX(17)->Master RX(17), Master TX(18)->HMI RX(18), GND");
#endif
}

static void handleLine(const char *line) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, line);
    if (err) {
#if UART_LINK_DEBUG
        Serial.printf("[UART RX] JSON invalido: %s\n", err.c_str());
        logLinePreview("[UART RX] raw", line);
#endif
        return;
    }
    const char *t = doc["t"] | "";
#if UART_LINK_DEBUG
    rxLineCount++;
    logRxSummary(line, t);
#endif
    if (strcmp(t, "cmd_ack") == 0) {
        const bool ok = doc["ok"] | false;
        lastProcessAckState_ = ok ? 1 : 2;
        lastCmdAckState_ = ok ? 1 : 2;
        const char *act = doc["action"] | "";
        if (act[0]) {
            strncpy(lastCmdActionBuf, act, sizeof(lastCmdActionBuf) - 1);
            lastCmdActionBuf[sizeof(lastCmdActionBuf) - 1] = '\0';
        }
        if (ok) {
            processBridgeOk_ = true;
        }
        markLinkAlive();
        return;
    }
    if (strcmp(t, "wifi_config_ack") == 0) {
        wifiConfigAckState_ = (doc["ok"] | false) ? 1 : 2;
        const char *id = doc["device_id"] | "";
        if (id[0]) {
            strncpy(masterDeviceIdBuf, id, sizeof(masterDeviceIdBuf) - 1);
            masterDeviceIdBuf[sizeof(masterDeviceIdBuf) - 1] = '\0';
            masterSysInfoValidFlag = true;
        }
        markLinkAlive();
        return;
    }
    if (strcmp(t, "sys_info") == 0) {
        const char *id = doc["device_id"] | "";
        if (id[0]) {
            strncpy(masterDeviceIdBuf, id, sizeof(masterDeviceIdBuf) - 1);
            masterDeviceIdBuf[sizeof(masterDeviceIdBuf) - 1] = '\0';
        } else {
            masterDeviceIdBuf[0] = '\0';
        }
        masterCloudOkFlag = doc["cloud_ok"] | false;
        masterSysInfoValidFlag = true;
        if (doc["process_bridge"] | false) {
            processBridgeOk_ = true;
        }
        masterHasWifiFlag = doc["has_wifi"] | false;
        masterWifiConnectedFlag = doc["wifi_connected"] | false;
        const char *ssid = masterHasWifiFlag ? (doc["ssid"] | "") : "";
        const char *pass = masterHasWifiFlag ? (doc["password"] | "") : "";
        strncpy(masterSsidBuf, ssid, sizeof(masterSsidBuf) - 1);
        masterSsidBuf[sizeof(masterSsidBuf) - 1] = '\0';
        const char *dn = doc["device_name"] | "";
        const char *loc = doc["location"] | "";
        strncpy(masterDeviceNameBuf, dn, sizeof(masterDeviceNameBuf) - 1);
        masterDeviceNameBuf[sizeof(masterDeviceNameBuf) - 1] = '\0';
        strncpy(masterLocationBuf, loc, sizeof(masterLocationBuf) - 1);
        masterLocationBuf[sizeof(masterLocationBuf) - 1] = '\0';
        MasterWifiDraft::applyFromMaster(ssid, pass, doc["email"] | "", dn, loc);
        markLinkAlive();
        return;
    }
    if (strcmp(t, "slaves") == 0) {
        SlaveInventory::applyFromJson(line);
        size_t localN = 0;
        size_t atlasN = 0;
        const size_t nT = SlaveInventory::count();
        for (size_t i = 0; i < nT; ++i) {
            const SlaveInventory::Target *tgt = SlaveInventory::at(i);
            if (SlaveInventory::isEspNow(tgt)) {
                ++atlasN;
            } else if (tgt && tgt->local) {
                ++localN;
            }
        }
        Serial.printf("[UART] inventario local=%u atlas=%u%s\n",
                      static_cast<unsigned>(localN), static_cast<unsigned>(atlasN),
                      atlasN == 0 ? " (hub Atlas offline esperado)" : "");
        for (size_t i = 0; i < nT; ++i) {
            const SlaveInventory::Target *tgt = SlaveInventory::at(i);
            if (!tgt) {
                continue;
            }
            Serial.printf("[UART]   %s mac=%s name=%s online=%d relays=%u\n",
                          tgt->local ? "local" : "atlas", tgt->mac,
                          tgt->name[0] ? tgt->name : "-", tgt->online ? 1 : 0,
                          static_cast<unsigned>(tgt->numRelays));
        }
        const size_t hubIx = SlaveInventory::firstEspNowIndex();
        const SlaveInventory::Target *hub =
            hubIx != SIZE_MAX ? SlaveInventory::at(hubIx) : nullptr;
        Serial.printf("[UART] hub=%s\n", hub && hub->mac[0] ? hub->mac : "(ninguno)");
        markLinkAlive();
        return;
    }
    if (strcmp(t, "plant_cfg") == 0) {
        JsonArray pumps = doc["pumps"].as<JsonArray>();
        const char *names[NUTRIENT_MAX];
        float mls[NUTRIENT_MAX];
        uint8_t relays[NUTRIENT_MAX];
        char nameStore[NUTRIENT_MAX][NUTRIENT_NAME_LEN];
        size_t nQty = 0;
        for (JsonObject p : pumps) {
            const int relay = p["relay"] | 0;
            const float ml = p["mlPerLiter"] | 0.0f;
            const float flow = p["flowMlPerMin"] | 0.0f;
            const char *name = p["name"] | "";
            DoseChannel ch;
            if (tryDoseFromRelayNumber(static_cast<uint8_t>(relay), &ch) && flow > 0.1f) {
                PumpConfig::setFlowMlPerMin(ch, flow);
            }
            if (ml <= 0.05f || !name[0] || strncmp(name, "pump_r", 6) == 0) {
                continue;
            }
            if (relay < 1 || relay > static_cast<int>(PUMP_RELAY_COUNT) || nQty >= NUTRIENT_MAX) {
                continue;
            }
            strncpy(nameStore[nQty], name, NUTRIENT_NAME_LEN - 1);
            nameStore[nQty][NUTRIENT_NAME_LEN - 1] = '\0';
            names[nQty] = nameStore[nQty];
            mls[nQty] = ml;
            relays[nQty] = static_cast<uint8_t>(relay);
            ++nQty;
        }
        if (nQty > 0) {
            NutrientConfig::replaceQuantities(names, mls, relays, nQty);
        }
        Serial.printf("[UART] plant_cfg qty=%u\n", static_cast<unsigned>(nQty));
        markLinkAlive();
        return;
    }
    if (strcmp(t, "relay") == 0) {
        const char *mac = doc["mac"] | "";
        const int relay = doc["relay"] | -1;
        const bool on = (doc["on"] | 0) != 0;
        if (mac[0] && relay >= 0 && relay < static_cast<int>(SlaveInventory::kMaxRelays)) {
            SlaveInventory::applyRelayBit(mac, static_cast<uint8_t>(relay), on);
            Serial.printf("[UART] relay mac=%s r=%d on=%d\n", mac, relay, on ? 1 : 0);
        }
        markLinkAlive();
        return;
    }
    if (strcmp(t, "telemetry") != 0) {
        return;
    }
    const float ph = doc["ph"] | NAN;
    const float ec = doc["ec"] | NAN;
    const float temp = doc["temp_agua"] | NAN;
    const float orp = doc["orp"] | NAN;
    const float doMgL = doc["do"] | NAN;

    /* Flags aditivos (Master 2026-09). Sin flag: válido si el campo numérico llegó. */
    const bool phOk = doc.containsKey("ph_valid") ? (doc["ph_valid"] | false) : !isnan(ph);
    const bool ecOk = doc.containsKey("ec_valid") ? (doc["ec_valid"] | false) : !isnan(ec);
    const bool tempOk =
        doc.containsKey("temp_valid") ? (doc["temp_valid"] | false) : !isnan(temp);

    if (!phOk && !ecOk && !tempOk && isnan(orp) && isnan(doMgL)) {
        /* Solo flags false / vacío — aún así marcar link vivo y limpiar PV. */
        DataStore::instance().setTelemetry(NAN, NAN, NAN, NAN, NAN, DataSource::Live, false, false,
                                           false);
        markLinkAlive();
        return;
    }

    DataStore &store = DataStore::instance();
    store.setTelemetry(phOk ? ph : NAN, ecOk ? ec : NAN, tempOk ? temp : NAN, orp, doMgL,
                       DataSource::Live, phOk, ecOk, tempOk);
    markLinkAlive();
}

static void maybeLogUartDebug(unsigned long nowMs) {
#if UART_LINK_DEBUG
    if (lastDbgMs != 0 && (nowMs - lastDbgMs) < UART_LINK_DEBUG_INTERVAL_MS) {
        return;
    }
    lastDbgMs = nowMs;
    Serial.printf("[UART DBG] rx_bytes=%lu lines=%lu link=%s last_rx=%lums ago\n",
                  static_cast<unsigned long>(rxByteCount),
                  static_cast<unsigned long>(rxLineCount), linked ? "OK" : "NO",
                  lastRxMs != 0 ? static_cast<unsigned long>(nowMs - lastRxMs) : 0UL);
    if (rxByteCount == 0) {
        Serial.println("[UART DBG] sin bytes RX — Master TX(18)->HMI RX(18) + GND?");
    }
#else
    (void)nowMs;
#endif
}

void MasterLink::loop() {
    while (MasterSerial.available() > 0) {
        const char c = static_cast<char>(MasterSerial.read());
#if UART_LINK_DEBUG
        rxByteCount++;
#endif
        if (c == '\n' || c == '\r') {
            if (discardRestOfLine) {
                discardRestOfLine = false;
                lineLen = 0;
                continue;
            }
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                handleLine(lineBuf);
                lineLen = 0;
            }
            continue;
        }
        if (discardRestOfLine) {
            continue;
        }
        if (lineLen + 1 < sizeof(lineBuf)) {
            lineBuf[lineLen++] = c;
        } else {
            Serial.printf("[UART RX] linea truncada (>%u B) — resto descartado\n",
                          static_cast<unsigned>(sizeof(lineBuf) - 1));
            discardRestOfLine = true;
            lineLen = 0;
        }
    }

    const unsigned long nowMs = millis();
    maybeLogUartDebug(nowMs);

    if (linked && lastRxMs != 0 && (nowMs - lastRxMs) > UART_LINK_TIMEOUT_MS) {
#if UART_LINK_DEBUG
        Serial.println("[UART] link LOST — sin RX del Master >5s (pin 18 / GND?)");
#endif
        linked = false;
        processBridgeOk_ = false;
        DataStore::instance().setLinkOk(false);
    }
}

void MasterLink::sendSetpoint(const char *paramKey, float value) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "setpoint";
    doc[paramKey] = value;
    emitJson(doc);
}

void MasterLink::sendCalib(const char *paramKey, float point) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "calib";
    doc["param"] = paramKey;
    doc["point"] = point;
    emitJson(doc);
}

void MasterLink::sendDose(const char *channel, float ml) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "dose";
    doc["channel"] = channel;
    doc["ml"] = ml;
    emitJson(doc);
}

void MasterLink::sendDoseStop(const char *channel) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "dose_stop";
    doc["channel"] = channel;
    emitJson(doc);
}

void MasterLink::sendDoseHold(const char *channel, bool on) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "dose_hold";
    doc["channel"] = channel;
    doc["on"] = on ? 1 : 0;
    emitJson(doc);
}

void MasterLink::sendPumpFlowCalib(const char *channel, float flowMlPerMin, float measuredMl,
                                   float durationSec) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "pump_flow_calib";
    doc["channel"] = channel ? channel : "";
    doc["flowMlPerMin"] = flowMlPerMin;
    doc["measuredMl"] = measuredMl;
    doc["durationSec"] = durationSec;
    emitJson(doc);
}

void MasterLink::sendNutrientProportions() {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "nutrient_proportions";
    doc["totalMlPerLiter"] = NutrientConfig::totalMlPerL();
    doc["recipeEcUs"] = NutrientConfig::recipeEcUs();
    doc["baseDose"] = NutrientConfig::recipeEcUs();
    JsonArray arr = doc["nutrients"].to<JsonArray>();
    const size_t n = NutrientConfig::listCount();
    for (size_t i = 0; i < n; ++i) {
        const float ml = NutrientConfig::listMlPerL(i);
        const char *nm = NutrientConfig::listName(i);
        if (ml <= 0.0f || !nm || nm[0] == '\0') {
            continue;
        }
        JsonObject o = arr.add<JsonObject>();
        o["name"] = nm;
        o["relayNumber"] = NutrientConfig::listRelayNumber(i);
        o["mlPerLiter"] = ml;
        o["proportion"] = NutrientConfig::listProportion(i);
        o["active"] = true;
        const uint8_t rn = NutrientConfig::listRelayNumber(i);
        if (rn >= 1 && rn <= PUMP_RELAY_COUNT) {
            const float qMin = PumpConfig::flowMlPerMin(doseFromRelayNumber(rn));
            if (qMin > 0.01f) {
                o["flowRate"] = qMin / 60.0f;
            }
        }
    }
    emitJson(doc);
}

void MasterLink::sendLoopControl() {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "loop_control";
    doc["volumeL"] = ReservoirConfig::volumeL();
    doc["homoSec"] = ReservoirConfig::homoSec();
    doc["nutrientGapSec"] = ReservoirConfig::nutrientGapSec();
    doc["pulseMl"] = ReservoirConfig::pulseMl();
    doc["pulseGapSec"] = ReservoirConfig::pulseGapSec();
    doc["dosingDelaySec"] = ReservoirConfig::dosingDelaySec();
    doc["dosingMode"] =
        (ReservoirConfig::dosingMode() == ReservoirConfig::DosingMode::Recirculating) ? "recirc"
                                                                                      : "batch";
    doc["dosingArmed"] = ReservoirConfig::dosingArmed();
    /* Sin armar, Auto no sale activo (Inactive|Dosing). */
    const bool armed = ReservoirConfig::dosingArmed();
    doc["autoEc"] = armed && ReservoirConfig::autoEcEnabled();
    doc["autoEcIntervalSec"] = ReservoirConfig::autoEcIntervalSec();
    doc["autoPh"] = armed && ReservoirConfig::autoPhEnabled();
    doc["autoPhIntervalSec"] = ReservoirConfig::autoPhIntervalSec();
    doc["maxStepEc"] = ReservoirConfig::maxStepEc() / 100.0f;
    doc["maxStepPh"] = ReservoirConfig::maxStepPh() / 100.0f;
    doc["dosingConstEc"] = 0.0;
    doc["dosingConstPh"] = 0.0;
    doc["phUpRelay"] = NutrientConfig::phUpRelay();
    doc["phDownRelay"] = NutrientConfig::phDownRelay();
    doc["consumoDiario"] = ReservoirConfig::consumoDiarioEnabled();
    doc["consumoPh24h"] = ReservoirConfig::consumoPh24hEnabled();
    const ParamConfig ecCfg = DataStore::instance().config(ParamId::Ec);
    doc["ecLo"] = ecCfg.low;
    doc["ecHi"] = ecCfg.high;
    const ParamConfig phCfg = DataStore::instance().config(ParamId::Ph);
    doc["phLo"] = phCfg.low;
    doc["phHi"] = phCfg.high;
    emitJson(doc);
}

void MasterLink::requestSlaves() {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "slaves_req";
    emitJson(doc);
}

void MasterLink::requestSysInfo() {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "sys_info_req";
    emitJson(doc);
}

void MasterLink::sendWifiConfig(const char *ssid, const char *password, const char *deviceName,
                                const char *email, const char *location) {
    wifiConfigAckState_ = 0;
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "wifi_config";
    doc["ssid"] = ssid ? ssid : "";
    doc["password"] = password ? password : "";
    if (deviceName && deviceName[0]) {
        doc["device_name"] = deviceName;
    }
    if (email && email[0]) {
        doc["email"] = email;
    }
    if (location && location[0]) {
        doc["location"] = location;
    }
    emitJson(doc);
}

uint8_t MasterLink::wifiConfigAckState() { return wifiConfigAckState_; }

void MasterLink::clearWifiConfigAck() { wifiConfigAckState_ = 0; }

void MasterLink::sendMasterReboot() {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "master_reboot";
    emitJson(doc);
}

void MasterLink::sendFactoryReset() {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "factory_reset";
    emitJson(doc);
}

void MasterLink::sendRelayLocal(uint8_t relay, const char *action, int durationSec) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "relay_local";
    doc["relay"] = relay;
    doc["state"] = action ? action : "off";
    doc["duration"] = durationSec;
    emitJson(doc);
}

void MasterLink::sendRelaySlave(const char *mac, uint8_t relay, const char *action, int durationSec,
                                 int cycleOffSec, const char *mode) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "relay_slave";
    doc["mac"] = mac ? mac : "";
    doc["relay"] = relay;
    doc["state"] = action ? action : "off";
    doc["duration"] = durationSec;
    if (cycleOffSec > 0) {
        doc["cycleOff"] = cycleOffSec;
    }
    if (mode && mode[0] != '\0') {
        doc["mode"] = mode;
    }
    emitJson(doc);
}

bool MasterLink::linkOk() { return linked; }

const char *MasterLink::masterDeviceId() { return masterDeviceIdBuf; }

bool MasterLink::masterCloudOk() { return masterCloudOkFlag; }

bool MasterLink::masterSysInfoValid() { return masterSysInfoValidFlag; }

bool MasterLink::masterWifiConnected() { return masterWifiConnectedFlag; }

bool MasterLink::masterHasWifi() { return masterHasWifiFlag; }

const char *MasterLink::masterSsid() { return masterSsidBuf; }

const char *MasterLink::masterDeviceName() { return masterDeviceNameBuf; }

const char *MasterLink::masterLocation() { return masterLocationBuf; }

const char *MasterLink::lastCmdAction() { return lastCmdActionBuf; }

uint8_t MasterLink::lastCmdAckState() { return lastCmdAckState_; }

bool MasterLink::processBridgeOk() { return processBridgeOk_; }

uint8_t MasterLink::lastProcessAckState() { return lastProcessAckState_; }

void MasterLink::flush() { MasterSerial.flush(); }

void MasterLink::clearLastCmdAck() {
    lastCmdAckState_ = 0;
    lastCmdActionBuf[0] = '\0';
}
