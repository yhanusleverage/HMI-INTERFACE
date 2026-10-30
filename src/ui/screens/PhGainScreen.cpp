#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "AppStrings.h"
#include "Config.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

#include <Preferences.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

/**
 * Ganho químico pH+ / pH−: ml / |pH después − pH antes|.
 * El vaso o el balde no es el tanque del sistema.
 */

namespace {

enum class GainField : uint8_t { Before = 0, After, Ml };

DoseChannel ch_ = DoseChannel::R1;
GainField field_ = GainField::Before;
float phBefore_ = 6.0f;
float phAfter_ = 6.2f;
float ml_ = 1.0f;
float stored_ = 2.0f;

lv_obj_t *beforeLbl_ = nullptr;
lv_obj_t *afterLbl_ = nullptr;
lv_obj_t *mlLbl_ = nullptr;
lv_obj_t *resultLbl_ = nullptr;
lv_obj_t *storedLbl_ = nullptr;
lv_obj_t *statusLbl_ = nullptr;
lv_obj_t *editBar_ = nullptr;
lv_obj_t *editTitle_ = nullptr;
lv_obj_t *editTa_ = nullptr;
lv_obj_t *kb_ = nullptr;

constexpr float kMinDelta = 0.05f;
constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kEditBarH = 40;

const char *prefKey() {
    return NavShell::phPumpIsUp(ch_) ? "phg_up" : "phg_dn";
}

float loadStored() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, true)) {
        return 2.0f;
    }
    const float v = prefs.getFloat(prefKey(), 2.0f);
    prefs.end();
    return v > 0.01f ? v : 2.0f;
}

void saveStored(float gain) {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, false)) {
        return;
    }
    prefs.putFloat(prefKey(), gain);
    prefs.end();
    stored_ = gain;
}

void setStatus(UiKit::PumpUiStatus st, const char *msg) {
    UiKit::setPumpStatus(statusLbl_, st, msg);
}

void hideKb() {
    if (kb_) {
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    }
    if (editBar_) {
        lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    }
}

float *activeValue() {
    if (field_ == GainField::After) {
        return &phAfter_;
    }
    if (field_ == GainField::Ml) {
        return &ml_;
    }
    return &phBefore_;
}

void refreshValues() {
    char buf[40];
    if (beforeLbl_) {
        snprintf(buf, sizeof(buf), "%.2f", phBefore_);
        lv_label_set_text(beforeLbl_, buf);
    }
    if (afterLbl_) {
        snprintf(buf, sizeof(buf), "%.2f", phAfter_);
        lv_label_set_text(afterLbl_, buf);
    }
    if (mlLbl_) {
        snprintf(buf, sizeof(buf), "%.1f", ml_);
        lv_label_set_text(mlLbl_, buf);
    }
    if (storedLbl_) {
        snprintf(buf, sizeof(buf), Strings::tr(Msg::PhGainSavedFmt), stored_);
        lv_label_set_text(storedLbl_, buf);
    }
    if (!resultLbl_) {
        return;
    }
    const float d = fabsf(phAfter_ - phBefore_);
    if (d < kMinDelta || ml_ < 0.1f) {
        lv_label_set_text(resultLbl_, Strings::tr(Msg::PhGainNeedDelta));
        return;
    }
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PhGainResultFmt), ml_ / d);
    lv_label_set_text(resultLbl_, buf);
}

void showKb() {
    if (!kb_ || !editBar_ || !editTa_ || !editTitle_) {
        return;
    }
    const char *title = Strings::tr(Msg::PhGainBefore);
    const char *fmt = "%.2f";
    if (field_ == GainField::After) {
        title = Strings::tr(Msg::PhGainAfter);
    } else if (field_ == GainField::Ml) {
        title = Strings::tr(Msg::PhGainMl);
        fmt = "%.1f";
    }
    lv_label_set_text(editTitle_, title);
    char buf[16];
    snprintf(buf, sizeof(buf), fmt, *activeValue());
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
    if (field_ == GainField::Ml) {
        if (v < 0.1f) {
            v = 0.1f;
        }
        if (v > 500.0f) {
            v = 500.0f;
        }
        ml_ = static_cast<float>(static_cast<int>(v * 10.0f + 0.5f)) / 10.0f;
    } else {
        if (v < 0.0f) {
            v = 0.0f;
        }
        if (v > 14.0f) {
            v = 14.0f;
        }
        v = static_cast<float>(static_cast<int>(v * 100.0f + 0.5f)) / 100.0f;
        if (field_ == GainField::After) {
            phAfter_ = v;
        } else {
            phBefore_ = v;
        }
    }
    refreshValues();
}

void onKbDone(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        applyTa();
    }
    hideKb();
}

void onBefore(lv_event_t *) {
    field_ = GainField::Before;
    showKb();
}
void onAfter(lv_event_t *) {
    field_ = GainField::After;
    showKb();
}
void onMl(lv_event_t *) {
    field_ = GainField::Ml;
    showKb();
}

void onBack(lv_event_t *) {
    hideKb();
    NavShell::back();
}

void onSave(lv_event_t *) {
    hideKb();
    const float d = fabsf(phAfter_ - phBefore_);
    if (d < kMinDelta || ml_ < 0.1f) {
        setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::PhGainNeedDelta));
        refreshValues();
        return;
    }
    float gain = ml_ / d;
    gain = static_cast<float>(static_cast<int>(gain * 100.0f + 0.5f)) / 100.0f;
    if (gain < 0.01f) {
        setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::PhGainNeedDelta));
        return;
    }
    saveStored(gain);
    MasterLink::sendPhDeltaGain(!NavShell::phPumpIsUp(ch_), gain);
    refreshValues();
    char buf[48];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PhGainSavedFmt), gain);
    setStatus(UiKit::PumpUiStatus::Done, buf);
}

lv_obj_t *makeField(lv_obj_t *parent, lv_coord_t y, const char *title, lv_obj_t **valueOut,
                    lv_event_cb_t cb) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 16, AppTheme::TOUCH_MIN_H);
    lv_obj_set_pos(row, 8, y);
    UiKit::forceOpaqueBg(row, AppTheme::surface());
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, title);
    lv_obj_set_style_text_color(name, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *value = lv_label_create(row);
    lv_obj_set_style_text_color(value, AppTheme::text(), 0);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_16, 0);
    lv_obj_align(value, LV_ALIGN_RIGHT_MID, -10, 0);
    if (valueOut) {
        *valueOut = value;
    }
    return row;
}

}  // namespace

lv_obj_t *Screens::createPhGain(lv_obj_t *parent, DoseChannel channel) {
    ch_ = channel;
    field_ = GainField::Before;
    phBefore_ = 6.0f;
    phAfter_ = NavShell::phPumpIsUp(channel) ? 6.2f : 5.8f;
    ml_ = 1.0f;
    stored_ = loadStored();
    beforeLbl_ = afterLbl_ = mlLbl_ = resultLbl_ = storedLbl_ = statusLbl_ = nullptr;
    editBar_ = editTitle_ = editTa_ = kb_ = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::PhGainTitle), onBack);

    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    lv_obj_t *host = lv_obj_create(root);
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

    lv_obj_t *hint = lv_label_create(host);
    lv_label_set_text(hint, Strings::tr(Msg::PhGainHint));
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(hint, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(hint, 12, y);
    y += 36;

    storedLbl_ = lv_label_create(host);
    lv_obj_set_style_text_color(storedLbl_, AppTheme::text(), 0);
    lv_obj_set_style_text_font(storedLbl_, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(storedLbl_, 12, y);
    y += 26;

    makeField(host, y, Strings::tr(Msg::PhGainBefore), &beforeLbl_, onBefore);
    y += AppTheme::TOUCH_MIN_H + gap;
    makeField(host, y, Strings::tr(Msg::PhGainAfter), &afterLbl_, onAfter);
    y += AppTheme::TOUCH_MIN_H + gap;
    makeField(host, y, Strings::tr(Msg::PhGainMl), &mlLbl_, onMl);
    y += AppTheme::TOUCH_MIN_H + gap + 4;

    resultLbl_ = lv_label_create(host);
    lv_obj_set_width(resultLbl_, LCD_H_RES);
    lv_obj_set_style_text_align(resultLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(resultLbl_, AppTheme::text(), 0);
    lv_obj_set_style_text_font(resultLbl_, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(resultLbl_, 0, y);
    y += 28;

    lv_obj_t *save = UiKit::makePrimaryButton(host, Strings::tr(Msg::NutSave), onSave);
    lv_obj_set_size(save, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(save, 8, y);
    y += AppTheme::BTN_PRIMARY_H + gap;

    statusLbl_ = lv_label_create(host);
    lv_obj_set_width(statusLbl_, LCD_H_RES - 24);
    lv_label_set_long_mode(statusLbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(statusLbl_, 12, y);
    setStatus(UiKit::PumpUiStatus::Idle, Strings::tr(Msg::ReadyStatus));

    refreshValues();

    editBar_ = lv_obj_create(root);
    lv_obj_remove_style_all(editBar_);
    lv_obj_set_size(editBar_, LCD_H_RES, kEditBarH);
    lv_obj_align(editBar_, LV_ALIGN_BOTTOM_MID, 0, -kKbH);
    UiKit::forceOpaqueBg(editBar_, AppTheme::surface());
    lv_obj_set_style_border_width(editBar_, 1, 0);
    lv_obj_set_style_border_color(editBar_, AppTheme::gridLine(), 0);
    lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(editBar_, LV_OBJ_FLAG_SCROLLABLE);

    editTitle_ = lv_label_create(editBar_);
    lv_label_set_text(editTitle_, Strings::tr(Msg::PhGainBefore));
    lv_obj_set_style_text_color(editTitle_, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(editTitle_, &lv_font_montserrat_14, 0);
    lv_obj_align(editTitle_, LV_ALIGN_LEFT_MID, 8, 0);

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

    kb_ = lv_keyboard_create(root);
    lv_obj_set_size(kb_, LCD_H_RES, kKbH);
    lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    UiKit::attachNumericKeyboard(kb_, editTa_, onKbDone);

    return root;
}

void Screens::refreshPhGain(lv_obj_t *root) {
    (void)root;
    refreshValues();
}
