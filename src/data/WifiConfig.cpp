#include "WifiConfig.h"
#include "AppLocale.h"
#include <Preferences.h>
#include <WiFi.h>
#include <string.h>

namespace {

constexpr const char *kNs = "wifi_cfg";
Preferences prefs;
bool ready = false;
bool scanStarted = false;
bool connectArmed = false;
bool wasConnected = false;
uint8_t linkState_ = 0;  // 0 idle 1 connecting 2 ok 3 fail
unsigned long connectStartMs = 0;

}  // namespace

namespace WifiConfig {

bool begin() {
    if (ready) {
        return true;
    }
    ready = prefs.begin(kNs, false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(40);
    return ready;
}

bool configured() {
    if (!ready && !begin()) {
        return false;
    }
    return prefs.getBool("cfg", false);
}

void getSsid(char *out, size_t len) {
    if (!out || len == 0) {
        return;
    }
    out[0] = '\0';
    if (!ready && !begin()) {
        return;
    }
    prefs.getString("ssid", out, len);
}

void getPass(char *out, size_t len) {
    if (!out || len == 0) {
        return;
    }
    out[0] = '\0';
    if (!ready && !begin()) {
        return;
    }
    prefs.getString("pass", out, len);
}

bool save(const char *ssid, const char *pass) {
    if (!ssid || ssid[0] == '\0') {
        return false;
    }
    if (!ready && !begin()) {
        return false;
    }
    /* NVS primero; conectar es paso aparte (no mezclar con “guardar”). */
    const size_t nSsid = prefs.putString("ssid", ssid);
    const size_t nPass = prefs.putString("pass", pass ? pass : "");
    const size_t nCfg = prefs.putBool("cfg", true);
    prefs.remove("ap");
    if (nSsid == 0 || nCfg == 0) {
        Serial.println("[WIFI] NVS save failed");
        return false;
    }
    (void)nPass;  /* clave vacía válida (red abierta) */
    Serial.printf("[WIFI] saved ssid='%s'\n", ssid);
    connectSaved();
    return true;
}

bool clear() {
    if (!ready && !begin()) {
        return false;
    }
    prefs.clear();
    WiFi.disconnect(true, true);
    linkState_ = 0;
    wasConnected = false;
    connectArmed = false;
    return true;
}

bool startScan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(20);
    const int16_t r = WiFi.scanNetworks(true, true);
    if (r == WIFI_SCAN_FAILED) {
        scanStarted = false;
        return false;
    }
    scanStarted = true;
    return true;
}

int pollScan(Network *out, int maxN) {
    if (!out || maxN <= 0) {
        return -2;
    }
    const int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        return -1;
    }
    if (n == WIFI_SCAN_FAILED) {
        scanStarted = false;
        return -2;
    }
    if (!scanStarted && n < 0) {
        return -2;
    }

    const int count = (n > maxN) ? maxN : static_cast<int>(n);
    for (int i = 0; i < count; ++i) {
        String s = WiFi.SSID(i);
        strncpy(out[i].ssid, s.c_str(), SSID_MAX - 1);
        out[i].ssid[SSID_MAX - 1] = '\0';
        out[i].rssi = WiFi.RSSI(i);
        out[i].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    scanStarted = false;
    return count;
}

bool connectSaved() {
    if (!configured()) {
        return false;
    }
    char ssid[SSID_MAX];
    char pass[PASS_MAX];
    getSsid(ssid, sizeof(ssid));
    getPass(pass, sizeof(pass));
    if (ssid[0] == '\0') {
        return false;
    }
    /* Abortar scan pendiente antes de conectar. */
    if (scanStarted) {
        WiFi.scanDelete();
        scanStarted = false;
    }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(30);
    WiFi.begin(ssid, pass);
    linkState_ = 1;
    connectArmed = true;
    connectStartMs = millis();
    wasConnected = false;
    Serial.printf("[WIFI] connecting to '%s'\n", ssid);
    return true;
}

void loop() {
    if (!connectArmed && linkState_ != 1) {
        if (WiFi.status() == WL_CONNECTED && !wasConnected) {
            wasConnected = true;
            linkState_ = 2;
            AppLocale::syncNtpIfOnline();
        }
        return;
    }

    const wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
        linkState_ = 2;
        connectArmed = false;
        if (!wasConnected) {
            wasConnected = true;
            AppLocale::syncNtpIfOnline();
            Serial.printf("[WIFI] connected %s\n", WiFi.localIP().toString().c_str());
        }
        return;
    }

    if (linkState_ == 1 && (millis() - connectStartMs) > 20000UL) {
        linkState_ = 3;
        connectArmed = false;
        Serial.println("[WIFI] connect timeout");
    }
}

bool isConnected() { return WiFi.status() == WL_CONNECTED; }

uint8_t linkState() { return linkState_; }

}  // namespace WifiConfig
