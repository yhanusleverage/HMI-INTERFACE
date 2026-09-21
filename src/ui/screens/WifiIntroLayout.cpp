#include "ui/WifiIntroLayout.h"
#include "WifiConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

/**
 * Layout WiFi: wizard 4/5 + Ajuste (settingsMode).
 * Solo SSID + clave; perfil cloud = wizard 5/5.
 */

namespace {

constexpr lv_coord_t kHeaderWizard = 36;
constexpr lv_coord_t kHeaderSettings = AppTheme::BACK_H + 4;
constexpr lv_coord_t kStatusH = 22;
constexpr lv_coord_t kFooterH = 72;
constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kPad = 6;

lv_obj_t *rootScr = nullptr;
lv_obj_t *listHost = nullptr;
lv_obj_t *pickedHost = nullptr;
lv_obj_t *pickedNameLbl = nullptr;
lv_obj_t *statusLbl = nullptr;
lv_obj_t *selLbl = nullptr;
lv_obj_t *passTa = nullptr;
lv_obj_t *kb = nullptr;
lv_obj_t *scanBtn = nullptr;
WifiIntroCallbacks cbs = {};
char selectedSsid[WifiConfig::SSID_MAX] = {};
bool scanning = false;
bool listHidden = false;
bool settingsMode_ = false;
lv_coord_t headerH_ = kHeaderWizard;

void hideKb();
void layoutBody();
void rebuildList(const WifiConfig::Network *nets, int n);
void setStatus(const char *msg, bool ok);
void showListMode();
void showPickedMode();
void clearListChildren();

lv_coord_t bodyTop() { return headerH_ + kStatusH; }

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
    const lv_coord_t top = bodyTop();
    const lv_coord_t bodyH = LCD_V_RES - top - bottomReserve;

    if (listHost) {
        lv_obj_set_size(listHost, LCD_H_RES, bodyH > 40 ? bodyH : 40);
        lv_obj_set_pos(listHost, 0, top);
    }
    if (pickedHost) {
        lv_obj_set_size(pickedHost, LCD_H_RES - 2 * kPad, bodyH > 60 ? bodyH : 60);
        lv_obj_set_pos(pickedHost, kPad, top);
    }

    if (selLbl) {
        lv_obj_align(selLbl, LV_ALIGN_BOTTOM_LEFT, kPad, kbVisible ? -(kKbH + 48) : -48);
    }
    if (passTa) {
        lv_obj_align(passTa, LV_ALIGN_BOTTOM_LEFT, kPad, kbVisible ? -(kKbH + 4) : -4);
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

void onContinueHdr(lv_event_t *) {
    hideKb();
    if (selectedSsid[0] == '\0') {
        setStatus(Strings::tr(Msg::WifiSelectFirst), false);
        return;
    }
    const char *pass = passTa ? lv_textarea_get_text(passTa) : "";
    if (cbs.onContinue) {
        cbs.onContinue(selectedSsid, pass ? pass : "");
    }
}

void onSkipHdr(lv_event_t *) {
    hideKb();
    if (cbs.onSkip) {
        cbs.onSkip();
    }
}

void onBackHdr(lv_event_t *) {
    hideKb();
    if (cbs.onBack) {
        cbs.onBack();
    } else if (cbs.onSkip) {
        cbs.onSkip();
    }
}

void onPassFocus(lv_event_t *) { showKb(); }

void onKbDone(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        onContinueHdr(e);
        return;
    }
    hideKb();
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

void startScanInternal() {
    hideKb();
    if (scanning) {
        return;
    }
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

void onScan(lv_event_t *) { startScanInternal(); }

}  // namespace

namespace WifiIntroLayout {

lv_obj_t *create(lv_obj_t *parent, const WifiIntroConfig &cfg) {
    cbs = cfg.callbacks;
    settingsMode_ = cfg.settingsMode;
    headerH_ = settingsMode_ ? kHeaderSettings : kHeaderWizard;
    selectedSsid[0] = '\0';
    scanning = false;
    listHidden = false;
    rootScr = nullptr;
    listHost = nullptr;
    pickedHost = nullptr;
    pickedNameLbl = nullptr;
    statusLbl = nullptr;
    selLbl = nullptr;
    passTa = nullptr;
    kb = nullptr;
    scanBtn = nullptr;

    WifiConfig::begin();

    rootScr = lv_obj_create(parent);
    UiKit::styleScreen(rootScr);
    lv_obj_clear_flag(rootScr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr = lv_obj_create(rootScr);
    lv_obj_remove_style_all(hdr);
    lv_obj_set_size(hdr, LCD_H_RES, headerH_);
    lv_obj_set_pos(hdr, 0, 0);
    UiKit::forceOpaqueBg(hdr, AppTheme::bg());

    if (settingsMode_) {
        UiKit::makeBackButton(hdr, onBackHdr);
        lv_obj_t *titleLbl = lv_label_create(hdr);
        lv_label_set_text(titleLbl, Strings::tr(Msg::Wifi));
        UiKit::styleTitle(titleLbl);
        lv_obj_align(titleLbl, LV_ALIGN_LEFT_MID, AppTheme::BACK_W + 4, 0);

        scanBtn = UiKit::makeSecondaryButton(hdr, Strings::tr(Msg::WifiScan), 90, AppTheme::BACK_H,
                                             onScan);
        lv_obj_t *cont =
            UiKit::makePrimaryButton(hdr, Strings::tr(Msg::Continue), onContinueHdr);
        lv_obj_set_size(cont, 100, AppTheme::BACK_H);
        lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -kPad, 0);
        lv_obj_align(scanBtn, LV_ALIGN_RIGHT_MID, -108, 0);
    } else {
        lv_obj_t *titleLbl = lv_label_create(hdr);
        lv_label_set_text(titleLbl, Strings::tr(Msg::Wifi));
        UiKit::styleTitle(titleLbl);
        lv_obj_align(titleLbl, LV_ALIGN_LEFT_MID, kPad, 0);

        if (cfg.stepTotal > 0) {
            lv_obj_t *stepLbl = lv_label_create(hdr);
            char sbuf[24];
            snprintf(sbuf, sizeof(sbuf), Strings::tr(Msg::WizardStep), cfg.stepNum, cfg.stepTotal);
            lv_label_set_text(stepLbl, sbuf);
            lv_obj_set_style_text_color(stepLbl, AppTheme::text(), 0);
            lv_obj_set_style_text_font(stepLbl, &lv_font_montserrat_14, 0);
            lv_obj_align_to(stepLbl, titleLbl, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        }

        scanBtn = UiKit::makeSecondaryButton(hdr, Strings::tr(Msg::WifiScan), 90, 32, onScan);
        lv_obj_t *cont = UiKit::makePrimaryButton(hdr, Strings::tr(Msg::Continue), onContinueHdr);
        lv_obj_set_size(cont, 100, 32);
        lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -kPad, 0);
        lv_obj_t *sk = UiKit::makeSecondaryButton(hdr, Strings::tr(Msg::Skip), 70, 32, onSkipHdr);
        lv_obj_align(sk, LV_ALIGN_RIGHT_MID, -108, 0);
        lv_obj_align(scanBtn, LV_ALIGN_RIGHT_MID, -186, 0);
    }

    lv_obj_t *statusBar = lv_obj_create(rootScr);
    lv_obj_remove_style_all(statusBar);
    lv_obj_set_size(statusBar, LCD_H_RES, kStatusH);
    lv_obj_set_pos(statusBar, 0, headerH_);
    UiKit::forceOpaqueBg(statusBar, AppTheme::surfaceAlt());

    statusLbl = lv_label_create(statusBar);
    lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_width(statusLbl, LCD_H_RES - 2 * kPad);
    lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(statusLbl, LV_ALIGN_LEFT_MID, kPad, 0);
    setStatus(Strings::tr(settingsMode_ ? Msg::MasterWifiHint : Msg::WizardWifiHint), false);

    listHost = lv_obj_create(rootScr);
    lv_obj_remove_style_all(listHost);
    UiKit::forceOpaqueBg(listHost, AppTheme::bg());
    lv_obj_set_flex_flow(listHost, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(listHost, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(listHost, 4, 0);
    lv_obj_set_style_pad_all(listHost, kPad, 0);
    lv_obj_add_flag(listHost, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(listHost, LV_DIR_VER);

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
    lv_obj_set_style_text_font(pickedNameLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_width(pickedNameLbl, LCD_H_RES - 48);
    lv_label_set_long_mode(pickedNameLbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(pickedNameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pickedNameLbl, LV_ALIGN_CENTER, 0, 0);

    selLbl = lv_label_create(rootScr);
    lv_obj_set_style_text_color(selLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(selLbl, &lv_font_montserrat_12, 0);
    updateSelLabel();

    passTa = lv_textarea_create(rootScr);
    lv_obj_set_size(passTa, LCD_H_RES - 24, AppTheme::TOUCH_MIN_H);
    lv_textarea_set_one_line(passTa, true);
    lv_textarea_set_password_mode(passTa, false);
    lv_textarea_set_placeholder_text(passTa, Strings::tr(Msg::WifiPassPh));
    lv_textarea_set_max_length(passTa, static_cast<uint32_t>(WifiConfig::PASS_MAX - 1));
    UiKit::forceOpaqueBg(passTa, AppTheme::surface());
    lv_obj_set_style_border_width(passTa, 1, 0);
    lv_obj_set_style_border_color(passTa, AppTheme::gridLine(), 0);
    lv_obj_set_style_text_color(passTa, AppTheme::text(), 0);
    lv_obj_set_style_text_font(passTa, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_all(passTa, 6, 0);
    lv_obj_add_event_cb(passTa, onPassFocus, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(passTa, onPassFocus, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_state(passTa, LV_STATE_DISABLED);

    if (cfg.initialSsid && cfg.initialSsid[0] != '\0') {
        strncpy(selectedSsid, cfg.initialSsid, sizeof(selectedSsid) - 1);
        selectedSsid[sizeof(selectedSsid) - 1] = '\0';
        updateSelLabel();
        if (cfg.initialPass) {
            lv_textarea_set_text(passTa, cfg.initialPass);
        }
        lv_obj_clear_state(passTa, LV_STATE_DISABLED);
        showPickedMode();
        setStatus(Strings::tr(Msg::WifiWritePass), true);
    }

    kb = lv_keyboard_create(rootScr);
    UiKit::styleDarkKeyboard(kb);
    lv_obj_add_event_cb(kb, onKbDone, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb, onKbDone, LV_EVENT_CANCEL, nullptr);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    layoutBody();
    if (selectedSsid[0] == '\0') {
        rebuildList(nullptr, 0);
        startScanInternal();
    }
    return rootScr;
}

void refresh(lv_obj_t *root) {
    (void)root;
    if (!scanning) {
        return;
    }
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
}

void applyPrefill(const char *ssid, const char *pass) {
    if (!rootScr || !ssid || !ssid[0]) {
        return;
    }
    if (selectedSsid[0] != '\0') {
        return;
    }
    strncpy(selectedSsid, ssid, sizeof(selectedSsid) - 1);
    selectedSsid[sizeof(selectedSsid) - 1] = '\0';
    updateSelLabel();
    if (passTa) {
        if (pass) {
            lv_textarea_set_text(passTa, pass);
        }
        lv_obj_clear_state(passTa, LV_STATE_DISABLED);
    }
    showPickedMode();
    setStatus(Strings::tr(Msg::WifiWritePass), true);
}

}  // namespace WifiIntroLayout
