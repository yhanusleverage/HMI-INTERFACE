#include "FactoryReset.h"
#include "WifiConfig.h"
#include "Config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>

void FactoryReset::wipeAndReboot() {
    Serial.println("[FACTORY] wiping NVS user data...");

    WifiConfig::begin();
    WifiConfig::clear();

    Preferences prefs;
    if (prefs.begin(PREF_NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
    }
    /* Rules legacy (tick off) — no debe sobrevivir al reset. */
    if (prefs.begin("hmirules", false)) {
        prefs.clear();
        prefs.end();
    }

    WiFi.disconnect(true, true);
    delay(100);
    Serial.println("[FACTORY] reboot");
    ESP.restart();
}
