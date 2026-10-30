#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "DataStore.h"
#include "UnitsConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>
#include <math.h>

namespace {

uint8_t kind_ = 0;
lv_obj_t *statusLbl_ = nullptr;
lv_obj_t *bodyLbl_ = nullptr;
lv_obj_t *btnOnline_ = nullptr;
lv_obj_t *btnOffline_ = nullptr;
lv_obj_t *refVal_ = nullptr;
lv_obj_t *nowVal_ = nullptr;
lv_obj_t *timeVal_ = nullptr;
lv_obj_t *lastVal_ = nullptr;

constexpr uint32_t kDaySec = 24u * 3600u;

void setText(lv_obj_t *lbl, const char *txt);
void paintWifiButtons();

void onBack(lv_event_t *) { NavShell::back(); }

void onWifiMode(lv_event_t *e) {
    if (!MasterLink::linkOk()) {
        setText(bodyLbl_, Strings::tr(Msg::ArmWifiNeedCable));
        return;
    }
    const bool offline = lv_event_get_user_data(e) != nullptr;
    MasterLink::sendWifiUserMode(offline);
    setText(statusLbl_, offline ? Strings::tr(Msg::ArmLinkOff) : Strings::tr(Msg::ArmLinkOn));
    if (statusLbl_) {
        lv_obj_set_style_text_color(statusLbl_, offline ? AppTheme::alarm() : AppTheme::accent(), 0);
    }
    paintWifiButtons();
}

void paintWifiButtons() {
    const bool known = MasterLink::wifiUserKnown();
    const bool offline = known && MasterLink::wifiUserOffline();
    const bool online = known && !MasterLink::wifiUserOffline();
    if (btnOnline_) {
        lv_obj_set_style_border_width(btnOnline_, online ? 3 : 1, 0);
        lv_obj_set_style_border_color(btnOnline_, online ? AppTheme::accent() : AppTheme::gridLine(), 0);
    }
    if (btnOffline_) {
        lv_obj_set_style_border_width(btnOffline_, offline ? 3 : 1, 0);
        lv_obj_set_style_border_color(btnOffline_, offline ? AppTheme::alarm() : AppTheme::gridLine(), 0);
    }
}

lv_obj_t *makeChoice(lv_obj_t *parent, lv_coord_t y, const char *text, void *user) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, LCD_H_RES - 32, 56);
    lv_obj_set_pos(btn, 16, y);
    lv_obj_set_style_bg_color(btn, AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, AppTheme::gridLine(), 0);
    lv_obj_add_event_cb(btn, onWifiMode, LV_EVENT_CLICKED, user);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, user ? AppTheme::alarm() : AppTheme::text(), 0);
    lv_obj_center(lbl);
    return btn;
}

void setText(lv_obj_t *lbl, const char *txt) {
    if (!lbl || !txt) {
        return;
    }
    const char *cur = lv_label_get_text(lbl);
    if (!cur || strcmp(cur, txt) != 0) {
        lv_label_set_text(lbl, txt);
    }
}

void formatSignedEc(float us, char *buf, size_t n) {
    float v = us;
    switch (UnitsConfig::ecUnit()) {
    case EcUnit::Ppm500:
        v *= 0.5f;
        break;
    case EcUnit::Ppm640:
        v *= 0.64f;
        break;
    default:
        break;
    }
    snprintf(buf, n, "%+.0f", static_cast<double>(v));
}

void formatWindow(const MasterLink::DayWindow &w, char *buf, size_t n) {
    if (!w.hasRef) {
        snprintf(buf, n, "--");
        return;
    }
    if (w.elapsedSec >= kDaySec) {
        snprintf(buf, n, "%s", Strings::tr(Msg::ArmDone));
        return;
    }
    const uint32_t eh = w.elapsedSec / 3600u;
    const uint32_t em = (w.elapsedSec % 3600u) / 60u;
    const uint32_t left = kDaySec - w.elapsedSec;
    const uint32_t lh = left / 3600u;
    const uint32_t lm = (left % 3600u) / 60u;
    char ran[48];
    char remain[48];
    snprintf(ran, sizeof(ran), Strings::tr(Msg::ArmElapsed), eh, em);
    snprintf(remain, sizeof(remain), Strings::tr(Msg::ArmLeft), lh, lm);
    snprintf(buf, n, "%s\n%s", ran, remain);
}

lv_obj_t *addRow(lv_obj_t *parent, lv_coord_t y, lv_coord_t h, const char *title, lv_obj_t **outVal) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES, h);
    lv_obj_set_pos(row, 0, y);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, title);
    lv_obj_set_style_text_color(name, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 16, 0);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, AppTheme::text(), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_set_width(val, LCD_H_RES / 2);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(val, LV_LABEL_LONG_WRAP);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, -16, 0);
    if (outVal) {
        *outVal = val;
    }
    return row;
}

}  // namespace

lv_obj_t *Screens::createArmSlot(lv_obj_t *parent) {
    kind_ = NavShell::armSlot();
    statusLbl_ = nullptr;
    bodyLbl_ = nullptr;
    btnOnline_ = nullptr;
    btnOffline_ = nullptr;
    refVal_ = nullptr;
    nowVal_ = nullptr;
    timeVal_ = nullptr;
    lastVal_ = nullptr;

    const char *title = Strings::tr(Msg::ArmLinkTitle);
    if (kind_ == 1) {
        title = Strings::tr(Msg::ArmEcTitle);
    } else if (kind_ == 2) {
        title = Strings::tr(Msg::ArmPhTitle);
    }

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    UiKit::styleHeader(root, title, onBack);

    if (kind_ == 0) {
        statusLbl_ = lv_label_create(root);
        lv_label_set_text(statusLbl_, "--");
        lv_obj_set_style_text_color(statusLbl_, AppTheme::text(), 0);
        lv_obj_set_style_text_font(statusLbl_, &lv_font_montserrat_28, 0);
        lv_obj_align(statusLbl_, LV_ALIGN_TOP_LEFT, 16, AppTheme::HEADER_H + 24);

        bodyLbl_ = lv_label_create(root);
        lv_label_set_text(bodyLbl_, Strings::tr(Msg::ArmWifiBody));
        lv_obj_set_style_text_color(bodyLbl_, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(bodyLbl_, &lv_font_montserrat_16, 0);
        lv_obj_set_width(bodyLbl_, LCD_H_RES - 32);
        lv_label_set_long_mode(bodyLbl_, LV_LABEL_LONG_WRAP);
        lv_obj_align(bodyLbl_, LV_ALIGN_TOP_LEFT, 16, AppTheme::HEADER_H + 64);

        btnOnline_ = makeChoice(root, AppTheme::HEADER_H + 130, Strings::tr(Msg::ArmLinkOn), nullptr);
        btnOffline_ = makeChoice(root, AppTheme::HEADER_H + 196, Strings::tr(Msg::ArmLinkOff),
                                 reinterpret_cast<void *>(1));
        return root;
    }

    const lv_coord_t y0 = AppTheme::HEADER_H + 8;
    addRow(root, y0, 48, Strings::tr(Msg::ArmRef), &refVal_);
    addRow(root, y0 + 48, 48, Strings::tr(Msg::ArmNow), &nowVal_);
    addRow(root, y0 + 96, 72, Strings::tr(Msg::ArmTime), &timeVal_);
    addRow(root, y0 + 168, 48, Strings::tr(Msg::ArmLast), &lastVal_);
    if (timeVal_) {
        lv_obj_set_style_text_font(timeVal_, &lv_font_montserrat_16, 0);
    }
    return root;
}

void Screens::refreshArmSlot(lv_obj_t *root) {
    (void)root;
    if (kind_ == 0) {
        if (MasterLink::wifiUserKnown()) {
            const bool offline = MasterLink::wifiUserOffline();
            setText(statusLbl_, offline ? Strings::tr(Msg::ArmLinkOff) : Strings::tr(Msg::ArmLinkOn));
            setText(bodyLbl_, Strings::tr(Msg::ArmWifiBody));
            if (statusLbl_) {
                lv_obj_set_style_text_color(statusLbl_, offline ? AppTheme::alarm() : AppTheme::accent(), 0);
            }
        } else if (!MasterLink::linkOk()) {
            setText(statusLbl_, "--");
            setText(bodyLbl_, Strings::tr(Msg::ArmWifiNeedCable));
            if (statusLbl_) {
                lv_obj_set_style_text_color(statusLbl_, AppTheme::text(), 0);
            }
        } else {
            setText(statusLbl_, "--");
            setText(bodyLbl_, Strings::tr(Msg::ArmWifiBody));
            if (statusLbl_) {
                lv_obj_set_style_text_color(statusLbl_, AppTheme::text(), 0);
            }
        }
        paintWifiButtons();
        return;
    }

    const bool ph = kind_ == 2;
    const MasterLink::DayWindow &w = ph ? MasterLink::consumoPh() : MasterLink::consumoEc();
    const TelemetrySnapshot snap = DataStore::instance().snapshot();
    char buf[96];

    if (w.hasRef) {
        if (ph) {
            snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(w.ref));
        } else {
            UnitsConfig::formatEc(w.ref, buf, sizeof(buf));
        }
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    setText(refVal_, buf);

    const float now = ph ? snap.ph : snap.ec;
    if (isfinite(now)) {
        if (ph) {
            snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(now));
        } else {
            UnitsConfig::formatEc(now, buf, sizeof(buf));
        }
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    setText(nowVal_, buf);

    formatWindow(w, buf, sizeof(buf));
    setText(timeVal_, buf);

    if (w.hasDeriv) {
        if (ph) {
            snprintf(buf, sizeof(buf), "%+.2f", static_cast<double>(w.deriv));
        } else {
            formatSignedEc(w.deriv, buf, sizeof(buf));
        }
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    setText(lastVal_, buf);
}
