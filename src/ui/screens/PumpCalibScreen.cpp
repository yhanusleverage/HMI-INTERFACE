#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "PumpConfig.h"
#include "ReservoirConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstdlib>

/**
 * Calibración: hold 60 s (bomba ON) → usuario mide ml → teclado numérico → guardar caudal.
 */

namespace {

DoseChannel ch_ = DoseChannel::R1;
float measuredMl_ = 100.0f;
lv_obj_t *root_ = nullptr;
lv_obj_t *flowLbl = nullptr;
lv_obj_t *measLbl = nullptr;
lv_obj_t *statusLbl = nullptr;
lv_obj_t *editBar_ = nullptr;
lv_obj_t *editTa_ = nullptr;
lv_obj_t *kb_ = nullptr;
bool calibRunning_ = false;
unsigned long calibStartMs_ = 0;
constexpr float CALIB_SEC = 60.0f;
constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kEditBarH = 40;

void setStatus(UiKit::PumpUiStatus st, const char *msg) {
    UiKit::setPumpStatus(statusLbl, st, msg);
}

void refreshFlowLabel() {
    if (!flowLbl) {
        return;
    }
    char buf[40];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpCalibDoneFmt), PumpConfig::flowMlPerMin(ch_));
    lv_label_set_text(flowLbl, buf);
}

void refreshMeasLabel() {
    if (!measLbl) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f ml", measuredMl_);
    lv_label_set_text(measLbl, buf);
}

void hideKb() {
    if (kb_) {
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    }
    if (editBar_) {
        lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    }
}

void showKb() {
    if (calibRunning_ || !kb_ || !editBar_ || !editTa_) {
        return;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", measuredMl_);
    lv_textarea_set_text(editTa_, buf);
    lv_obj_clear_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(editBar_);
    lv_obj_move_foreground(kb_);
    UiKit::attachNumericKeyboard(kb_, editTa_, nullptr);
    lv_keyboard_set_textarea(kb_, editTa_);
}

void applyTa() {
    if (!editTa_) {
        return;
    }
    const char *txt = lv_textarea_get_text(editTa_);
    if (!txt || !txt[0]) {
        return;
    }
    float v = static_cast<float>(atof(txt));
    if (v < 1.0f) {
        v = 1.0f;
    }
    if (v > 500.0f) {
        v = 500.0f;
    }
    measuredMl_ = static_cast<float>(static_cast<int>(v * 10.0f + 0.5f)) / 10.0f;
    refreshMeasLabel();
}

void onKbDone(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        applyTa();
    }
    hideKb();
}

void onValueClick(lv_event_t *) { showKb(); }

void onBack(lv_event_t *) {
    hideKb();
    if (calibRunning_) {
        MasterLink::sendDoseHold(doseChannelKey(ch_), false);
        calibRunning_ = false;
    }
    NavShell::back();
}

void onMeasMinus(lv_event_t *) {
    measuredMl_ -= 1.0f;
    if (measuredMl_ < 1.0f) {
        measuredMl_ = 1.0f;
    }
    refreshMeasLabel();
}

void onMeasPlus(lv_event_t *) {
    measuredMl_ += 1.0f;
    if (measuredMl_ > 500.0f) {
        measuredMl_ = 500.0f;
    }
    refreshMeasLabel();
}

void onCalibRun(lv_event_t *) {
    if (calibRunning_) {
        return;
    }
    hideKb();
    if (!ReservoirConfig::manualDoseAllowed()) {
        setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::DoseBlockedArmed));
        return;
    }
    calibRunning_ = true;
    calibStartMs_ = millis();
    MasterLink::sendDoseHold(doseChannelKey(ch_), true);
    char buf[48];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpCalibRunning), static_cast<int>(CALIB_SEC));
    setStatus(UiKit::PumpUiStatus::Active, buf);
}

void onCalibApply(lv_event_t *) {
    if (calibRunning_) {
        return;
    }
    hideKb();
    PumpConfig::applyCalibMeasure(ch_, measuredMl_, CALIB_SEC);
    MasterLink::sendPumpFlowCalib(doseChannelKey(ch_), PumpConfig::flowMlPerMin(ch_), measuredMl_,
                                  CALIB_SEC);
    refreshFlowLabel();
    char buf[64];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpCalibDoneFmt), PumpConfig::flowMlPerMin(ch_));
    setStatus(UiKit::PumpUiStatus::Done, buf);
}

lv_obj_t *makePmBar(lv_obj_t *parent, lv_coord_t y, lv_event_cb_t onMinus, lv_event_cb_t onPlus) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 16, AppTheme::TOUCH_MIN_H);
    lv_obj_set_pos(row, 8, y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *minus = UiKit::makeSecondaryButton(row, "- 1", 100, AppTheme::TOUCH_MIN_H, onMinus);
    lv_obj_align(minus, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *plus = UiKit::makeSecondaryButton(row, "+ 1", 100, AppTheme::TOUCH_MIN_H, onPlus);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    return row;
}

}  // namespace

lv_obj_t *Screens::createPumpCalib(lv_obj_t *parent, DoseChannel channel) {
    ch_ = channel;
    measuredMl_ = 100.0f;
    calibRunning_ = false;
    flowLbl = measLbl = statusLbl = editBar_ = editTa_ = kb_ = nullptr;

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root_, Strings::tr(Msg::PumpActionCalib), onBack);

    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    lv_obj_t *host = lv_obj_create(root_);
    lv_obj_remove_style_all(host);
    lv_obj_set_size(host, LCD_H_RES, LCD_V_RES - top);
    lv_obj_align(host, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(host, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(host, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(host, 12, 0);

    lv_coord_t y = 4;
    const lv_coord_t gap = 6;

    flowLbl = lv_label_create(host);
    lv_obj_set_style_text_color(flowLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(flowLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_width(flowLbl, LCD_H_RES);
    lv_obj_set_style_text_align(flowLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(flowLbl, 0, y);
    refreshFlowLabel();
    y += 28;

    lv_obj_t *calibRun = UiKit::makePrimaryButton(host, Strings::tr(Msg::PumpCalibRun), onCalibRun);
    lv_obj_set_size(calibRun, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(calibRun, 8, y);
    y += AppTheme::BTN_PRIMARY_H + gap + 4;

    lv_obj_t *measTitle = lv_label_create(host);
    lv_label_set_text(measTitle, Strings::tr(Msg::PumpCalibMeasured));
    lv_obj_set_style_text_color(measTitle, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(measTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(measTitle, 12, y);
    y += 20;

    measLbl = lv_label_create(host);
    lv_obj_set_style_text_color(measLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(measLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_width(measLbl, LCD_H_RES);
    lv_obj_set_style_text_align(measLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(measLbl, 0, y);
    lv_obj_add_flag(measLbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(measLbl, onValueClick, LV_EVENT_CLICKED, nullptr);
    refreshMeasLabel();
    y += 34;

    makePmBar(host, y, onMeasMinus, onMeasPlus);
    y += AppTheme::TOUCH_MIN_H + gap;

    lv_obj_t *apply = UiKit::makePrimaryButton(host, Strings::tr(Msg::PumpCalibApply), onCalibApply);
    lv_obj_set_size(apply, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(apply, 8, y);
    y += AppTheme::BTN_PRIMARY_H + gap;

    statusLbl = lv_label_create(host);
    lv_obj_set_width(statusLbl, LCD_H_RES - 24);
    lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(statusLbl, 12, y);
    setStatus(UiKit::PumpUiStatus::Idle, Strings::tr(Msg::ReadyStatus));

    editBar_ = lv_obj_create(root_);
    lv_obj_remove_style_all(editBar_);
    lv_obj_set_size(editBar_, LCD_H_RES, kEditBarH);
    lv_obj_align(editBar_, LV_ALIGN_BOTTOM_MID, 0, -kKbH);
    UiKit::forceOpaqueBg(editBar_, AppTheme::surface());
    lv_obj_set_style_border_width(editBar_, 1, 0);
    lv_obj_set_style_border_color(editBar_, AppTheme::gridLine(), 0);
    lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(editBar_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *editTitle = lv_label_create(editBar_);
    lv_label_set_text(editTitle, "ml");
    lv_obj_set_style_text_color(editTitle, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(editTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(editTitle, LV_ALIGN_LEFT_MID, 8, 0);

    editTa_ = lv_textarea_create(editBar_);
    lv_textarea_set_one_line(editTa_, true);
    lv_textarea_set_accepted_chars(editTa_, "0123456789.");
    lv_textarea_set_max_length(editTa_, 8);
    lv_obj_set_size(editTa_, 160, 32);
    lv_obj_align(editTa_, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(editTa_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(editTa_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(editTa_, AppTheme::text(), 0);
    lv_obj_set_style_border_width(editTa_, 1, 0);
    lv_obj_set_style_border_color(editTa_, AppTheme::accent(), 0);
    lv_obj_set_style_radius(editTa_, 0, 0);
    lv_obj_set_style_pad_all(editTa_, 4, 0);

    kb_ = lv_keyboard_create(root_);
    lv_obj_set_size(kb_, LCD_H_RES, kKbH);
    lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    UiKit::attachNumericKeyboard(kb_, editTa_, onKbDone);

    return root_;
}

void Screens::refreshPumpCalib(lv_obj_t *root) {
    (void)root;
    if (!calibRunning_) {
        return;
    }
    const unsigned long elapsed = millis() - calibStartMs_;
    const unsigned long totalMs = static_cast<unsigned long>(CALIB_SEC * 1000.0f);
    if (elapsed >= totalMs) {
        MasterLink::sendDoseHold(doseChannelKey(ch_), false);
        calibRunning_ = false;
        setStatus(UiKit::PumpUiStatus::Done, Strings::tr(Msg::PumpCalibEnterMeasured));
        return;
    }
    const int left = static_cast<int>((totalMs - elapsed + 999) / 1000);
    char buf[48];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpCalibRunning), left);
    setStatus(UiKit::PumpUiStatus::Active, buf);
}
