#include <Arduino.h>
#include "Config.h"
#include "DisplayHal.h"
#include "NavShell.h"
#include "DataStore.h"
#include "Simulator.h"
#include "MasterLink.h"
#include "BoardPins.h"
#include "UiMirror.h"
#include "AppLocale.h"
#include "WifiConfig.h"
#include "NutrientConfig.h"
#include "RelayAliasConfig.h"
#include "DisplayConfig.h"
#include "UnitsConfig.h"
#include "ReservoirConfig.h"
#include "PumpConfig.h"
#include "RulesConfig.h"
#include "RulesEngine.h"
#include "RelayCycleConfig.h"
#include "RelayActuationLock.h"
#include "BacklightIdle.h"

static char cmdBuf[48];
static size_t cmdLen = 0;

static void printHelp() {
    Serial.println();
    Serial.println("===== CMD SERIAL (Enter) =====");
    Serial.println("  help / status / test / bl0 / bl1");
    Serial.println("  red|green|blue|white|black / ui");
    Serial.println("  menu / back   - navegar UI sin touch");
    Serial.println("  view          - espejo UNA vez (lo que ve el user)");
    Serial.println("  mirror on|off - espejo automatico cada 2s");
    Serial.println("==============================");
}

static void handleCmd(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        printHelp();
    } else if (strcmp(cmd, "status") == 0) {
        DisplayHal::dumpStatus();
        UiMirror::dump();
    } else if (strcmp(cmd, "view") == 0 || strcmp(cmd, "mirror") == 0) {
        UiMirror::dump();
    } else if (strcmp(cmd, "mirror on") == 0) {
        UiMirror::setAuto(true);
    } else if (strcmp(cmd, "mirror off") == 0) {
        UiMirror::setAuto(false);
    } else if (strcmp(cmd, "test") == 0) {
        DisplayHal::runSelfTest();
    } else if (strcmp(cmd, "bl0") == 0) {
        BacklightIdle::setMode(BacklightIdle::Mode::ForcedOff);
    } else if (strcmp(cmd, "bl1") == 0) {
        BacklightIdle::setMode(BacklightIdle::Mode::AlwaysOn);
    } else if (strcmp(cmd, "red") == 0) {
        DisplayHal::fillRgb(255, 0, 0);
    } else if (strcmp(cmd, "green") == 0) {
        DisplayHal::fillRgb(0, 255, 0);
    } else if (strcmp(cmd, "blue") == 0) {
        DisplayHal::fillRgb(0, 0, 255);
    } else if (strcmp(cmd, "white") == 0) {
        DisplayHal::fillRgb(255, 255, 255);
    } else if (strcmp(cmd, "black") == 0) {
        DisplayHal::fillRgb(0, 0, 0);
    } else if (strcmp(cmd, "ui") == 0) {
        if (DisplayHal::ready()) {
            NavShell::goTo(ScreenId::Central);
            Serial.println("[UI] -> Central");
            UiMirror::dump();
        } else {
            Serial.println("[UI] display no ready");
        }
    } else if (strcmp(cmd, "menu") == 0) {
        if (DisplayHal::ready()) {
            NavShell::goTo(ScreenId::Settings);
            Serial.println("[UI] -> Settings");
            UiMirror::dump();
        } else {
            Serial.println("[UI] display no ready");
        }
    } else if (strcmp(cmd, "back") == 0) {
        if (DisplayHal::ready()) {
            NavShell::back();
            Serial.println("[UI] <- back");
            UiMirror::dump();
        } else {
            Serial.println("[UI] display no ready");
        }
    } else if (cmd[0] != '\0') {
        Serial.printf("[CMD] desconocido: '%s' (help)\n", cmd);
    }
}

static void pollSerialCmds() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            cmdBuf[cmdLen] = '\0';
            handleCmd(cmdBuf);
            cmdLen = 0;
            continue;
        }
        if (cmdLen + 1 < sizeof(cmdBuf)) {
            cmdBuf[cmdLen++] = c;
        } else {
            cmdLen = 0;
        }
    }
}

void setup() {
    Serial.begin(115200);
    const unsigned long t0 = millis();
    while (!Serial && (millis() - t0) < 3000) {
        delay(10);
    }
    delay(200);

    Serial.println();
    Serial.println("========================================");
    Serial.printf(" %s  %s\n", SYSTEM_NAME, FIRMWARE_VERSION);
    Serial.printf(" Panel IPS %dx%d | USB CDC\n", LCD_H_RES, LCD_V_RES);
    Serial.printf(" Chip: %s  PSRAM: %u KB\n",
                  ESP.getChipModel(),
                  ESP.getPsramSize() / 1024);
    Serial.println(" Espejo UI: mirror on (default)");
    Serial.println("========================================");
    printHelp();

    DataStore::instance().begin();
    AppLocale::begin();
    PumpConfig::begin();
    NutrientConfig::begin();
    RelayAliasConfig::begin();
    DisplayConfig::begin();
    UnitsConfig::begin();
    ReservoirConfig::begin();
    RulesConfig::begin();
    RulesEngine::begin();
    /* Rules SI→ENTONCES fuera del HMI (web futuro); no tick. */
    RelayActuationLock::begin();
    RelayCycleConfig::begin();
    WifiConfig::begin();
#if !UART_BENCH
    if (WifiConfig::configured()) {
        WifiConfig::connectSaved();
    }
    Simulator::begin();
#else
    Serial.println("[UART BENCH] HMI — solo MasterLink + UI + logs RX/TX (sin WiFi/SIM)");
#endif
    MasterLink::begin();
    NutrientConfig::syncToMaster();
    ReservoirConfig::syncToMaster();

    if (!DisplayHal::begin()) {
        Serial.println("[FATAL] display no inicio");
    } else {
        BacklightIdle::begin();
        NavShell::begin();
        Serial.println(AppLocale::setupDone() ? "[UI] central lista" : "[UI] wizard setup");
        UiMirror::dump();
    }
}

void loop() {
    pollSerialCmds();
#if !UART_BENCH
    Simulator::tick();
#endif
    MasterLink::loop();
#if !UART_BENCH
    // RulesEngine::tick(); /* desactivado — Rules fuera del HMI */
    RelayCycleConfig::tick();
    WifiConfig::loop();
#endif
    if (DisplayHal::ready()) {
        NavShell::tick();
        DisplayHal::loop();
    }
    UiMirror::tick(millis());
    delay(5);
}
