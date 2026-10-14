#include "MasterLink.h"
#include "BoardPins.h"
#include "DataStore.h"
#include "MasterWifiDraft.h"
#include "WifiConfig.h"
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
/** Una vez por arranque: no repetir wifi_config si el master reinicia. */
static bool wifiOfferDone_ = false;
static unsigned long lastSysInfoAskMs_ = 0;
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

struct NameSlot {
    bool used;
    char mac[18];
    char place[40];
    char names[8][32];
};

static MasterLink::RuleView ruleLive_[8];
static MasterLink::RuleView ruleStage_[8];
static uint8_t ruleLiveCount_ = 0;
static uint8_t ruleStageCount_ = 0;
static bool ruleOpen_ = false;
static uint32_t ruleGen_ = 0;

static NameSlot nameLive_[4];
static NameSlot nameStage_[4];
static uint8_t nameLiveCount_ = 0;
static uint8_t nameStageCount_ = 0;
static bool nameOpen_ = false;
static uint32_t nameGen_ = 0;

static void copyField(char *dst, size_t n, const char *src) {
    if (!dst || n == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

static bool macEq(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'a' && ca <= 'z') {
            ca = static_cast<char>(ca - 32);
        }
        if (cb >= 'a' && cb <= 'z') {
            cb = static_cast<char>(cb - 32);
        }
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

static void onRelayNamesBegin() {
    nameOpen_ = true;
    nameStageCount_ = 0;
    memset(nameStage_, 0, sizeof(nameStage_));
}

static void onRelayNames(JsonDocument &doc) {
    if (!nameOpen_ || nameStageCount_ >= 4) {
        return;
    }
    NameSlot &slot = nameStage_[nameStageCount_];
    memset(&slot, 0, sizeof(slot));
    slot.used = true;
    copyField(slot.mac, sizeof(slot.mac), doc["mac"] | "");
    copyField(slot.place, sizeof(slot.place), doc["place"] | "");
    JsonArray names = doc["names"].as<JsonArray>();
    uint8_t i = 0;
    if (names.isNull()) {
        ++nameStageCount_;
        return;
    }
    for (JsonVariant item : names) {
        if (i >= 8) {
            break;
        }
        if (item.is<const char *>()) {
            copyField(slot.names[i], sizeof(slot.names[i]), item.as<const char *>());
        }
        ++i;
    }
    ++nameStageCount_;
}

static void onRelayNamesEnd() {
    nameOpen_ = false;
    const bool same = nameStageCount_ == nameLiveCount_ &&
                      memcmp(nameStage_, nameLive_, sizeof(nameLive_)) == 0;
    if (same) {
        return;
    }
    memcpy(nameLive_, nameStage_, sizeof(nameLive_));
    nameLiveCount_ = nameStageCount_;
    ++nameGen_;
}

static void onRulesBegin() {
    ruleOpen_ = true;
    ruleStageCount_ = 0;
    memset(ruleStage_, 0, sizeof(ruleStage_));
}

static bool parseRuleEnabled(JsonDocument &doc) {
    JsonVariantConst v = doc["enabled"];
    if (v.isNull()) {
        v = doc["active"];
    }
    if (v.isNull()) {
        return false;
    }
    if (v.is<bool>()) {
        return v.as<bool>();
    }
    if (v.is<const char *>()) {
        const char *s = v.as<const char *>();
        return s && (s[0] == '1' || s[0] == 't' || s[0] == 'T' || s[0] == 'y' || s[0] == 'Y');
    }
    return v.as<int>() != 0;
}

static void fillRuleView(JsonDocument &doc, MasterLink::RuleView &view) {
    memset(&view, 0, sizeof(view));
    view.used = true;
    view.enabled = parseRuleEnabled(doc);
    copyField(view.id, sizeof(view.id), doc["id"] | "");
    copyField(view.name, sizeof(view.name), doc["name"] | "");
    copyField(view.level, sizeof(view.level), doc["level"] | "");
    for (uint8_t i = 0; i < 4; ++i) {
        view.relays[i] = -1;
    }
    JsonArray steps = doc["steps"].as<JsonArray>();
    uint8_t n = 0;
    if (!steps.isNull()) {
        for (JsonVariant step : steps) {
            if (n >= 4) {
                break;
            }
            if (step.is<const char *>()) {
                copyField(view.steps[n], sizeof(view.steps[n]), step.as<const char *>());
                ++n;
            }
        }
    }
    view.stepCount = n;
    JsonArray relays = doc["relays"].as<JsonArray>();
    uint8_t r = 0;
    if (!relays.isNull()) {
        for (JsonObject slot : relays) {
            if (r >= 4) {
                break;
            }
            view.relays[r] = slot["r"] | -1;
            copyField(view.macs[r], sizeof(view.macs[r]), slot["mac"] | "");
            copyField(view.untils[r], sizeof(view.untils[r]), slot["until"] | "");
            ++r;
        }
    }
}

static void applyLiveRule(JsonDocument &doc) {
    MasterLink::RuleView incoming;
    fillRuleView(doc, incoming);
    if (!incoming.id[0]) {
        return;
    }
    for (uint8_t i = 0; i < ruleLiveCount_; ++i) {
        if (strcmp(ruleLive_[i].id, incoming.id) == 0) {
            ruleLive_[i] = incoming;
            ++ruleGen_;
            return;
        }
    }
    if (ruleLiveCount_ >= 8) {
        return;
    }
    ruleLive_[ruleLiveCount_++] = incoming;
    ++ruleGen_;
}

static void onRule(JsonDocument &doc) {
    if (ruleOpen_) {
        if (ruleStageCount_ >= 8) {
            return;
        }
        fillRuleView(doc, ruleStage_[ruleStageCount_]);
        ++ruleStageCount_;
        return;
    }
    /* Master puede empujar un rule suelto al auto-desactivar tras ejecutar. */
    applyLiveRule(doc);
}

static void onRulesEnd() {
    ruleOpen_ = false;
    bool changed = (ruleLiveCount_ != ruleStageCount_);
    if (!changed) {
        for (uint8_t i = 0; i < ruleStageCount_; ++i) {
            if (memcmp(&ruleLive_[i], &ruleStage_[i], sizeof(ruleLive_[i])) != 0) {
                changed = true;
                break;
            }
        }
    }
    memcpy(ruleLive_, ruleStage_, sizeof(ruleLive_));
    ruleLiveCount_ = ruleStageCount_;
    if (changed) {
        ++ruleGen_;
    }
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
    if (strcmp(t, "relay_names_begin") == 0) {
        onRelayNamesBegin();
        markLinkAlive();
        return;
    }
    if (strcmp(t, "relay_names") == 0) {
        onRelayNames(doc);
        markLinkAlive();
        return;
    }
    if (strcmp(t, "relay_names_end") == 0) {
        onRelayNamesEnd();
        markLinkAlive();
        return;
    }
    if (strcmp(t, "rules_begin") == 0) {
        onRulesBegin();
        markLinkAlive();
        return;
    }
    if (strcmp(t, "rule") == 0) {
        onRule(doc);
        markLinkAlive();
        return;
    }
    if (strcmp(t, "rules_end") == 0) {
        onRulesEnd();
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

/** Master sin NVS y HMI ya asociado: manda la clave probada, una vez. */
static void offerSavedWifiIfMasterEmpty() {
#if UART_BENCH
    return;
#else
    if (wifiOfferDone_) {
        return;
    }
    if (!WifiConfig::configured()) {
        wifiOfferDone_ = true;
        return;
    }
    if (!masterSysInfoValidFlag) {
        const unsigned long now = millis();
        if (lastSysInfoAskMs_ == 0 || (now - lastSysInfoAskMs_) >= 2000UL) {
            lastSysInfoAskMs_ = now;
            MasterLink::requestSysInfo();
        }
        return;
    }
    if (masterHasWifiFlag) {
        wifiOfferDone_ = true;
        return;
    }
    if (WifiConfig::linkState() != 2) {
        return;
    }
    char ssid[WifiConfig::SSID_MAX];
    char pass[WifiConfig::PASS_MAX];
    WifiConfig::getSsid(ssid, sizeof(ssid));
    WifiConfig::getPass(pass, sizeof(pass));
    if (ssid[0] == '\0') {
        wifiOfferDone_ = true;
        return;
    }
    wifiOfferDone_ = true;
    Serial.printf("[WIFI] master sin red — enviando clave ya conectada ssid=%s\n", ssid);
    MasterLink::sendWifiConfig(ssid, pass);
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

    offerSavedWifiIfMasterEmpty();
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

void MasterLink::requestRules() {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "rules_req";
    emitJson(doc);
}

void MasterLink::sendRuleSet(const char *ruleId, bool enabled) {
    if (!ruleId || !ruleId[0]) {
        return;
    }
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "rule_set";
    doc["id"] = ruleId;
    doc["enabled"] = enabled ? 1 : 0;
    emitJson(doc);
}

void MasterLink::sendRuleDelete(const char *ruleId) {
    if (!ruleId || !ruleId[0]) {
        return;
    }
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "rule_delete";
    doc["id"] = ruleId;
    emitJson(doc);
}

void MasterLink::sendRelayRename(const char *mac, uint8_t relay, const char *name) {
    if (!mac || !mac[0] || relay > 7) {
        return;
    }
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "relay_rename";
    doc["mac"] = mac;
    doc["relay"] = relay;
    doc["name"] = name ? name : "";
    emitJson(doc);
}

void MasterLink::setRelayDisplayName(const char *mac, uint8_t relay, const char *name) {
    if (!mac || !mac[0] || relay >= 8) {
        return;
    }
    for (uint8_t i = 0; i < nameLiveCount_; ++i) {
        if (!nameLive_[i].used || !macEq(nameLive_[i].mac, mac)) {
            continue;
        }
        copyField(nameLive_[i].names[relay], sizeof(nameLive_[i].names[relay]), name ? name : "");
        ++nameGen_;
        return;
    }
}

void MasterLink::sendRuleCreate(const char *name, const char *mac, uint8_t relay, const char *until,
                                int relay2, const char *until2, const char *id) {
    JsonDocument doc;
    doc["t"] = "cmd";
    doc["action"] = "rule_create";
    doc["name"] = name ? name : "";
    doc["mac"] = mac ? mac : "";
    doc["relay"] = relay;
    doc["until"] = until ? until : "";
    if (relay2 >= 0 && until2 && until2[0]) {
        doc["relay2"] = relay2;
        doc["until2"] = until2;
    }
    if (id && id[0]) {
        doc["id"] = id;
    }
    emitJson(doc);
}

uint8_t MasterLink::ruleViewCount() { return ruleLiveCount_; }

const MasterLink::RuleView *MasterLink::ruleViewAt(uint8_t index) {
    if (index >= ruleLiveCount_ || !ruleLive_[index].used) {
        return nullptr;
    }
    return &ruleLive_[index];
}

uint32_t MasterLink::ruleViewGeneration() { return ruleGen_; }

void MasterLink::setRuleViewEnabled(uint8_t index, bool enabled) {
    if (index >= ruleLiveCount_) {
        return;
    }
    if (ruleLive_[index].enabled == enabled) {
        return;
    }
    ruleLive_[index].enabled = enabled;
    ++ruleGen_;
}

void MasterLink::dropRuleView(uint8_t index) {
    if (index >= ruleLiveCount_) {
        return;
    }
    for (uint8_t i = index; i + 1 < ruleLiveCount_; ++i) {
        ruleLive_[i] = ruleLive_[i + 1];
    }
    --ruleLiveCount_;
    memset(&ruleLive_[ruleLiveCount_], 0, sizeof(ruleLive_[0]));
    ++ruleGen_;
}

uint32_t MasterLink::relayCatalogGeneration() { return nameGen_; }

uint8_t MasterLink::relayCatalogCount() { return nameLiveCount_; }

const char *MasterLink::relayCatalogMac(uint8_t slot) {
    if (slot >= nameLiveCount_ || !nameLive_[slot].used) {
        return "";
    }
    return nameLive_[slot].mac;
}

const char *MasterLink::relayCatalogName(uint8_t slot, uint8_t relay) {
    if (slot >= nameLiveCount_ || relay >= 8) {
        return "";
    }
    return nameLive_[slot].names[relay];
}

const char *MasterLink::relayCatalogPlace(const char *mac) {
    if (mac && mac[0]) {
        for (uint8_t i = 0; i < nameLiveCount_; ++i) {
            if (nameLive_[i].used && macEq(nameLive_[i].mac, mac)) {
                return nameLive_[i].place;
            }
        }
        return "";
    }
    if (nameLiveCount_ > 0) {
        return nameLive_[0].place;
    }
    return "";
}

const char *MasterLink::relayDisplayName(const char *mac, uint8_t relay) {
    if (relay >= 8) {
        return "";
    }
    if (mac && mac[0]) {
        for (uint8_t i = 0; i < nameLiveCount_; ++i) {
            if (nameLive_[i].used && macEq(nameLive_[i].mac, mac)) {
                return nameLive_[i].names[relay];
            }
        }
        return "";
    }
    if (nameLiveCount_ > 0) {
        return nameLive_[0].names[relay];
    }
    return "";
}
