#include "Screens.h"
#include "NavShell.h"
#include "DataStore.h"
#include "MasterLink.h"
#include "SlaveInventory.h"
#include "WifiConfig.h"
#include "AppLocale.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/HmiSemantics.h"
#include "theme/UiKit.h"
#include "Config.h"
#include "BoardPins.h"

#include <WiFi.h>
#include <Arduino.h>
#include <ctime>
#include <cstring>

namespace {

lv_obj_t *uptimeLbl = nullptr;
lv_obj_t *readyLbl = nullptr;
bool bootReadyLatched = false;
unsigned long bootReadyAtSec = 0;

bool clockReady() { return time(nullptr) >= 1700000000; }

const char *bootGap() {
    if (!MasterLink::linkOk()) {
        return "UART";
    }
    if (!MasterLink::processBridgeOk()) {
        return "bridge";
    }
    if (!MasterLink::masterSysInfoValid()) {
        return "sys_info";
    }
    if (!MasterLink::masterWifiConnected()) {
        return "Master WiFi";
    }
    if (!WifiConfig::isConnected()) {
        return "HMI WiFi";
    }
    if (!clockReady()) {
        return "reloj";
    }
    return nullptr;
}

void formatBootReady(char *buf, size_t len) {
    if (bootReadyLatched) {
        snprintf(buf, len, "listo en %lu s", bootReadyAtSec);
        return;
    }
    const char *gap = bootGap();
    snprintf(buf, len, "incompleto %lu s · %s", static_cast<unsigned long>(millis() / 1000UL),
             gap ? gap : "reloj");
}
lv_obj_t *linkLbl = nullptr;
lv_obj_t *bridgeLbl = nullptr;
lv_obj_t *wifiLbl = nullptr;
lv_obj_t *tzLbl = nullptr;
lv_obj_t *aboutLbl = nullptr;
lv_obj_t *masterIdLbl = nullptr;
lv_obj_t *masterWifiLbl = nullptr;
lv_obj_t *atlasLbl = nullptr;
lv_obj_t *inventLbl = nullptr;
lv_obj_t *lastCmdLbl = nullptr;
lv_obj_t *profileLbl = nullptr;

constexpr lv_coord_t kGap = 6;
constexpr lv_coord_t kSide = 16;

void onBack(lv_event_t *) { NavShell::back(); }

void onRefreshDebug(lv_event_t *) {
    MasterLink::requestSysInfo();
    MasterLink::requestSlaves();
    /* Dar tiempo a RX antes de volcar Serial. */
    for (int i = 0; i < 15; ++i) {
        MasterLink::loop();
        delay(20);
    }
    Screens::dumpSystemSerial();
}

void onRestart(lv_event_t *) {
    /* TX primero (antes del splash) — 3x por si el Master está ocupado. */
    MasterLink::clearLastCmdAck();
    for (int i = 0; i < 3; ++i) {
        MasterLink::sendMasterReboot();
        MasterLink::flush();
        delay(30);
    }
    UiKit::showRebootSplash(Strings::tr(Msg::RestartBtn));
    const unsigned long t0 = millis();
    while ((millis() - t0) < 2000UL) {
        MasterLink::loop();
        delay(40);
        if (MasterLink::lastCmdAckState() != 0 &&
            strcmp(MasterLink::lastCmdAction(), "master_reboot") == 0) {
            Serial.println("[SYSTEM] master_reboot ack OK — HMI reboot");
            break;
        }
    }
    if (MasterLink::lastCmdAckState() == 0 ||
        strcmp(MasterLink::lastCmdAction(), "master_reboot") != 0) {
        Serial.println("[SYSTEM] WARN: sin ack master_reboot — HMI reboot igual");
    }
    delay(200);
    ESP.restart();
}

lv_coord_t placeRow(lv_obj_t *lbl, lv_coord_t y) {
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, kSide, y);
    lv_obj_update_layout(lbl);
    return y + lv_obj_get_height(lbl) + kGap;
}

lv_obj_t *makeDebugLabel(lv_obj_t *root, lv_coord_t *y) {
    lv_obj_t *lbl = lv_label_create(root);
    lv_label_set_text(lbl, "--");
    lv_obj_set_style_text_color(lbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl, LCD_H_RES - 2 * kSide);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    *y = placeRow(lbl, *y);
    return lbl;
}

}  // namespace

void Screens::dumpSystemSerial() {
    tickBootReady();
    Serial.println();
    Serial.println("+------------- SISTEMA HMI (copia pantalla) -------------+");
    Serial.printf("| FW %s  %dx%d  up=%lus\n", FIRMWARE_VERSION, LCD_H_RES, LCD_V_RES,
                  static_cast<unsigned long>(millis() / 1000UL));
    char readyBuf[48];
    formatBootReady(readyBuf, sizeof(readyBuf));
    Serial.printf("| Listo: %s\n", readyBuf);
    Serial.printf("| UART: %s   bridge: %s\n", MasterLink::linkOk() ? "OK" : "FAIL",
                  MasterLink::processBridgeOk() ? "OK" : "pendiente");

    if (!MasterLink::masterSysInfoValid()) {
        Serial.println("| Master WiFi: -- (sin sys_info)");
    } else if (MasterLink::masterWifiConnected()) {
        Serial.printf("| Master WiFi: OK  %s\n",
                      MasterLink::masterSsid()[0] ? MasterLink::masterSsid() : "?");
    } else if (MasterLink::masterHasWifi()) {
        Serial.printf("| Master WiFi: OFF  %s\n",
                      MasterLink::masterSsid()[0] ? MasterLink::masterSsid() : "?");
    } else {
        Serial.println("| Master WiFi: sin NVS");
    }

    size_t atlasN = 0;
    size_t atlasOn = 0;
    const size_t nT = SlaveInventory::count();
    for (size_t i = 0; i < nT; ++i) {
        const SlaveInventory::Target *t = SlaveInventory::at(i);
        if (!SlaveInventory::isEspNow(t)) {
            continue;
        }
        ++atlasN;
        if (t->online) {
            ++atlasOn;
        }
    }
    if (atlasN == 0) {
        Serial.println("| Atlas: 0 (sin MAC en UART)  <- hub offline esperado");
    } else {
        const size_t hubIx = SlaveInventory::firstEspNowIndex();
        const SlaveInventory::Target *hub =
            hubIx != SIZE_MAX ? SlaveInventory::at(hubIx) : nullptr;
        Serial.printf("| Atlas: %u/%u online  hub=%s\n", static_cast<unsigned>(atlasOn),
                      static_cast<unsigned>(atlasN),
                      hub && hub->mac[0] ? hub->mac : "?");
        for (size_t i = 0; i < nT; ++i) {
            const SlaveInventory::Target *t = SlaveInventory::at(i);
            if (!SlaveInventory::isEspNow(t)) {
                continue;
            }
            Serial.printf("|   %s  %s  %s  relays=%u\n", t->online ? "ON " : "OFF", t->mac,
                          t->name[0] ? t->name : "-", static_cast<unsigned>(t->numRelays));
        }
        for (size_t i = 0; i < nT; ++i) {
            const SlaveInventory::Target *t = SlaveInventory::at(i);
            if (!t || !t->local) {
                continue;
            }
            Serial.printf("|   local  %s  %s  relays=%u\n", t->mac, t->name[0] ? t->name : "-",
                          static_cast<unsigned>(t->numRelays));
        }
    }

    const unsigned long last = SlaveInventory::lastUpdateMs();
    if (last == 0) {
        Serial.println("| Inventario: nunca — pulse Actualizar en Sistema");
    } else {
        Serial.printf("| Inventario: hace %lus  n=%u\n",
                      static_cast<unsigned long>((millis() - last) / 1000UL),
                      static_cast<unsigned>(SlaveInventory::count()));
    }

    const char *act = MasterLink::lastCmdAction();
    const uint8_t st = MasterLink::lastCmdAckState();
    if (!act[0] || st == 0) {
        Serial.println("| Ultimo cmd: --");
    } else {
        Serial.printf("| Ultimo cmd: %s ok=%d\n", act, st == 1 ? 1 : 0);
    }

    const char *dn = MasterLink::masterDeviceName();
    const char *loc = MasterLink::masterLocation();
    Serial.printf("| Perfil: %s | %s\n", dn && dn[0] ? dn : "-", loc && loc[0] ? loc : "-");

    if (MasterLink::masterSysInfoValid() && MasterLink::masterDeviceId()[0]) {
        Serial.printf("| Master id: %s  cloud=%s\n", MasterLink::masterDeviceId(),
                      MasterLink::masterCloudOk() ? "OK" : "--");
    } else {
        Serial.println("| Master id: --");
    }

    if (WifiConfig::isConnected()) {
        Serial.printf("| HMI WiFi: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("| HMI WiFi: --");
    }

    Serial.println("| Actualizar en Sistema | o escribi: view");
    Serial.println("+----------------------------------------------------------+");
    Serial.println();
}

lv_obj_t *Screens::createSystem(lv_obj_t *parent) {
    uptimeLbl = readyLbl = linkLbl = bridgeLbl = wifiLbl = tzLbl = aboutLbl = masterIdLbl = nullptr;
    masterWifiLbl = atlasLbl = inventLbl = lastCmdLbl = profileLbl = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_bottom(root, 16, 0);

    UiKit::styleHeader(root, Strings::tr(Msg::System), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP;

    readyLbl = lv_label_create(root);
    lv_label_set_text(readyLbl, "incompleto 0 s");
    lv_obj_set_style_text_color(readyLbl, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(readyLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(readyLbl, LCD_H_RES - 2 * kSide);
    lv_label_set_long_mode(readyLbl, LV_LABEL_LONG_CLIP);
    y = placeRow(readyLbl, y);

    aboutLbl = lv_label_create(root);
    char abuf[96];
    snprintf(abuf, sizeof(abuf), "%s\nFW %s\nIPS %d x %d", Strings::tr(Msg::AboutTitle),
             FIRMWARE_VERSION, LCD_H_RES, LCD_V_RES);
    lv_label_set_text(aboutLbl, abuf);
    lv_obj_set_style_text_color(aboutLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(aboutLbl, &lv_font_montserrat_16, 0);
    y = placeRow(aboutLbl, y);

    linkLbl = lv_label_create(root);
    lv_label_set_text(linkLbl, Strings::tr(Msg::UartFail));
    lv_obj_set_style_text_color(linkLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(linkLbl, &lv_font_montserrat_14, 0);
    y = placeRow(linkLbl, y);

    bridgeLbl = lv_label_create(root);
    lv_label_set_text(bridgeLbl, Strings::tr(Msg::ProcessBridgePending));
    lv_obj_set_style_text_color(bridgeLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(bridgeLbl, &lv_font_montserrat_14, 0);
    y = placeRow(bridgeLbl, y);

    masterWifiLbl = makeDebugLabel(root, &y);
    {
        lv_obj_t *lbl = lv_label_create(root);
        lv_label_set_text(lbl, "Atlas: --\n--\n--\n--\n--");
        lv_obj_set_style_text_color(lbl, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_width(lbl, LCD_H_RES - 2 * kSide);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        y = placeRow(lbl, y);
        atlasLbl = lbl;
    }
    inventLbl = makeDebugLabel(root, &y);
    lastCmdLbl = makeDebugLabel(root, &y);
    profileLbl = makeDebugLabel(root, &y);

    wifiLbl = lv_label_create(root);
    {
        char wbuf[48];
        snprintf(wbuf, sizeof(wbuf), Strings::tr(Msg::WifiStatusFmt), "--");
        lv_label_set_text(wifiLbl, wbuf);
    }
    lv_obj_set_style_text_color(wifiLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(wifiLbl, &lv_font_montserrat_14, 0);
    y = placeRow(wifiLbl, y);

    tzLbl = lv_label_create(root);
    lv_label_set_text(tzLbl, "--");
    lv_obj_set_style_text_color(tzLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(tzLbl, &lv_font_montserrat_14, 0);
    y = placeRow(tzLbl, y);

    uptimeLbl = lv_label_create(root);
    lv_label_set_text(uptimeLbl, "--");
    lv_obj_set_style_text_color(uptimeLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(uptimeLbl, &lv_font_montserrat_14, 0);
    y = placeRow(uptimeLbl, y);

    masterIdLbl = lv_label_create(root);
    lv_label_set_text(masterIdLbl, "Master: --");
    lv_obj_set_style_text_color(masterIdLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(masterIdLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(masterIdLbl, LCD_H_RES - 2 * kSide);
    lv_label_set_long_mode(masterIdLbl, LV_LABEL_LONG_CLIP);
    y = placeRow(masterIdLbl, y);

    y += 4;
    lv_obj_t *refresh =
        UiKit::makeSecondaryButton(root, Strings::tr(Msg::RulesRefresh), 120, AppTheme::BACK_H,
                                   onRefreshDebug);
    lv_obj_align(refresh, LV_ALIGN_TOP_LEFT, kSide, y);

    lv_obj_t *restart = UiKit::makeCautionButton(root, Strings::tr(Msg::RestartBtn), onRestart);
    lv_obj_align(restart, LV_ALIGN_TOP_RIGHT, -kSide, y);
    lv_obj_update_layout(restart);
    const lv_coord_t btnBottom = y + lv_obj_get_height(restart);

    lv_obj_t *note = lv_label_create(root);
    lv_label_set_text(note, Strings::tr(Msg::SystemNote));
    UiKit::styleHint(note);
    lv_obj_set_width(note, LCD_H_RES - 2 * kSide);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, btnBottom + 8);

    MasterLink::requestSysInfo();
    MasterLink::requestSlaves();
    Screens::dumpSystemSerial();
    return root;
}

void Screens::tickBootReady() {
    if (bootReadyLatched || bootGap() != nullptr) {
        return;
    }
    bootReadyLatched = true;
    bootReadyAtSec = millis() / 1000UL;
}

void Screens::refreshSystem(lv_obj_t *root) {
    (void)root;
    tickBootReady();
    const unsigned long sec = millis() / 1000UL;
    char buf[128];
    if (readyLbl) {
        formatBootReady(buf, sizeof(buf));
        lv_label_set_text(readyLbl, buf);
        lv_obj_set_style_text_color(readyLbl, bootReadyLatched ? AppTheme::muted() : AppTheme::warn(),
                                    0);
    }
    snprintf(buf, sizeof(buf), Strings::tr(Msg::UptimeFmt), sec / 3600UL, (sec / 60UL) % 60UL);
    if (uptimeLbl) {
        lv_label_set_text(uptimeLbl, buf);
    }
    if (linkLbl) {
        const bool ok = MasterLink::linkOk();
        lv_label_set_text(linkLbl, ok ? Strings::tr(Msg::UartOk) : Strings::tr(Msg::UartFail));
        lv_obj_set_style_text_color(linkLbl, HmiSemantics::linkLabel(ok), 0);
    }
    if (bridgeLbl) {
        const bool ok = MasterLink::processBridgeOk();
        lv_label_set_text(bridgeLbl, ok ? Strings::tr(Msg::ProcessBridgeOk)
                                        : Strings::tr(Msg::ProcessBridgePending));
        lv_obj_set_style_text_color(bridgeLbl, ok ? AppTheme::muted() : AppTheme::warn(), 0);
    }

    if (masterWifiLbl) {
        if (!MasterLink::masterSysInfoValid()) {
            lv_label_set_text(masterWifiLbl, "Master WiFi: --");
            lv_obj_set_style_text_color(masterWifiLbl, AppTheme::muted(), 0);
        } else if (MasterLink::masterWifiConnected()) {
            snprintf(buf, sizeof(buf), "Master WiFi: OK  %s",
                     MasterLink::masterSsid()[0] ? MasterLink::masterSsid() : "?");
            lv_label_set_text(masterWifiLbl, buf);
            lv_obj_set_style_text_color(masterWifiLbl, HmiSemantics::linkLabel(true), 0);
        } else if (MasterLink::masterHasWifi()) {
            snprintf(buf, sizeof(buf), "Master WiFi: OFF  %s",
                     MasterLink::masterSsid()[0] ? MasterLink::masterSsid() : "?");
            lv_label_set_text(masterWifiLbl, buf);
            lv_obj_set_style_text_color(masterWifiLbl, AppTheme::warn(), 0);
        } else {
            lv_label_set_text(masterWifiLbl, "Master WiFi: sin NVS");
            lv_obj_set_style_text_color(masterWifiLbl, AppTheme::muted(), 0);
        }
    }

    size_t atlasN = 0;
    size_t atlasOn = 0;
    const size_t nT = SlaveInventory::count();
    for (size_t i = 0; i < nT; ++i) {
        const SlaveInventory::Target *t = SlaveInventory::at(i);
        if (!SlaveInventory::isEspNow(t)) {
            continue;
        }
        ++atlasN;
        if (t->online) {
            ++atlasOn;
        }
    }
    if (atlasLbl) {
        char atlasBuf[420];
        if (atlasN == 0) {
            snprintf(atlasBuf, sizeof(atlasBuf), "Atlas: 0 (sin MAC en UART)");
            lv_obj_set_style_text_color(atlasLbl, AppTheme::warn(), 0);
        } else {
            const size_t hubIx = SlaveInventory::firstEspNowIndex();
            const SlaveInventory::Target *hub =
                hubIx != SIZE_MAX ? SlaveInventory::at(hubIx) : nullptr;
            int n = snprintf(atlasBuf, sizeof(atlasBuf), "Atlas %u/%u online  hub %s",
                             static_cast<unsigned>(atlasOn), static_cast<unsigned>(atlasN),
                             hub && hub->mac[0] ? hub->mac : "?");
            for (size_t i = 0; i < nT && n > 0 && n < static_cast<int>(sizeof(atlasBuf) - 1); ++i) {
                const SlaveInventory::Target *t = SlaveInventory::at(i);
                if (!SlaveInventory::isEspNow(t)) {
                    continue;
                }
                n += snprintf(atlasBuf + n, sizeof(atlasBuf) - static_cast<size_t>(n),
                              "\n%s %s %s", t->online ? "ON " : "OFF", t->mac,
                              t->name[0] ? t->name : "-");
            }
            for (size_t i = 0; i < nT && n > 0 && n < static_cast<int>(sizeof(atlasBuf) - 1); ++i) {
                const SlaveInventory::Target *t = SlaveInventory::at(i);
                if (!t || !t->local) {
                    continue;
                }
                n += snprintf(atlasBuf + n, sizeof(atlasBuf) - static_cast<size_t>(n),
                              "\nlocal %s %s", t->mac, t->name[0] ? t->name : "-");
            }
            lv_obj_set_style_text_color(atlasLbl,
                                        atlasOn > 0 ? HmiSemantics::linkLabel(true) : AppTheme::warn(),
                                        0);
        }
        lv_label_set_text(atlasLbl, atlasBuf);
    }

    if (inventLbl) {
        const unsigned long last = SlaveInventory::lastUpdateMs();
        if (last == 0) {
            lv_label_set_text(inventLbl, "Inventario: nunca (Actualizar)");
            lv_obj_set_style_text_color(inventLbl, AppTheme::warn(), 0);
        } else {
            const unsigned long ageS = (millis() - last) / 1000UL;
            snprintf(buf, sizeof(buf), "Inventario: hace %lus  n=%u", ageS,
                     static_cast<unsigned>(SlaveInventory::count()));
            lv_label_set_text(inventLbl, buf);
            lv_obj_set_style_text_color(inventLbl, AppTheme::muted(), 0);
        }
    }

    if (lastCmdLbl) {
        const char *act = MasterLink::lastCmdAction();
        const uint8_t st = MasterLink::lastCmdAckState();
        if (!act[0] || st == 0) {
            lv_label_set_text(lastCmdLbl, "Ultimo cmd: --");
            lv_obj_set_style_text_color(lastCmdLbl, AppTheme::muted(), 0);
        } else {
            snprintf(buf, sizeof(buf), "Ultimo cmd: %s ok=%d", act, st == 1 ? 1 : 0);
            lv_label_set_text(lastCmdLbl, buf);
            lv_obj_set_style_text_color(lastCmdLbl, st == 1 ? AppTheme::muted() : AppTheme::warn(),
                                        0);
        }
    }

    if (profileLbl) {
        const char *dn = MasterLink::masterDeviceName();
        const char *loc = MasterLink::masterLocation();
        if ((!dn || !dn[0]) && (!loc || !loc[0])) {
            lv_label_set_text(profileLbl, "Perfil Master: --");
        } else {
            snprintf(buf, sizeof(buf), "Perfil: %s | %s", dn && dn[0] ? dn : "-",
                     loc && loc[0] ? loc : "-");
            lv_label_set_text(profileLbl, buf);
        }
        lv_obj_set_style_text_color(profileLbl, AppTheme::muted(), 0);
    }

    if (wifiLbl) {
        if (WifiConfig::isConnected()) {
            snprintf(buf, sizeof(buf), Strings::tr(Msg::WifiStatusFmt),
                     WiFi.localIP().toString().c_str());
            lv_label_set_text(wifiLbl, buf);
            lv_obj_set_style_text_color(wifiLbl, HmiSemantics::linkLabel(true), 0);
        } else {
            const uint8_t st = WifiConfig::linkState();
            const char *msg = Strings::tr(Msg::WifiConnectFail);
            if (st == 1) {
                msg = Strings::tr(Msg::WifiConnecting);
            } else if (st == 0 && !WifiConfig::configured()) {
                msg = Strings::tr(Msg::WifiPickHint);
            }
            snprintf(buf, sizeof(buf), Strings::tr(Msg::WifiStatusFmt), msg);
            lv_label_set_text(wifiLbl, buf);
            lv_obj_set_style_text_color(wifiLbl, AppTheme::muted(), 0);
        }
    }
    if (tzLbl) {
        char tz[48];
        AppLocale::formatTzLabel(tz, sizeof(tz), AppLocale::tzOffsetMin());
        snprintf(buf, sizeof(buf), "%s: %s", Strings::tr(Msg::TimeZoneMenu), tz);
        lv_label_set_text(tzLbl, buf);
    }
    if (masterIdLbl) {
        if (MasterLink::masterSysInfoValid() && MasterLink::masterDeviceId()[0]) {
            snprintf(buf, sizeof(buf), "Master: %s · cloud %s", MasterLink::masterDeviceId(),
                     MasterLink::masterCloudOk() ? "OK" : "--");
        } else {
            snprintf(buf, sizeof(buf), "Master: --");
        }
        lv_label_set_text(masterIdLbl, buf);
    }
    static unsigned long lastReqMs = 0;
    static unsigned long lastDumpMs = 0;
    if (MasterLink::linkOk() && (millis() - lastReqMs) > 15000UL) {
        lastReqMs = millis();
        MasterLink::requestSysInfo();
        MasterLink::requestSlaves();
    }
    /* Copia periódica al Serial USB de la HMI (cada 3 s en pantalla Sistema). */
    if (lastDumpMs == 0 || (millis() - lastDumpMs) > 3000UL) {
        lastDumpMs = millis();
        Screens::dumpSystemSerial();
    }
}
