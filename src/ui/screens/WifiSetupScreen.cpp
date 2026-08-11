#include "Screens.h"
#include "NavShell.h"
#include "WifiConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <WiFi.h>
#include <cstdio>
#include <cstring>

/**
 * WiFi landscape: lista al escanear; al elegir red → lista desaparece,
 * solo SSID seleccionado + clave + estado (paso / guardando / IP).
 */

namespace {

constexpr lv_coord_t kHeaderH = 36;
constexpr lv_coord_t kStatusH = 22;
constexpr lv_coord_t kFooterH = 52;
constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kPad = 6;
constexpr lv_coord_t kBodyTop = kHeaderH + kStatusH;

lv_obj_t *rootScr = nullptr;
lv_obj_t *listHost = nullptr;
lv_obj_t *pickedHost = nullptr;
lv_obj_t *pickedNameLbl = nullptr;
lv_obj_t *statusLbl = nullptr;
lv_obj_t *selLbl = nullptr;
lv_obj_t *passTa = nullptr;
lv_obj_t *kb = nullptr;
lv_obj_t *scanBtn = nullptr;
lv_obj_t *saveBtn = nullptr;
lv_obj_t *titleLbl = nullptr;
lv_obj_t *stepLbl = nullptr;

char selectedSsid[WifiConfig::SSID_MAX] = {};
bool scanning = false;
bool listHidden = false;

void hideKb();
void layoutBody();
void rebuildList(const WifiConfig::Network *nets, int n);
void setStatus(const char *msg, bool ok);
void showListMode();
void showPickedMode();
void clearListChildren();

void hideKb() {
    if (kb) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    layoutBody();
}

void showKb() {
    if (!kb || !passTa) {
        return;
    }
    lv_keyboard_set_textarea(kb, passTa);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
    layoutBody();
}

void layoutBody() {
    if (!rootScr) {
        return;
    }
    const bool kbVisible = kb && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN);
    const lv_coord_t bottomReserve = kFooterH + (kbVisible ? kKbH : 0);
    const lv_coord_t bodyH = LCD_V_RES - kBodyTop - bottomReserve;

    if (listHost) {
        lv_obj_set_size(listHost, LCD_H_RES, bodyH > 40 ? bodyH : 40);
        lv_obj_set_pos(listHost, 0, kBodyTop);
    }
    if (pickedHost) {
        lv_obj_set_size(pickedHost, LCD_H_RES - 2 * kPad, bodyH > 60 ? bodyH : 60);
        lv_obj_set_pos(pickedHost, kPad, kBodyTop);
    }

    if (selLbl) {
        lv_obj_align(selLbl, LV_ALIGN_BOTTOM_LEFT, kPad, kbVisible ? -(kKbH + 28) : -28);
    }
    if (passTa) {
        lv_obj_align(passTa, LV_ALIGN_BOTTOM_LEFT, kPad, kbVisible ? -(kKbH + 4) : -4);
    }
    if (saveBtn) {
        lv_obj_align(saveBtn, LV_ALIGN_BOTTOM_RIGHT, -kPad, kbVisible ? -(kKbH + 4) : -4);
    }
}

void setStatus(const char *msg, bool ok) {
    if (!statusLbl) {
        return;
    }
    lv_label_set_text(statusLbl, msg ? msg : "");
    lv_obj_set_style_text_color(statusLbl, ok ? AppTheme::text() : AppTheme::muted(), 0);
}

void updateSelLabel() {
    if (!selLbl) {
        return;
    }
    if (selectedSsid[0] == '\0') {
        lv_label_set_text(selLbl, Strings::tr(Msg::WifiChooseNet));
        lv_obj_set_style_text_color(selLbl, AppTheme::muted(), 0);
    } else {
        char buf[56];
        snprintf(buf, sizeof(buf), "%s: %s", Strings::tr(Msg::WifiRed), selectedSsid);
        lv_label_set_text(selLbl, buf);
        lv_obj_set_style_text_color(selLbl, AppTheme::text(), 0);
    }
}

void showListMode() {
    listHidden = false;
    if (listHost) {
        lv_obj_clear_flag(listHost, LV_OBJ_FLAG_HIDDEN);
    }
    if (pickedHost) {
        lv_obj_add_flag(pickedHost, LV_OBJ_FLAG_HIDDEN);
    }
    layoutBody();
}

void showPickedMode() {
    listHidden = true;
    if (listHost) {
        lv_obj_add_flag(listHost, LV_OBJ_FLAG_HIDDEN);
        clearListChildren();
    }
    if (pickedHost) {
        lv_obj_clear_flag(pickedHost, LV_OBJ_FLAG_HIDDEN);
    }
    if (pickedNameLbl) {
        lv_label_set_text(pickedNameLbl, selectedSsid[0] ? selectedSsid : "--");
    }
    layoutBody();
}

void clearListChildren() {
    if (!listHost) {
        return;
    }
    lv_obj_clean(listHost);
}

void onBack(lv_event_t *) {
    hideKb();
    NavShell::back();
}

void onContinue(lv_event_t *) {
    hideKb();
    NavShell::wizardContinue();
}

void onSkip(lv_event_t *) {
    hideKb();
    NavShell::wizardContinue();
}

void onPassFocus(lv_event_t *) { showKb(); }

void onKbDone(lv_event_t *) { hideKb(); }

void paintLinkStatus() {
    if (scanning) {
        return;
    }
    const uint8_t st = WifiConfig::linkState();
    if (st == 1) {
        setStatus(Strings::tr(Msg::WifiConnecting), false);
        return;
    }
    if (WifiConfig::isConnected()) {
        char buf[56];
        snprintf(buf, sizeof(buf), "%s  %s", Strings::tr(Msg::WifiConnected),
                 WiFi.localIP().toString().c_str());
        setStatus(buf, true);
        return;
    }
    if (st == 3) {
        setStatus(Strings::tr(Msg::WifiConnectFail), false);
    }
}

void onSave(lv_event_t *) {
    hideKb();
    if (selectedSsid[0] == '\0') {
        setStatus(Strings::tr(Msg::WifiSelectFirst), false);
        return;
    }
    const char *pass = passTa ? lv_textarea_get_text(passTa) : "";
    setStatus(Strings::tr(Msg::WifiSavedOk), true);
    if (!WifiConfig::save(selectedSsid, pass ? pass : "")) {
        setStatus(Strings::tr(Msg::WifiSaveErr), false);
        return;
    }
    setStatus(Strings::tr(Msg::WifiConnecting), false);
}

void onPickNet(lv_event_t *e) {
    const char *ssid = static_cast<const char *>(lv_event_get_user_data(e));
    if (!ssid) {
        return;
    }
    strncpy(selectedSsid, ssid, sizeof(selectedSsid) - 1);
    selectedSsid[sizeof(selectedSsid) - 1] = '\0';
    updateSelLabel();
    showPickedMode();
    setStatus(Strings::tr(Msg::WifiWritePass), true);
    if (passTa) {
        lv_obj_clear_state(passTa, LV_STATE_DISABLED);
        showKb();
    }
}

static const char *rssiBars(int32_t rssi) {
    if (rssi >= -55) {
        return "****";
    }
    if (rssi >= -65) {
        return "***";
    }
    if (rssi >= -75) {
        return "**";
    }
    return "*";
}

void rebuildList(const WifiConfig::Network *nets, int n) {
    showListMode();
    clearListChildren();
    if (n <= 0) {
        lv_obj_t *empty = lv_label_create(listHost);
        lv_label_set_text(empty, Strings::tr(Msg::WifiNoNets));
        lv_obj_set_style_text_color(empty, AppTheme::muted(), 0);
        lv_obj_set_style_pad_all(empty, 12, 0);
        return;
    }

    static char ssidPool[WifiConfig::SCAN_MAX][WifiConfig::SSID_MAX];
    for (int i = 0; i < n; ++i) {
        if (nets[i].ssid[0] == '\0') {
            continue;
        }
        lv_obj_t *row = lv_btn_create(listHost);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, LCD_H_RES - 2 * kPad);
        lv_obj_set_height(row, 40);
        UiKit::forceOpaqueBg(row, AppTheme::surface());
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
        lv_obj_set_style_pad_left(row, 10, 0);
        lv_obj_set_style_pad_right(row, 10, 0);
        lv_obj_set_style_radius(row, 0, 0);
        UiKit::applyPressStyle(row, AppTheme::surface(), AppTheme::surfaceAlt(), AppTheme::gridLine(),
                               AppTheme::accent());

        strncpy(ssidPool[i], nets[i].ssid, WifiConfig::SSID_MAX - 1);
        ssidPool[i][WifiConfig::SSID_MAX - 1] = '\0';
        lv_obj_add_event_cb(row, onPickNet, LV_EVENT_CLICKED, ssidPool[i]);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, nets[i].ssid);
        lv_obj_set_style_text_color(name, AppTheme::text(), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        char meta[24];
        snprintf(meta, sizeof(meta), "%s %s", rssiBars(nets[i].rssi),
                 nets[i].open ? Strings::tr(Msg::WifiOpen) : Strings::tr(Msg::WifiLock));
        lv_obj_t *metaLbl = lv_label_create(row);
        lv_label_set_text(metaLbl, meta);
        lv_obj_set_style_text_color(metaLbl, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(metaLbl, &lv_font_montserrat_12, 0);
        lv_obj_align(metaLbl, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void onScan(lv_event_t *) {
    hideKb();
    if (scanning) {
        return;
    }
    /* Nueva busqueda: volver a modo lista. */
    selectedSsid[0] = '\0';
    updateSelLabel();
    showListMode();
    if (!WifiConfig::startScan()) {
        setStatus(Strings::tr(Msg::WifiScanFail), false);
        return;
    }
    scanning = true;
    setStatus(Strings::tr(Msg::WifiScanning), false);
    if (scanBtn) {
        lv_obj_add_state(scanBtn, LV_STATE_DISABLED);
    }
}

}  // namespace

lv_obj_t *Screens::createWifiSetup(lv_obj_t *parent) {
    WifiConfig::begin();
    selectedSsid[0] = '\0';
    scanning = false;
    listHidden = false;

    rootScr = lv_obj_create(parent);
    UiKit::styleScreen(rootScr);
    lv_obj_clear_flag(rootScr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr = lv_obj_create(rootScr);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, LCD_H_RES, kHeaderH);
    lv_obj_set_pos(hdr, 0, 0);
    UiKit::forceOpaqueBg(hdr, AppTheme::bg());

    if (!NavShell::inWizard()) {
        lv_obj_t *back = UiKit::makeBackButton(hdr, onBack);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    }

    titleLbl = lv_label_create(hdr);
    lv_label_set_text(titleLbl, Strings::tr(Msg::Wifi));
    UiKit::styleTitle(titleLbl);
    lv_obj_align(titleLbl, LV_ALIGN_LEFT_MID, NavShell::inWizard() ? kPad : AppTheme::BACK_W + 4, 0);

    stepLbl = nullptr;
    if (NavShell::inWizard()) {
        stepLbl = lv_label_create(hdr);
        char sbuf[24];
        snprintf(sbuf, sizeof(sbuf), Strings::tr(Msg::WizardStep), NavShell::wizardSetupStep(), 4);
        lv_label_set_text(stepLbl, sbuf);
        lv_obj_set_style_text_color(stepLbl, AppTheme::text(), 0);
        lv_obj_set_style_text_font(stepLbl, &lv_font_montserrat_14, 0);
        lv_obj_align_to(stepLbl, titleLbl, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    }

    scanBtn = UiKit::makeSecondaryButton(hdr, Strings::tr(Msg::WifiScan), 90, 32, onScan);
    if (NavShell::inWizard()) {
        lv_obj_t *cont = UiKit::makePrimaryButton(hdr, Strings::tr(Msg::Continue), onContinue);
        lv_obj_set_size(cont, 100, 32);
        lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -kPad, 0);
        lv_obj_t *sk = UiKit::makeSecondaryButton(hdr, Strings::tr(Msg::Skip), 70, 32, onSkip);
        lv_obj_align(sk, LV_ALIGN_RIGHT_MID, -108, 0);
        lv_obj_align(scanBtn, LV_ALIGN_RIGHT_MID, -186, 0);
    } else {
        lv_obj_align(scanBtn, LV_ALIGN_RIGHT_MID, -kPad, 0);
    }

    /* Barra de estado a ancho completo: clave / guardando / IP (no pelea con Paso 2/3). */
    lv_obj_t *statusBar = lv_obj_create(rootScr);
    lv_obj_remove_style_all(statusBar);
    lv_obj_set_size(statusBar, LCD_H_RES, kStatusH);
    lv_obj_set_pos(statusBar, 0, kHeaderH);
    UiKit::forceOpaqueBg(statusBar, AppTheme::surfaceAlt());

    statusLbl = lv_label_create(statusBar);
    lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_width(statusLbl, LCD_H_RES - 2 * kPad);
    lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(statusLbl, LV_ALIGN_LEFT_MID, kPad, 0);
    if (NavShell::inWizard()) {
        setStatus(Strings::tr(Msg::WizardWifiHint), false);
    } else {
        setStatus(WifiConfig::configured() ? Strings::tr(Msg::WifiSavedNet) : Strings::tr(Msg::WifiPickHint),
                  WifiConfig::configured());
    }

    listHost = lv_obj_create(rootScr);
    lv_obj_remove_style_all(listHost);
    UiKit::forceOpaqueBg(listHost, AppTheme::bg());
    lv_obj_set_flex_flow(listHost, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(listHost, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(listHost, 4, 0);
    lv_obj_set_style_pad_all(listHost, kPad, 0);
    lv_obj_add_flag(listHost, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(listHost, LV_DIR_VER);

    /* Panel de red elegida (lista oculta). */
    pickedHost = lv_obj_create(rootScr);
    lv_obj_remove_style_all(pickedHost);
    UiKit::forceOpaqueBg(pickedHost, AppTheme::surface());
    lv_obj_set_style_border_width(pickedHost, 1, 0);
    lv_obj_set_style_border_color(pickedHost, AppTheme::gridLine(), 0);
    lv_obj_set_style_pad_all(pickedHost, 16, 0);
    lv_obj_add_flag(pickedHost, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pickedHost, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pickedHint = lv_label_create(pickedHost);
    lv_label_set_text(pickedHint, Strings::tr(Msg::WifiRed));
    lv_obj_set_style_text_color(pickedHint, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(pickedHint, &lv_font_montserrat_12, 0);
    lv_obj_align(pickedHint, LV_ALIGN_TOP_MID, 0, 8);

    pickedNameLbl = lv_label_create(pickedHost);
    lv_label_set_text(pickedNameLbl, "--");
    lv_obj_set_style_text_color(pickedNameLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(pickedNameLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_width(pickedNameLbl, LCD_H_RES - 48);
    lv_label_set_long_mode(pickedNameLbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(pickedNameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pickedNameLbl, LV_ALIGN_CENTER, 0, 0);

    selLbl = lv_label_create(rootScr);
    lv_obj_set_style_text_color(selLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(selLbl, &lv_font_montserrat_12, 0);
    updateSelLabel();

    passTa = lv_textarea_create(rootScr);
    lv_obj_set_size(passTa, LCD_H_RES - 130, 36);
    lv_textarea_set_one_line(passTa, true);
    lv_textarea_set_password_mode(passTa, false);
    lv_textarea_set_placeholder_text(passTa, Strings::tr(Msg::WifiPassPh));
    lv_textarea_set_max_length(passTa, static_cast<uint32_t>(WifiConfig::PASS_MAX - 1));
    UiKit::forceOpaqueBg(passTa, AppTheme::surface());
    lv_obj_set_style_border_width(passTa, 1, 0);
    lv_obj_set_style_border_color(passTa, AppTheme::gridLine(), 0);
    lv_obj_set_style_text_color(passTa, AppTheme::text(), 0);
    lv_obj_set_style_pad_all(passTa, 6, 0);
    lv_obj_add_event_cb(passTa, onPassFocus, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(passTa, onPassFocus, LV_EVENT_CLICKED, nullptr);

    char savedSsid[WifiConfig::SSID_MAX];
    char savedPass[WifiConfig::PASS_MAX];
    WifiConfig::getSsid(savedSsid, sizeof(savedSsid));
    WifiConfig::getPass(savedPass, sizeof(savedPass));
    if (savedSsid[0] != '\0') {
        strncpy(selectedSsid, savedSsid, sizeof(selectedSsid) - 1);
        updateSelLabel();
        lv_textarea_set_text(passTa, savedPass);
        showPickedMode();
        setStatus(Strings::tr(Msg::WifiSavedNet), true);
    }

    saveBtn = UiKit::makePrimaryButton(rootScr, Strings::tr(Msg::WifiOk), onSave);
    lv_obj_set_size(saveBtn, 110, 36);

    kb = lv_keyboard_create(rootScr);
    lv_obj_set_size(kb, LCD_H_RES, kKbH);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb, onKbDone, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb, onKbDone, LV_EVENT_CANCEL, nullptr);
    UiKit::styleDarkKeyboard(kb);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    layoutBody();
    if (selectedSsid[0] == '\0') {
        rebuildList(nullptr, 0);
        onScan(nullptr);
    }
    return rootScr;
}

void Screens::refreshWifiSetup(lv_obj_t *root) {
    (void)root;
    if (scanning) {
        WifiConfig::Network nets[WifiConfig::SCAN_MAX];
        const int n = WifiConfig::pollScan(nets, WifiConfig::SCAN_MAX);
        if (n == -1) {
            return;
        }
        scanning = false;
        if (scanBtn) {
            lv_obj_clear_state(scanBtn, LV_STATE_DISABLED);
        }
        if (n < 0) {
            setStatus(Strings::tr(Msg::WifiScanRetry), false);
            rebuildList(nullptr, 0);
            return;
        }
        char msg[48];
        snprintf(msg, sizeof(msg), Strings::tr(Msg::WifiNetsFound), n);
        setStatus(msg, true);
        rebuildList(nets, n);
        return;
    }
    /* No pisar "escribe clave" mientras el usuario escribe, salvo connecting/IP. */
    if (!listHidden || WifiConfig::linkState() == 1 || WifiConfig::isConnected() ||
        WifiConfig::linkState() == 3) {
        paintLinkStatus();
    }
}
