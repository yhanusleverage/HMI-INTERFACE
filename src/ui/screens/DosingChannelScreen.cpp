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
 * Quick Dose: ml ± · teclado numérico · Dose · Stop.
 */

namespace {

DoseChannel ch_ = DoseChannel::R1;
float doseMl_ = 5.0f;
lv_obj_t *root_ = nullptr;
lv_obj_t *mlLbl = nullptr;
lv_obj_t *statusLbl = nullptr;
lv_obj_t *editBar_ = nullptr;
lv_obj_t *editTa_ = nullptr;
lv_obj_t *kb_ = nullptr;

constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kEditBarH = 40;

void setStatus(UiKit::PumpUiStatus st, const char *msg) {
    UiKit::setPumpStatus(statusLbl, st, msg);
}

void refreshMlLabel() {
    if (!mlLbl) {
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f ml", doseMl_);
    lv_label_set_text(mlLbl, buf);
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
    if (!kb_ || !editBar_ || !editTa_) {
        return;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", doseMl_);
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
    if (v < 0.5f) {
        v = 0.5f;
    }
    if (v > 50.0f) {
        v = 50.0f;
    }
    doseMl_ = static_cast<float>(static_cast<int>(v * 10.0f + 0.5f)) / 10.0f;
    refreshMlLabel();
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
    MasterLink::sendDoseStop(doseChannelKey(ch_));
    NavShell::back();
}

void onMinusDose(lv_event_t *) {
    doseMl_ -= 0.5f;
    if (doseMl_ < 0.5f) {
        doseMl_ = 0.5f;
    }
    refreshMlLabel();
}

void onPlusDose(lv_event_t *) {
    doseMl_ += 0.5f;
    if (doseMl_ > 50.0f) {
        doseMl_ = 50.0f;
    }
    refreshMlLabel();
}

void onDose(lv_event_t *) {
    hideKb();
    if (!ReservoirConfig::manualDoseAllowed()) {
        setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::DoseBlockedArmed));
        return;
    }
    MasterLink::sendDose(doseChannelKey(ch_), doseMl_);
    PumpConfig::addDispensed(ch_, doseMl_);
    char buf[48];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::DoseSentFmt), doseMl_);
    setStatus(UiKit::PumpUiStatus::Done, buf);
}

void onStop(lv_event_t *) {
    hideKb();
    MasterLink::sendDoseStop(doseChannelKey(ch_));
    setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::StopSent));
}

lv_obj_t *makePmBar(lv_obj_t *parent, lv_coord_t y, lv_event_cb_t onMinus, lv_event_cb_t onPlus) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 16, AppTheme::TOUCH_MIN_H);
    lv_obj_set_pos(row, 8, y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *minus = UiKit::makeSecondaryButton(row, "- 0.5", 100, AppTheme::TOUCH_MIN_H, onMinus);
    lv_obj_align(minus, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *plus = UiKit::makeSecondaryButton(row, "+ 0.5", 100, AppTheme::TOUCH_MIN_H, onPlus);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, 0, 0);
    return row;
}

}  // namespace

lv_obj_t *Screens::createDosingChannel(lv_obj_t *parent, DoseChannel channel) {
    ch_ = channel;
    doseMl_ = 5.0f;
    mlLbl = statusLbl = editBar_ = editTa_ = kb_ = nullptr;

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root_, Strings::tr(Msg::PumpActionQuick), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP + 4;
    const lv_coord_t gap = 8;

    char name[40];
    PumpConfig::formatTitle(channel, name, sizeof(name));
    lv_obj_t *sub = lv_label_create(root_);
    lv_label_set_text(sub, name);
    lv_obj_set_style_text_color(sub, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(sub, 12, y);
    y += 22;

    lv_obj_t *doseTitle = lv_label_create(root_);
    lv_label_set_text(doseTitle, Strings::tr(Msg::PumpManualDose));
    lv_obj_set_style_text_color(doseTitle, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(doseTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(doseTitle, 12, y);
    y += 20;

    mlLbl = lv_label_create(root_);
    lv_obj_set_style_text_color(mlLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(mlLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(mlLbl, 0, y);
    lv_obj_set_width(mlLbl, LCD_H_RES);
    lv_obj_set_style_text_align(mlLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(mlLbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mlLbl, onValueClick, LV_EVENT_CLICKED, nullptr);
    refreshMlLabel();
    y += 36;

    makePmBar(root_, y, onMinusDose, onPlusDose);
    y += AppTheme::TOUCH_MIN_H + gap;

    lv_obj_t *dose = UiKit::makePrimaryButton(root_, Strings::tr(Msg::DoseBtn), onDose);
    lv_obj_set_size(dose, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(dose, 8, y);
    y += AppTheme::BTN_PRIMARY_H + gap;

    lv_obj_t *stop = UiKit::makeCautionButton(root_, Strings::tr(Msg::StopBtn), onStop);
    lv_obj_set_size(stop, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(stop, 8, y);
    y += AppTheme::BTN_PRIMARY_H + gap;

    statusLbl = lv_label_create(root_);
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

void Screens::refreshDosingChannel(lv_obj_t *root) { (void)root; }
