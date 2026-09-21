#include "FactoryReset.h"
#include "MasterLink.h"
#include "WifiConfig.h"
#include "Config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>

void FactoryReset::wipeAndReboot(bool alsoResetMaster) {
    Serial.println("[FACTORY] wiping NVS user data...");

    if (alsoResetMaster && MasterLink::linkOk()) {
        MasterLink::sendFactoryReset();
        /* Dejar salir el UART antes de borrar/reiniciar HMI. */
        delay(250);
        MasterLink::loop();
        delay(100);
    } else if (alsoResetMaster) {
        Serial.println("[FACTORY] UART sin enlace — solo reset local HMI");
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
