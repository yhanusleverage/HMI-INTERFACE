#include "MasterLink.h"
#include "BoardPins.h"
#include "DataStore.h"
#include "NutrientConfig.h"
#include "ReservoirConfig.h"
#include "SlaveInventory.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <cstring>

static HardwareSerial MasterSerial(1);
static char lineBuf[768];
static size_t lineLen = 0;
static unsigned long lastRxMs = 0;
static bool linked = false;
static char masterDeviceIdBuf[40] = {};
static bool masterCloudOkFlag = false;
static bool masterSysInfoValidFlag = false;
static uint8_t wifiConfigAckState_ = 0;
static bool processBridgeOk_ = false;
static uint8_t lastProcessAckState_ = 0;

static void emitJson(JsonDocument &doc) {
    serializeJson(doc, MasterSerial);
    MasterSerial.print('\n');
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
}

static void handleLine(const char *line) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, line);
    if (err) {
        return;
    }
    const char *t = doc["t"] | "";
    if (strcmp(t, "cmd_ack") == 0) {
        const bool ok = doc["ok"] | false;
        lastProcessAckState_ = ok ? 1 : 2;
        if (ok) {
            processBridgeOk_ = true;
        }
        lastRxMs = millis();
        linked = true;
        DataStore::instance().setLinkOk(true);
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
        lastRxMs = millis();
        linked = true;
        DataStore::instance().setLinkOk(true);
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
        lastRxMs = millis();
        linked = true;
        DataStore::instance().setLinkOk(true);
        return;
    }
    if (strcmp(t, "slaves") == 0) {
        SlaveInventory::applyFromJson(line);
        lastRxMs = millis();
        linked = true;
        DataStore::instance().setLinkOk(true);
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
    if (isnan(ph) && isnan(ec) && isnan(temp) && isnan(orp) && isnan(doMgL)) {
        return;
    }
    DataStore &store = DataStore::instance();
    const TelemetrySnapshot cur = store.snapshot();
    store.setTelemetry(isnan(ph) ? cur.ph : ph,
                       isnan(ec) ? cur.ec : ec,
                       isnan(temp) ? cur.tempAgua : temp,
                       isnan(orp) ? cur.orp : orp,
                       isnan(doMgL) ? cur.doMgL : doMgL,
                       DataSource::Live);
    lastRxMs = millis();
    linked = true;
    store.setLinkOk(true);
}

void MasterLink::loop() {
    while (MasterSerial.available() > 0) {
        const char c = static_cast<char>(MasterSerial.read());
        if (c == '\n' || c == '\r') {
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                handleLine(lineBuf);
                lineLen = 0;
            }
            continue;
        }
        if (lineLen + 1 < sizeof(lineBuf)) {
            lineBuf[lineLen++] = c;
        } else {
            lineLen = 0;
        }
    }

    if (linked && lastRxMs != 0 && (millis() - lastRxMs) > UART_LINK_TIMEOUT_MS) {
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

void MasterLink::sendRelayLocal(uint8_t relay, const char *action, int durationSec) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "relay_local";
    doc["relay"] = relay;
    doc["state"] = action ? action : "off";
    doc["duration"] = durationSec;
    emitJson(doc);
}

void MasterLink::sendRelaySlave(const char *mac, uint8_t relay, const char *action, int durationSec) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "relay_slave";
    doc["mac"] = mac ? mac : "";
    doc["relay"] = relay;
    doc["state"] = action ? action : "off";
    doc["duration"] = durationSec;
    emitJson(doc);
}

bool MasterLink::linkOk() { return linked; }

const char *MasterLink::masterDeviceId() { return masterDeviceIdBuf; }

bool MasterLink::masterCloudOk() { return masterCloudOkFlag; }

bool MasterLink::masterSysInfoValid() { return masterSysInfoValidFlag; }

bool MasterLink::processBridgeOk() { return processBridgeOk_; }

uint8_t MasterLink::lastProcessAckState() { return lastProcessAckState_; }
