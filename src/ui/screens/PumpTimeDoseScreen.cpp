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
 * Time Dose: solo minutos (± y teclado) · Start / Stop.
 * Hold UART hasta cumplirse el tiempo; ml estimado = caudal × tiempo.
 */

namespace {

DoseChannel ch_ = DoseChannel::R1;
float minutes_ = 5.0f;
lv_obj_t *root_ = nullptr;
lv_obj_t *minLbl = nullptr;
lv_obj_t *statusLbl = nullptr;
lv_obj_t *editBar_ = nullptr;
lv_obj_t *editTitle_ = nullptr;
lv_obj_t *editTa_ = nullptr;
lv_obj_t *kb_ = nullptr;
bool running_ = false;
unsigned long startMs_ = 0;
float dispensedEst_ = 0.0f;

constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kEditBarH = 40;

void setStatus(UiKit::PumpUiStatus st, const char *msg) {
    UiKit::setPumpStatus(statusLbl, st, msg);
}

void refreshMin() {
    if (!minLbl) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f min", minutes_);
    lv_label_set_text(minLbl, buf);
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
    if (running_ || !kb_ || !editBar_ || !editTa_) {
        return;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", minutes_);
    if (editTitle_) {
        lv_label_set_text(editTitle_, "min");
    }
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
    if (v > 240.0f) {
        v = 240.0f;
    }
    minutes_ = static_cast<float>(static_cast<int>(v + 0.5f));
    refreshMin();
}

void onKbDone(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        applyTa();
    }
    hideKb();
}

void onMinClick(lv_event_t *) { showKb(); }

void stopRun(bool userStop) {
    if (!running_) {
        if (userStop) {
            MasterLink::sendDoseStop(doseChannelKey(ch_));
            setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::StopSent));
        }
        return;
    }
    MasterLink::sendDoseHold(doseChannelKey(ch_), false);
    running_ = false;
    if (dispensedEst_ > 0.05f) {
        PumpConfig::addDispensed(ch_, dispensedEst_);
    }
    if (userStop) {
        setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::StopSent));
    } else {
        char buf[48];
        snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpTimeDoneFmt), minutes_);
        setStatus(UiKit::PumpUiStatus::Done, buf);
    }
    dispensedEst_ = 0.0f;
}

void onBack(lv_event_t *) {
    hideKb();
    stopRun(false);
    NavShell::back();
}

void onMinMinus(lv_event_t *) {
    if (running_) {
        return;
    }
    minutes_ -= 1.0f;
    if (minutes_ < 1.0f) {
        minutes_ = 1.0f;
    }
    refreshMin();
}

void onMinPlus(lv_event_t *) {
    if (running_) {
        return;
    }
    minutes_ += 1.0f;
    if (minutes_ > 240.0f) {
        minutes_ = 240.0f;
    }
    refreshMin();
}

void onStart(lv_event_t *) {
    if (running_) {
        return;
    }
    hideKb();
    if (!ReservoirConfig::manualDoseAllowed()) {
        setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::DoseBlockedArmed));
        return;
    }
    running_ = true;
    startMs_ = millis();
    dispensedEst_ = 0.0f;
    MasterLink::sendDoseHold(doseChannelKey(ch_), true);
    char buf[64];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpTimeRunning), 0.0f, minutes_);
    setStatus(UiKit::PumpUiStatus::Active, buf);
}

void onStop(lv_event_t *) {
    hideKb();
    stopRun(true);
}

lv_obj_t *makePmBar(lv_obj_t *parent, lv_coord_t y, const char *m, const char *p, lv_event_cb_t onM,
                    lv_event_cb_t onP) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 16, AppTheme::TOUCH_MIN_H);
    lv_obj_set_pos(row, 8, y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *minus = UiKit::makeSecondaryButton(row, m, 100, AppTheme::TOUCH_MIN_H, onM);
    lv_obj_align(minus, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *plus = UiKit::makeSecondaryButton(row, p, 100, AppTheme::TOUCH_MIN_H, onP);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    return row;
}

}  // namespace

lv_obj_t *Screens::createPumpTimeDose(lv_obj_t *parent, DoseChannel channel) {
    ch_ = channel;
    minutes_ = 5.0f;
    running_ = false;
    dispensedEst_ = 0.0f;
    minLbl = statusLbl = editBar_ = editTitle_ = editTa_ = kb_ = nullptr;

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root_, Strings::tr(Msg::PumpActionTime), onBack);

    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    lv_obj_t *host = lv_obj_create(root_);
    lv_obj_remove_style_all(host);
    lv_obj_set_size(host, LCD_H_RES, LCD_V_RES - top);
    lv_obj_align(host, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(host, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(host, 12, 0);

    lv_coord_t y = 4;
    const lv_coord_t gap = 6;

    lv_obj_t *minTitle = lv_label_create(host);
    lv_label_set_text(minTitle, Strings::tr(Msg::PumpTimeMin));
    lv_obj_set_style_text_color(minTitle, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(minTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(minTitle, 12, y);
    y += 20;

    minLbl = lv_label_create(host);
    lv_obj_set_style_text_color(minLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(minLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_width(minLbl, LCD_H_RES);
    lv_obj_set_style_text_align(minLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(minLbl, 0, y);
    lv_obj_add_flag(minLbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(minLbl, onMinClick, LV_EVENT_CLICKED, nullptr);
    refreshMin();
    y += 34;

    makePmBar(host, y, "- 1", "+ 1", onMinMinus, onMinPlus);
    y += AppTheme::TOUCH_MIN_H + gap;

    lv_obj_t *start = UiKit::makePrimaryButton(host, Strings::tr(Msg::PumpTimeStart), onStart);
    lv_obj_set_size(start, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(start, 8, y);
    y += AppTheme::BTN_PRIMARY_H + gap;

    lv_obj_t *stop = UiKit::makeCautionButton(host, Strings::tr(Msg::StopBtn), onStop);
    lv_obj_set_size(stop, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(stop, 8, y);
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

    editTitle_ = lv_label_create(editBar_);
    lv_label_set_text(editTitle_, "min");
    lv_obj_set_style_text_color(editTitle_, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(editTitle_, &lv_font_montserrat_14, 0);
    lv_obj_align(editTitle_, LV_ALIGN_LEFT_MID, 8, 0);

    editTa_ = lv_textarea_create(editBar_);
    lv_textarea_set_one_line(editTa_, true);
    lv_textarea_set_accepted_chars(editTa_, "0123456789");
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

void Screens::refreshPumpTimeDose(lv_obj_t *root) {
    (void)root;
    if (!running_) {
        return;
    }
    const float flow = PumpConfig::flowMlPerMin(ch_);
    const float elapsedMin = (millis() - startMs_) / 60000.0f;
    dispensedEst_ = flow * elapsedMin;
    const float windowMin = minutes_;

    char buf[64];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpTimeRunning), elapsedMin, windowMin);
    setStatus(UiKit::PumpUiStatus::Active, buf);

    if (elapsedMin >= windowMin) {
        stopRun(false);
    }
}
