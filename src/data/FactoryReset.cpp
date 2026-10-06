#include "FactoryReset.h"
#include "MasterLink.h"
#include "WifiConfig.h"
#include "Config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>

void FactoryReset::wipeAndReboot(bool alsoResetMaster) {
    Serial.println("[FACTORY] wiping NVS user data...");

    if (alsoResetMaster) {
        /* Siempre intentar UART (linkOk false no debe saltar el Master). */
        MasterLink::sendFactoryReset();
        MasterLink::flush();
        const unsigned long t0 = millis();
        while ((millis() - t0) < 800UL) {
            MasterLink::loop();
            delay(40);
        }
        delay(200);
    }

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
    Serial.println("[FACTORY] reboot HMI");
    ESP.restart();
}
