#include "Screens.h"
#include "NavShell.h"
#include "DataStore.h"
#include "MasterLink.h"
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

namespace {

lv_obj_t *uptimeLbl = nullptr;
lv_obj_t *linkLbl = nullptr;
lv_obj_t *bridgeLbl = nullptr;
lv_obj_t *wifiLbl = nullptr;
lv_obj_t *tzLbl = nullptr;
lv_obj_t *aboutLbl = nullptr;
lv_obj_t *masterIdLbl = nullptr;

constexpr lv_coord_t kGap = 6;
constexpr lv_coord_t kSide = 16;

void onBack(lv_event_t *) { NavShell::back(); }

void onRestart(lv_event_t *) {
    UiKit::showRebootSplash(Strings::tr(Msg::RestartBtn));
    if (MasterLink::linkOk()) {
        MasterLink::sendMasterReboot();
        delay(200);
        MasterLink::loop();
    }
    /* También reinicia el display para re-sincronizar UART tras el Master. */
    delay(300);
    ESP.restart();
}

/** Coloca label a la izquierda en y; devuelve y siguiente (bajo el label + gap). */
lv_coord_t placeRow(lv_obj_t *lbl, lv_coord_t y) {
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, kSide, y);
    lv_obj_update_layout(lbl);
    return y + lv_obj_get_height(lbl) + kGap;
}

}  // namespace

lv_obj_t *Screens::createSystem(lv_obj_t *parent) {
    uptimeLbl = linkLbl = bridgeLbl = wifiLbl = tzLbl = aboutLbl = masterIdLbl = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_bottom(root, 16, 0);

    UiKit::styleHeader(root, Strings::tr(Msg::System), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP;

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

    /* Solo lectura: device_id / cloud del Master (registro = Master, no HMI). */
    masterIdLbl = lv_label_create(root);
    lv_label_set_text(masterIdLbl, "Master: --");
    lv_obj_set_style_text_color(masterIdLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(masterIdLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(masterIdLbl, LCD_H_RES - 2 * kSide);
    lv_label_set_long_mode(masterIdLbl, LV_LABEL_LONG_CLIP);
    y = placeRow(masterIdLbl, y);

    y += 4;
    lv_obj_t *restart = UiKit::makeCautionButton(root, Strings::tr(Msg::RestartBtn), onRestart);
    lv_obj_align(restart, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_update_layout(restart);

    lv_obj_t *note = lv_label_create(root);
    lv_label_set_text(note, Strings::tr(Msg::SystemNote));
    UiKit::styleHint(note);
    lv_obj_set_width(note, LCD_H_RES - 2 * kSide);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(note, restart, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    MasterLink::requestSysInfo();
    return root;
}

void Screens::refreshSystem(lv_obj_t *root) {
    (void)root;
    const unsigned long sec = millis() / 1000UL;
    char buf[96];
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
    /* Re-pide de vez en cuando si hay link. */
    static unsigned long lastReqMs = 0;
    if (MasterLink::linkOk() && (millis() - lastReqMs) > 15000UL) {
        lastReqMs = millis();
        MasterLink::requestSysInfo();
    }
}
